// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/userspace_swap_renderer_initialization_impl.h"

#include <sys/mman.h>
#include <utility>

#include "base/functional/callback.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "chromeos/ash/components/memory/aligned_memory.h"
#include "chromeos/ash/components/memory/userspace_swap/userfaultfd.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace memory {
namespace userspace_swap {

UserspaceSwapRendererInitializationImpl::
    UserspaceSwapRendererInitializationImpl() {
  CHECK(UserspaceSwapRendererInitializationImpl::
            UserspaceSwapSupportedAndEnabled());
}

UserspaceSwapRendererInitializationImpl::
    ~UserspaceSwapRendererInitializationImpl() = default;

bool UserspaceSwapRendererInitializationImpl::PreSandboxSetup() {
  // The caller should have verified platform support before invoking
  // PreSandboxSetup.
  CHECK(UserspaceSwapRendererInitializationImpl::
            UserspaceSwapSupportedAndEnabled());

  std::unique_ptr<UserfaultFD> uffd =
      UserfaultFD::Create(static_cast<UserfaultFD::Features>(
          UserfaultFD::Features::kFeatureUnmap |
          UserfaultFD::Features::kFeatureRemap |
          UserfaultFD::Features::kFeatureRemove));

  if (!uffd) {
    uffd_errno_ = errno;
    return false;
  }

  uffd_ = uffd->ReleaseFD();

  // We need to create an area which is large enough to move all the PTEs for a
  // given swap round we may need. Since we're creating this mapping as
  // PROT_NONE it will not be accessible and it will not actually account any
  // memory.
  auto config = UserspaceSwapConfig::Get();
  swap_area_len_ = config.number_of_pages_per_region * base::GetPageSize() *
                   config.renderer_region_limit_per_swap;

  void* swap_area = nullptr;
  do {
    // We try to pick a random address to map the region which will be used for
    // MREMAP_DONTUNMAP.
    // Virtual Addresses are 48 bits.
    // (1) Make sure the most significant bit is not set to not intersect with
    // kernel addresses.
    // (2) Make sure it's page aligned.
    // (3) Make sure it's not 0 or only lower 32-bits (to avoid mmap_min_addr).
    // Use 47 bits for reason (1) above.
    constexpr uint64_t kVirtualAddrMask = (static_cast<uint64_t>(1) << 47) - 1;
    uint64_t rand_address = base::RandUint64() & kVirtualAddrMask;
    rand_address &= ~(base::GetPageSize() - 1);
    VLOG(1) << "Try to get address " << reinterpret_cast<void*>(rand_address)
            << " for userspace swap area";

    if (rand_address <= UINT32_MAX)
      continue;

    errno = 0;
    swap_area =
        mmap(reinterpret_cast<void*>(rand_address), swap_area_len_, PROT_NONE,
             MAP_FIXED_NOREPLACE | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    VPLOG(1) << "Kernel returned memory area: " << swap_area
             << " len: " << swap_area_len_;
  } while (swap_area == MAP_FAILED && errno == EEXIST);

  swap_area_ = reinterpret_cast<uint64_t>(swap_area);

  if (swap_area == MAP_FAILED) {
    PLOG(ERROR) << "Unable to create VMA for use as a swap area";
    mmap_errno_ = errno;
    swap_area_ = 0;
    swap_area_len_ = 0;
    uffd_.reset();
  }

  return uffd_.is_valid();
}

void UserspaceSwapRendererInitializationImpl::TransferFDsOrCleanup(
    base::OnceCallback<void(mojo::GenericPendingReceiver)>
        bind_host_receiver_callback) {
  mojo::Remote<::userspace_swap::mojom::UserspaceSwapInitialization> remote;
  std::move(bind_host_receiver_callback)
      .Run(remote.BindNewPipeAndPassReceiver());

  remote->TransferUserfaultFD(
      uffd_errno_, mojo::PlatformHandle(std::move(uffd_)), mmap_errno_,
      ::userspace_swap::mojom::MemoryRegion::New(swap_area_, swap_area_len_));

  mmap_errno_ = 0;
  uffd_errno_ = 0;
}

// Static
bool UserspaceSwapRendererInitializationImpl::
    UserspaceSwapSupportedAndEnabled() {
  return ash::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
