// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_trusted_listener.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "native_client/src/public/chrome_main.h"

namespace {

#if defined(COMPILER_MSVC)
// Disable warning that we don't care about:
// warning C4722: destructor never returns, potential memory leak
#pragma warning(disable : 4722)
#endif

class NaClExitControlImpl : public nacl::mojom::NaClExitControl {
 public:
  ~NaClExitControlImpl() override {
    // If the binding disconnects, the renderer process dropped its connection
    // to this process (the NaCl loader process), either because the <embed>
    // element was removed (perhaps implicitly if the tab was closed) or because
    // the renderer crashed.  The NaCl loader process should therefore exit.
    //
    // For SFI NaCl, trusted code does this exit voluntarily, but untrusted
    // code cannot disable it.  However, for Non-SFI NaCl, the following exit
    // call could be disabled by untrusted code.
    NaClExit(0);
  }
};

void CreateExitControl(
    mojo::PendingReceiver<nacl::mojom::NaClExitControl> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<NaClExitControlImpl>(),
                              std::move(receiver));
}

}  // namespace

NaClTrustedListener::NaClTrustedListener(
    mojo::PendingRemote<nacl::mojom::NaClRendererHost> renderer_host,
    base::SingleThreadTaskRunner* io_task_runner)
    : renderer_host_(std::move(renderer_host)) {
  mojo::PendingRemote<nacl::mojom::NaClExitControl> exit_control;
  // The exit control binding must run on the IO thread. The main thread used
  // by NaClListener is busy in NaClChromeMainAppStart(), so it can't be used
  // for servicing messages.
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&CreateExitControl,
                                exit_control.InitWithNewPipeAndPassReceiver()));
  renderer_host_->ProvideExitControl(std::move(exit_control));
}

NaClTrustedListener::~NaClTrustedListener() = default;
