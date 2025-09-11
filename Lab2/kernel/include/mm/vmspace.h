/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS),
 * Shanghai Jiao Tong University (SJTU) Licensed under the Mulan PSL v2. You can
 * use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */

#ifndef MM_VMSPACE_H
#define MM_VMSPACE_H

#include <common/list.h>
#include <common/lock.h>
#include <common/rbtree.h>
#include <machine.h>
#include <object/memory.h>

/* This struct represents one virtual memory region inside on address space */
struct vmregion {
        struct list_head list_node; /* As one node of the vmr_list of a vmspace, 用于按顺序遍历所有vmregion */
        struct rb_node tree_node; /* As one node of the vmr_tree, 用于快速按地址查找vmregion */
        /* As one node of the pmo's mapping_list, 用于跟踪所有映射到此PMO的vmregion */
        struct list_head mapping_list_node;

        struct vmspace *vmspace; // 此vmregion所属的vmspace
        vaddr_t start; // 此vmregion的起始虚拟地址
        size_t size; // 此区域的大小（字节数）
        /* Offset of underlying pmo 在对应PMO（物理内存对象）中的偏移量 */
        size_t offset;
        vmr_prop_t perm; // 访问权限标志（如：可读、可写、可执行等）
        struct pmobject *pmo; // 该vmregion对应的物理内存对象
        struct list_head cow_private_pages; // Copy on Write机制的私有页链表
};

/* This struct represents one virtual address space */
struct vmspace {
        /* List head of vmregion (vmr_list) */
        struct list_head vmr_list; // vmregion组成的链表，用于顺序遍历
        /* rbtree root node of vmregion (vmr_tree) */
        struct rb_root vmr_tree; // vmregion组成的红黑树，用于按地址快速查找

        /* Root page table */
        void *pgtbl;
        /* Address space ID for avoiding TLB conflicts */
        unsigned long pcid; // 进程上下文id，用于避免TLB冲突

        /* The lock for manipulating vmregions 上锁保护vmregion的增删改操作 */
        struct lock vmspace_lock;
        /* The lock for manipulating the page table 上锁保护页表操作 */
        struct lock pgtbl_lock;

        /*
         * For TLB flushing:
         * Record the all the CPU that a vmspace ran on.
         * 记录此vmspace曾在哪些CPU上运行过，这些CPU需要刷新TLB
         */
        unsigned char history_cpus[PLAT_CPU_NUM];

        struct vmregion *heap_boundary_vmr; // 堆区域的边界vmregion，用于堆的动态扩展

        /* Records size of memory mapped. Protected by pgtbl_lock. */
        long rss;
};

/* Interfaces on vmspace management */
int vmspace_init(struct vmspace *vmspace, unsigned long pcid);
void vmspace_deinit(void *ptr);
void plat_vmspace_init(struct vmspace *vmspace);
int vmspace_map_range(struct vmspace *vmspace, vaddr_t va, size_t len,
                      vmr_prop_t flags, struct pmobject *pmo);
int vmspace_unmap_range(struct vmspace *vmspace, vaddr_t va, size_t len);
int vmspace_unmap_pmo(struct vmspace *vmspace, vaddr_t va, size_t len,
                      struct pmobject *pmo);
struct vmregion *find_vmr_for_va(struct vmspace *vmspace, vaddr_t addr);
int trans_uva_to_kva(vaddr_t user_va, vaddr_t *kernel_va);

/* Interfaces on spliting vmr */
int split_vmr_locked(struct vmspace *vmspace, struct vmregion *old_vmr,
              vaddr_t split_vaddr);

/* Two interfaces on heap management */
struct vmregion *init_heap_vmr(struct vmspace *vmspace, vaddr_t va,
                               struct pmobject *pmo);
int adjust_heap_vmr(struct vmspace *vmspace, unsigned long len);

/* Print all the vmrs inside one vmspace */
void kprint_vmr(struct vmspace *vmspace);

/* For TLB maintenence */
void record_history_cpu(struct vmspace *vmspcae, unsigned int cpuid);
void clear_history_cpu(struct vmspace *vmspcae, unsigned int cpuid);

/* The following functions' implementation is arch-dependent */
void switch_vmspace_to(struct vmspace *);
void arch_vmspace_init(struct vmspace *);
void free_page_table(void *);
struct vmspace *create_idle_vmspace(void);

/* Interfaces on CoW implementation */
int vmregion_record_cow_private_page(struct vmregion *vmr, vaddr_t vaddr,
                                     void *private_page);

#endif /* MM_VMSPACE_H */
