// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
#define CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace chromeos {
namespace memory {
namespace userspace_swap {

class CHROMEOS_EXPORT UserspaceSwapRendererInitializationImpl {
 public:
  UserspaceSwapRendererInitializationImpl();
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

  DISALLOW_COPY_AND_ASSIGN(UserspaceSwapRendererInitializationImpl);
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
