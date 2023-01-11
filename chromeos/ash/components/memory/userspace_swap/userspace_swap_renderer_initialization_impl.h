// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace ash {
namespace memory {
namespace userspace_swap {

class COMPONENT_EXPORT(USERSPACE_SWAP) UserspaceSwapRendererInitializationImpl {
 public:
  UserspaceSwapRendererInitializationImpl();

  UserspaceSwapRendererInitializationImpl(
      const UserspaceSwapRendererInitializationImpl&) = delete;
  UserspaceSwapRendererInitializationImpl& operator=(
      const UserspaceSwapRendererInitializationImpl&) = delete;

  ~UserspaceSwapRendererInitializationImpl();

  static bool UserspaceSwapSupportedAndEnabled();

  // PreSandboxSetup() is responsible for creating any resources that might be
  // needed before we enter the sandbox.
  bool PreSandboxSetup();

  // TransferFDsOrCleanup should be called after the sandbox has been entered.
  // |bind_host_receiver_callback| will be invoked to bind a mojo interface
  // between the renderer process and its browser process host.
  void TransferFDsOrCleanup(
      base::OnceCallback<void(mojo::GenericPendingReceiver)>
          bind_host_receiver_callback);

 private:
  int uffd_errno_ = 0;
  base::ScopedFD uffd_;

  int mmap_errno_ = 0;
  uint64_t swap_area_ = 0;
  uint64_t swap_area_len_ = 0;
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
