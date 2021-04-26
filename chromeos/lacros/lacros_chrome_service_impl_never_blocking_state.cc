// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_chrome_service_impl_never_blocking_state.h"

#include "base/bind_post_task.h"
#include "base/notreached.h"

namespace chromeos {

LacrosChromeServiceImplNeverBlockingState::
    LacrosChromeServiceImplNeverBlockingState(
        scoped_refptr<base::SequencedTaskRunner> owner_sequence,
        base::WeakPtr<LacrosChromeServiceImpl> owner)
    : owner_sequence_(owner_sequence), owner_(owner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
LacrosChromeServiceImplNeverBlockingState::
    ~LacrosChromeServiceImplNeverBlockingState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// crosapi::mojom::BrowserService:
void LacrosChromeServiceImplNeverBlockingState::REMOVED_2(
    crosapi::mojom::BrowserInitParamsPtr) {
  NOTIMPLEMENTED();
}

void LacrosChromeServiceImplNeverBlockingState::RequestCrosapiReceiver(
    RequestCrosapiReceiverCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(hidehiko): Remove non-error logging from here.
  LOG(WARNING) << "CrosapiReceiver requested.";
  std::move(callback).Run(std::move(pending_crosapi_receiver_));
}

void LacrosChromeServiceImplNeverBlockingState::NewWindow(
    bool incognito,
    NewWindowCallback callback) {
  owner_sequence_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::NewWindowAffineSequence, owner_,
                     incognito),
      std::move(callback));
}

void LacrosChromeServiceImplNeverBlockingState::NewTab(
    NewTabCallback callback) {
  owner_sequence_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::NewTabAffineSequence, owner_),
      std::move(callback));
}

void LacrosChromeServiceImplNeverBlockingState::RestoreTab(
    RestoreTabCallback callback) {
  owner_sequence_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::RestoreTabAffineSequence,
                     owner_),
      std::move(callback));
}

void LacrosChromeServiceImplNeverBlockingState::GetFeedbackData(
    GetFeedbackDataCallback callback) {
  owner_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::GetFeedbackDataAffineSequence,
                     owner_,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(callback))));
}

void LacrosChromeServiceImplNeverBlockingState::GetHistograms(
    GetHistogramsCallback callback) {
  owner_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::GetHistogramsAffineSequence,
                     owner_,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(callback))));
}

void LacrosChromeServiceImplNeverBlockingState::GetActiveTabUrl(
    GetActiveTabUrlCallback callback) {
  owner_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImpl::GetActiveTabUrlAffineSequence,
                     owner_,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(callback))));
}

void LacrosChromeServiceImplNeverBlockingState::UpdateDeviceAccountPolicy(
    const std::vector<uint8_t>& policy) {
  owner_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceImpl::UpdateDeviceAccountPolicyAffineSequence,
          owner_, policy));
}

// Crosapi is the interface that lacros-chrome uses to message
// ash-chrome. This method binds the remote, which allows queuing of message
// to ash-chrome. The messages will not go through until
// RequestCrosapiReceiver() is invoked.
void LacrosChromeServiceImplNeverBlockingState::BindCrosapi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_crosapi_receiver_ = crosapi_.BindNewPipeAndPassReceiver();
}

// BrowserService is the interface that ash-chrome uses to message
// lacros-chrome. This handles and routes all incoming messages from
// ash-chrome.
void LacrosChromeServiceImplNeverBlockingState::BindBrowserServiceReceiver(
    mojo::PendingReceiver<crosapi::mojom::BrowserService> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.Bind(std::move(receiver));
}

void LacrosChromeServiceImplNeverBlockingState::FusePipeCrosapi(
    mojo::PendingRemote<crosapi::mojom::Crosapi> pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::FusePipes(std::move(pending_crosapi_receiver_),
                  std::move(pending_remote));
  crosapi_->BindBrowserServiceHost(
      browser_service_host_.BindNewPipeAndPassReceiver());
  browser_service_host_->AddBrowserService(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
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
