#ifndef CROWD_SAFE_GENCODE_H
#define CROWD_SAFE_GENCODE_H 1

#include "dr_api.h"
//#include "../../core/globals.h"
//#include "../../core/x86/arch.h"

#define IBL_SETUP_BYTE_COUNT 0x16

/* Called from DR to indicate that its internal phase of generating code stubs
 * is now starting. */
void
notify_gencode_starting();

void
init_crowd_safe_gencode();

/* Called from several places in DR as it generates a basic block for insertion
 * into the code cache. Each invocation occurs just before DR inserts a branch to
 * a gencode block that appears to be the indirect branch lookup stub. If it is
 * in fact some other stub, this function will detect that and return without doing
 * anything. If it is an IBL stub, this function inserts a pair of instructions:
 *     mov %temp1,<TLS_XDX_SLOT>
 *     mov <BB_TAG>, %temp1
 * This saves the contents of %temp1 to TLS and then puts the tag of the current
 * BB into %temp1, so that during IBL we will know which BB we just came from. */
uint
insert_indirect_link_branchpoint(dcontext_t *dcontext, instrlist_t *bb, app_pc bb_tag,
    instr_t *ibl_instr, bool is_return, int syscall_number);

bool
is_ibl_setup_instr(instr_t *instr);

/* Called from generation of the IBL stub at the point the stub discovers that the
 * indirect branch target is not yet known. Inserts 3 instructions into the gencode:
 *     mov %temp1,<TLS_IBP_FROM_TAG>
 *     mov %rbx,<TLS_IBP_TO_TAG>
 *     mov <TLS_XDX_SLOT>,%temp1
 * This saves the `from` and `to` tags of the IBL to TLS so we can generate a
 * hashcode for them after returning to DR, finally restoring the target app's
 * contents of %temp1. */
void
prepare_fcache_return_from_ibl(dcontext_t *dcontext, instrlist_t *bb,
                               app_pc ibl_routine_start_pc);

/* Called from generation of the IBL stub at the point the stub finds the indirect
 * branch target. Inserts one instruction into the gencode:
 *     jmp <ibp_gencode_head>
 * This jumps out to the head of the indirect branch path block of the IBL stub,
 * which we insert here in append_indirect_link_notification(). It basically says,
 * "IBL found the target, let's check the IBP hashtable and generate a hashcode
 * if this turns out to be a new BB pairing." */
void
append_indirect_link_notification_hook(dcontext_t *dcontext,
    instrlist_t *ilist, byte *ibl_routine_start_pc);

/* Called at the end of IBL stub generation to append the IBP block, which contains
 * about 25 instructions to:
 *    1. look in the IBP hashtable for the current BB pairing.
 *       a. the lookup key is generated by shifting the `from` tag left by
 *          one bit, followed by xor with the `to` tag, followed by
 *          activating the low bit to distinguish the key from NULL and the
 *          table end sentinel.
 *       b. the `from` tag is in %temp1
 *       c. the `to` tag is in memory at (%rcx)
 *       d. table traversal follows the same algorithm as the original IBL
 *          routine, according to open-addressing rules.
 *    2. when the IBP entry is found, restore the app's content into %temp1
 *       and jump back where we left off the main body of the IBL routine.
 *    3. when the IBP entry is not found, shuffle the registers to the
 *       configuration expected by fcache_return and call it.
 *       a. move the `from` tag back into %temp1 (it has drifted to %rbx)
 *       b. move the `to` tag into %rbx
 *       c. flag TLS_IBP_IS_NEW */
void
append_indirect_link_notification(dcontext_t *dcontext, instrlist_t *ilist,
                                  app_pc indirect_branch_lookup_routine,
                                  instr_t *fragment_not_found);

app_pc
adjust_for_ibl_instrumentation(dcontext_t *dcontext, app_pc pc, app_pc raw_start_pc);

/* Called by DR each time it emits an instruction into the code cache. This is
 * only used for debugging. When the instruction is the head of our IBP block,
 * a breakpoint is added for that PC to our generated gdb script. */
void
notify_emitting_instruction(instr_t *instr, byte *pc);

/* Called by DR when gencode is complete. This module keeps track of the
 * gencode phase and prints warnings if certain operations occur outside
 * of the expected phase. */
void
notify_gencode_complete();

/* Called by DR during IBL stub generation, identifying the head PC of the
 * IBL routine. This module tracks the generated IBL routine heads so that
 * it can filter ibl-like blocks to avoid instrumenting them for IBP. */
void
track_ibl_routine(byte *pc);

void
track_shared_syscall_routine(byte *pc);

void
destroy_crowd_safe_gencode();

#endif
