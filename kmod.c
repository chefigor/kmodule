#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#define NETLINK_USER 31

struct sock *nl_sk=NULL;
struct node{
    struct task_struct * info;
    struct node * next;
};
struct node* headptr=0;
void stack_push(struct task_struct* a){
    struct node* tmp=(struct node*)vmalloc(sizeof(struct node));
    tmp->info=a;
    tmp->next=headptr;
    headptr=tmp;
}

struct task_struct* stack_pop(void){
    struct node* tmp=headptr;
    headptr=headptr->next;
    struct task_struct *ret =tmp->info;
    vfree(tmp);
    return ret;
}

int stack_empty(void){
    return headptr==0;
}

static void hello_nl_recv_msg(struct sk_buff *skb)
{

    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int msg_size;
    char *msg="Hello from kernel";
    int res;

    printk(KERN_INFO "Entering: %s\n",__FUNCTION__);

    msg_size=strlen(msg);

    nlh=(struct nlmsghdr *)skb->data;
    printk(KERN_INFO "Netlink received msg payload:%s\n",(char *)nlmsg_data(nlh));
    pid=nlh->nlmsg_pid;

    skb_out=nlmsg_new(msg_size,0);
    if(!skb_out){
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }

    nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,msg_size,0);
    NETLINK_CB(skb_out).dst_group=0;
    strncpy(nlmsg_data(nlh),msg,msg_size);

    struct task_struct* task;
    for_each_process(task){
        if(task->pid==pid){
            stack_push(task);
            while(!stack_empty()){
                struct task_struct * tmp=stack_pop();
                   printk("%d  %s", tmp->pid, tmp->comm);
                struct task_struct * tmp1;
                struct list_head * list;
                list_for_each(list, &tmp->children){
                    tmp1 = list_entry(list, struct task_struct, sibling);
                    stack_push(tmp1);
                }
            }
            break;
        }
    }
    res=nlmsg_unicast(nl_sk,skb_out,pid);
    if(res<0)
        printk(KERN_INFO "Error while sending back to user\n");
}

static int __init hello_init(void)
{
    printk("Entering: %s\n",__FUNCTION__);
    struct netlink_kernel_cfg cfg={
        .input=hello_nl_recv_msg,
    };

    nl_sk=netlink_kernel_create(&init_net,NETLINK_USER,&cfg);
    if(!nl_sk)
    {
        printk(KERN_ALERT "Error creating socket\n");
        return -10;
    }

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "exiting hello module\n");
    netlink_kernel_release(nl_sk);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");