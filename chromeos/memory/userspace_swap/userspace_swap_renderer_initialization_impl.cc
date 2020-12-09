// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/userspace_swap/userspace_swap_renderer_initialization_impl.h"

#include <utility>

#include "base/callback.h"
#include "chromeos/memory/userspace_swap/userfaultfd.h"
#include "chromeos/memory/userspace_swap/userspace_swap.h"
#include "chromeos/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
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
  }

  uffd_ = uffd->ReleaseFD();
  return uffd_.is_valid();
}

void UserspaceSwapRendererInitializationImpl::TransferFDsOrCleanup(
    base::OnceCallback<void(mojo::GenericPendingReceiver)>
        bind_host_receiver_callback) {
  mojo::Remote<::userspace_swap::mojom::UserspaceSwapInitialization> remote;
  std::move(bind_host_receiver_callback)
      .Run(remote.BindNewPipeAndPassReceiver());
  remote->TransferUserfaultFD(uffd_errno_,
                              mojo::PlatformHandle(std::move(uffd_)));
  uffd_errno_ = 0;
}

// Static
bool UserspaceSwapRendererInitializationImpl::
    UserspaceSwapSupportedAndEnabled() {
  return chromeos::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos
