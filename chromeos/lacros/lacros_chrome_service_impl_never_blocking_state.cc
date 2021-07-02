// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_chrome_service_impl_never_blocking_state.h"

namespace chromeos {

LacrosChromeServiceImplNeverBlockingState::
    LacrosChromeServiceImplNeverBlockingState() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
LacrosChromeServiceImplNeverBlockingState::
    ~LacrosChromeServiceImplNeverBlockingState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Crosapi is the interface that lacros-chrome uses to message
// ash-chrome. This method binds the remote, which allows queuing of message
// to ash-chrome. The messages will not go through until
// RequestCrosapiReceiver() is invoked.
void LacrosChromeServiceImplNeverBlockingState::BindCrosapi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_crosapi_receiver_ = crosapi_.BindNewPipeAndPassReceiver();
}

void LacrosChromeServiceImplNeverBlockingState::FusePipeCrosapi(
    mojo::PendingRemote<crosapi::mojom::Crosapi> pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::FusePipes(std::move(pending_crosapi_receiver_),
                  std::move(pending_remote));
}

void LacrosChromeServiceImplNeverBlockingState::OnBrowserStartup(
    crosapi::mojom::BrowserInfoPtr browser_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  crosapi_->OnBrowserStartup(std::move(browser_info));
}

base::WeakPtr<LacrosChromeServiceImplNeverBlockingState>
LacrosChromeServiceImplNeverBlockingState::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
