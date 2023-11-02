// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/performance_manager/mechanisms/userspace_swap_impl_chromeos.h"

#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "base/threading/scoped_blocking_call.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// MREMAP_DONTUNMAP was added in the 5.7 kernel, but we've backported it to
// earlier CrOS kernels.
#if !defined(MREMAP_DONTUNMAP)
#define MREMAP_DONTUNMAP 4
#endif

namespace performance_manager {
namespace mechanism {

using ::ash::memory::userspace_swap::UserspaceSwapConfig;

UserspaceSwapImpl::UserspaceSwapImpl() {
  CHECK(UserspaceSwapImpl::PlatformSupportsUserspaceSwap());
}

UserspaceSwapImpl::~UserspaceSwapImpl() = default;

// static
void UserspaceSwapImpl::Create(
    mojo::PendingReceiver<::userspace_swap::mojom::UserspaceSwap> receiver) {
  if (!PlatformSupportsUserspaceSwap()) {
    DLOG(WARNING) << "Userspace swap not supported by platform";
    return;
  }
  auto impl = std::make_unique<UserspaceSwapImpl>();
  mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver));
}

// static
bool UserspaceSwapImpl::PlatformSupportsUserspaceSwap() {
  return ash::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

void UserspaceSwapImpl::MovePTEsLeavingMapping(MemoryRegionPtr src,
                                               uint64_t dest) {
  DCHECK(PlatformSupportsUserspaceSwap());

  // We're moving to a known location, the location provided to us here is one
  // that the renderer created and reserved for the browser the renderers
  // address space for moving mappings to. The browser will observe this remap
  // and then it will know it can safely use process_vm_readv() to move over the
  // memory.
  //
  // The MREMAP_DONTUNMAP (which requires MREMAP_MAYMOVE) is used to move just
  // the PTEs leaving the original mapping in place (with the userfaultfd still
  // attached). The MREMAP_FIXED tells the kernel we know where we want the
  // memory to go.
  void* dest_mapping =
      mremap(reinterpret_cast<void*>(src->address), src->length, src->length,
             MREMAP_MAYMOVE | MREMAP_DONTUNMAP | MREMAP_FIXED,
             reinterpret_cast<void*>(dest));
  PCHECK(dest_mapping != MAP_FAILED) << "mremap failed";
}

void UserspaceSwapImpl::MapArea(MemoryRegionPtr area) {
  DCHECK(PlatformSupportsUserspaceSwap());

  // MapArea is used to drop pages after they have been read over to the browser
  // process. We do this rather than MADV_DONTNEED because this will fully drop
  // the VMA and recreate it while holding the MM sem. This means the memory
  // will be unaccounted.
  void* dest_mapping =
      mmap(reinterpret_cast<void*>(area->address), area->length, PROT_NONE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  PCHECK(dest_mapping != MAP_FAILED) << "Unable to map area";
}

void UserspaceSwapImpl::GetPartitionAllocSuperPagesUsed(
    int32_t max_superpages,
    UserspaceSwapImpl::GetPartitionAllocSuperPagesUsedCallback callback) {
  std::vector<::userspace_swap::mojom::MemoryRegionPtr> areas;
  ash::memory::userspace_swap::GetPartitionAllocSuperPagesInUse(max_superpages,
                                                                areas);
  std::move(callback).Run(std::move(areas));
}

}  // namespace mechanism
}  // namespace performance_manager
