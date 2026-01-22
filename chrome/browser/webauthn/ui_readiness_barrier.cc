// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/ui_readiness_barrier.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

UiReadinessBarrier::UiReadinessBarrier(Delegate* delegate,
                                       AuthenticatorRequestDialogModel* model)
    : delegate_(delegate) {
  model_observation_.Observe(model);
}

UiReadinessBarrier::~UiReadinessBarrier() = default;

void UiReadinessBarrier::SetTransportAvailabilityInfo(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo tai) {
  tai_ = std::make_unique<
      device::FidoRequestHandlerBase::TransportAvailabilityInfo>(
      std::move(tai));
  TryToShowUI();
}

void UiReadinessBarrier::SetPasswordCredentials(
    PasswordCredentialFetcher::PasswordCredentials passwords) {
  passwords_ = std::make_unique<PasswordCredentialFetcher::PasswordCredentials>(
      std::move(passwords));
  TryToShowUI();
}

void UiReadinessBarrier::OnGPMReadyForUI() {
  TryToShowUI();
}

void UiReadinessBarrier::TryToShowUI() {
  if (!tai_) {
    return;
  }
  if (!delegate_->IsEnclaveReady()) {
    return;
  }
  if (delegate_->PasswordsUsable() && !passwords_) {
    return;
  }

  auto tai = std::move(*tai_);
  tai_.reset();

  auto passwords = passwords_
                       ? std::move(*passwords_)
                       : PasswordCredentialFetcher::PasswordCredentials();
  passwords_.reset();

  if (delegate_->IsEnclaveActive()) {
    delegate_->GetGpmPasskeys(
        std::move(tai),
        base::BindOnce(&UiReadinessBarrier::ContinueWithGpmPasskeys,
                       weak_ptr_factory_.GetWeakPtr(), std::move(passwords)));
    return;
  }

  delegate_->ShowUI(std::move(tai), std::move(passwords));
}

void UiReadinessBarrier::ContinueWithGpmPasskeys(
    PasswordCredentialFetcher::PasswordCredentials passwords,
    device::FidoRequestHandlerBase::TransportAvailabilityInfo tai) {
  delegate_->ShowUI(std::move(tai), std::move(passwords));
}
