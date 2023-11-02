// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERFAULTFD_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERFAULTFD_H_

#include <list>
#include <memory>

#include "base/component_export.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

struct uffd_msg;

namespace ash {
namespace memory {
namespace userspace_swap {

// UserfaultFDHandler is an interface that a class must implement to subscribe
// to UserfaultFD events. You may not receive all events, the events you will
// receive depend on the features used when the userfaultfd is created. It's
// always safe to use the UserfaultFD class associated with this handler during
// a callback, you're guaranteed that a UserfaultFD will always outlive its
// associated handler.
//
// For the purpose of a fault handler we will refer to the events received as
// two different types: pagefault events and non-pagefault events. You're
// guaranteed that pagefault events and non-pagefault events will all be
// delivered in order with respect to their group. Page fault events that
// couldn't be handled will be redelivered later until they are able to be
// handled. This isn't a problem, because when responding to a pagefault event
// any attempt to resolve it using CopyToRange or ZeroRange would fail if PTEs
// already exist or the mapping is gone. For those reasons it's important that
// you always read and handle those non-pagefault events before pagefaults. A
// more concrete example of this would be: suppose you had a MADV_DONTNEED
// racing with a pagefault to be handled. If you were to process the pagefault
// first before observing the Remove event you could potentially restore stale
// memory when the correct action after the MADV_DONTNEED would be to zero the
// range.
class COMPONENT_EXPORT(USERSPACE_SWAP) UserfaultFDHandler {
 public:
  // PagefaultFlags are passed in the Pagefault Handler.
  enum PagefaultFlags {
    kReadFault = 0,
    kWriteFault = 1 << 0,
  };

  // A Pagefault callback is delivered on a pagefault in a registered region.
  // The |fault_address| is the address that caused the fault, and |fault_flags|
  // specify any flags related to the fault, such as read or write fault.
  // Finally if kFeatureThreadID is set when the UserfaultFD is created, |tid|
  // will be set to the thread id that caused the fault, otherwise it will be
  // zero.
  //
  // The implementation is responsible for returning true or false, when true
  // is return it means the fault was handled, when false is returned the fault
  // will be retried later. The reason for this is you cannot resolve a fault
  // while mappings are changing.
  virtual bool Pagefault(uintptr_t fault_address,
                         PagefaultFlags fault_flags,
                         base::PlatformThreadId tid) = 0;

  // An Unmapped callback will be delivered when a region or subregion which was
  // registered with the UserfaultFD has been unmapped, either explicitly by an
  // munmap(2) or implicitly by a mremap(2). The range that was unmapped will be
  // specified by |range_start| to |range_end|. Unmapped callbacks will only be
  // received if the UserfaultFD was created using the kFeatureUnmap flag.
  virtual void Unmapped(uintptr_t range_start, uintptr_t range_end) = 0;

  // A Removed callback will be delivered when a region has page tables entries
  // removed, this can happen from an madvise(MADV_DONTNEED or MADV_FREE). The
  // range that was removed will be specified by |range_start| to |range_end|.
  // Removed callbacks will only be received if the UserfaultFD was created
  // using the kFeatureRemove flag.
  virtual void Removed(uintptr_t range_start, uintptr_t range_end) = 0;

  // A Remapped callback will be delivered when a region or subregion which was
  // registered with the UserfaultFD has been remapped by a call to mremap(2).
  // The region that was remapped will be described by |old_address| and
  // |original_length| the address where the mapping was moved to will be set in
  // |new_address|. Remapped callbacks will only be received if the UserfaultFD
  // was created using the kFeatureRemap flag.
  virtual void Remapped(uintptr_t old_address,
                        uintptr_t new_address,
                        uint64_t original_length) = 0;

  // Closed will be invoked when the UserfaultFD receives an EOF (closed) or an
  // error condition. |err| will be set to 0 on EOF or an errno value if the
  // read failed for an unexpected reason. Closed will always be the final
  // callback a UserfaultFDHandler will receive.
  virtual void Closed(int err) = 0;

  virtual ~UserfaultFDHandler() = default;
};

// UserfaultFD provides an implementation for the userfaultfd(2) system call.
//
// NOTE: All operations on a UserfaultFD expect page aligned addresses and
// page multiple lengths.
class COMPONENT_EXPORT(USERSPACE_SWAP) UserfaultFD {
 public:
  enum Features {
    // kFeatureRemap will subscribe to Remap callbacks.
    kFeatureRemap = 1 << 0,
    // kFeatureUnmap will subscribe to Unmap callbacks.
    kFeatureUnmap = 1 << 1,
    // kFeatureRemove will subscribe to Remove callbacks (PTEs removed).
    kFeatureRemove = 1 << 2,
    // kFeatureThreadID will cause Pagefault callbacks to include the faulting
    // thread id.
    kFeatureThreadID = 1 << 3,
  };

  // Note: Although it's documented UFFDIO_REGISTER_MDOE_WP is not actually
  // implemented as of 5.5 kernel, see:
  // https://elixir.bootlin.com/linux/v5.5-rc3/source/fs/userfaultfd.c#L1331
  // We use an enum so this can added later; it's on track to land in the 5.7
  // kernel.
  enum RegisterMode {
    // Deferred allows you to register a range but not start receiving fault
    // events on it until you've registered with kRegisterMissing.
    kRegisterDeferred = 0,
    // kRegisterMissing will register a range to receive missing page events
    // (page faults).
    kRegisterMissing = 1 << 0,
  };

  // RegisterRange will register an address range with the userfaultfd.
  bool RegisterRange(RegisterMode mode, uintptr_t range_start, uint64_t len);

  // UnregisterRange will unregister an address range with the userfaultfd.
  bool UnregisterRange(uintptr_t range_start, uint64_t len);

  // CopyToRange will resolve a fault by using the UFFDIO_COPY ioctl. This
  // uses the default behavior of waking the blocked task after the fault has
  // been resolved. |copied| will contain the number of bytes copied. It's
  // important to check |copied| when CopyToRange return false as it may have
  // copied the pages; but it can still fail to wake the range causing an
  // EAGAIN.
  bool CopyToRange(uintptr_t dest_range_start,
                   uint64_t len,
                   uintptr_t src_range_start,
                   int64_t* copied);

  // ZeroRange will zero fill a range to resolve a fault using the UFFDIO_ZERO
  // ioctl. Similarly to CopyToRange the blocked task will be woken after the
  // fault is resolved. |zeored| will return the number of bytes zeroed, it's
  // important to check |zeored| even when ZeroRange returns false as it may
  // have only failed to wake the range and would return EAGAIN in that
  // situation.
  bool ZeroRange(uintptr_t range_start, uint64_t len, int64_t* zeroed);

  // Wake any blocked tasks on this range.
  bool WakeRange(uintptr_t range_start, uint64_t len);

  // StartWaitingForEvents will create a blocking task which will monitor the
  // userfaultfd for events. The ownership of |handler| is transferred to the
  // userfaultfd class and will go out of scope when the userfaultfd is
  // closed or if there is an error. But UserfaultFDHandler::Closed() will
  // always be called before |handler| is destroyed.
  bool StartWaitingForEvents(std::unique_ptr<UserfaultFDHandler> handler);

  // CloseAndStopWaitingForEvents will trigger userfaultfd to close.
  void CloseAndStopWaitingForEvents();

  // Will return true if the userfaultfd syscall is supported.
  static bool KernelSupportsUserfaultFD();

  // Create will create a new UserfaultFD.
  static std::unique_ptr<UserfaultFD> Create(Features features);

  // Wrap FD is used to take a donated FD and assume ownership of it.
  static std::unique_ptr<UserfaultFD> WrapFD(base::ScopedFD fd);

  UserfaultFD(const UserfaultFD&) = delete;
  UserfaultFD& operator=(const UserfaultFD&) = delete;

  ~UserfaultFD();

  base::ScopedFD ReleaseFD();

 private:
  friend class UserfaultFDTest;

  explicit UserfaultFD(base::ScopedFD fd);

  void UserfaultFDReadable();

  bool DispatchMessage(const uffd_msg& msg);

  // DrainPendingFaults will attempt to deliver any pending fault messages.
  bool DrainPendingFaults();

  // Because userfaultfd will return -EAGAIN when the memory maps are changing
  // until the remap, unmap, or remove message has been read off the userfaultfd
  // we provide a mechanism for users to re-enque the fault to be delivered
  // again.
  std::list<uffd_msg> pending_faults_;

  // We need to make sure messages are read and posted in order so we prevent
  // two different threads from simultaenously reading and posting.
  base::Lock read_lock_;

  base::ScopedFD fd_;

  std::unique_ptr<UserfaultFDHandler> handler_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_controller_;
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERFAULTFD_H_
