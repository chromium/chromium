// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

using Step = AuthenticatorRequestDialogModel::Step;

ChangePinControllerImpl::ChangePinControllerImpl(
    content::WebContents* web_contents)
    : enclave_enabled_(
          base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator)) {
  if (!enclave_enabled_) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  enclave_manager_ =
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile);
  sync_service_ = SyncServiceFactory::IsSyncAllowed(profile)
                      ? SyncServiceFactory::GetForProfile(profile)
                      : nullptr;
  model_ = std::make_unique<AuthenticatorRequestDialogModel>(
      web_contents->GetPrimaryMainFrame());
  model_observation_.Observe(model_.get());
}

ChangePinControllerImpl::~ChangePinControllerImpl() {
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(false);
  }
}

// static
ChangePinControllerImpl* ChangePinControllerImpl::ForWebContents(
    content::WebContents* web_contents) {
  static constexpr char kChangePinControllerImplKey[] =
      "ChangePinControllerImplKey";
  if (!web_contents->GetUserData(kChangePinControllerImplKey)) {
    web_contents->SetUserData(
        kChangePinControllerImplKey,
        std::make_unique<ChangePinControllerImpl>(web_contents));
  }
  return static_cast<ChangePinControllerImpl*>(
      web_contents->GetUserData(kChangePinControllerImplKey));
}

bool ChangePinControllerImpl::IsChangePinFlowAvailable() {
  if (!enclave_enabled_) {
    return false;
  }
  bool sync_enabled = sync_service_ && sync_service_->IsSyncFeatureEnabled() &&
                      sync_service_->GetUserSettings()->GetSelectedTypes().Has(
                          syncer::UserSelectableType::kPasswords);
  bool enclave_valid = enclave_enabled_ && enclave_manager_->is_ready() &&
                       enclave_manager_->has_wrapped_pin();
  return sync_enabled && enclave_valid;
}

void ChangePinControllerImpl::StartChangePin(SuccessCallback callback) {
  if (!IsChangePinFlowAvailable()) {
    std::move(callback).Run(false);
    return;
  }
  notify_pin_change_callback_ = std::move(callback);
  // TODO(enclave): use local UV instead of GPM reauth when available.
  model_->SetStep(Step::kGPMReauthForPinReset);
}

void ChangePinControllerImpl::CancelAuthenticatorRequest() {
  // User clicked "Cancel" in the GPM dialog.
  Reset(/*success=*/false);
}

void ChangePinControllerImpl::OnReauthComplete(std::string rapt) {
  CHECK_EQ(model_->step(), Step::kGPMReauthForPinReset);
  rapt_ = std::move(rapt);
  model_->SetStep(Step::kGPMCreatePin);
}

void ChangePinControllerImpl::OnRecoverSecurityDomainClosed() {
  // User closed the reauth window.
  Reset(/*success=*/false);
}

void ChangePinControllerImpl::OnGPMPinEntered(const std::u16string& pin) {
  CHECK(rapt_.has_value() && (model_->step() == Step::kGPMCreatePin ||
                              model_->step() == Step::kGPMCreateArbitraryPin));
  enclave_manager_->ChangePIN(
      base::UTF16ToUTF8(pin), std::move(rapt_),
      base::BindOnce(&ChangePinControllerImpl::OnGpmPinChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangePinControllerImpl::OnGPMPinOptionChanged(bool is_arbitrary) {
  CHECK(model_->step() == Step::kGPMCreatePin ||
        model_->step() == Step::kGPMCreateArbitraryPin);
  model_->SetStep(is_arbitrary ? Step::kGPMCreateArbitraryPin
                               : Step::kGPMCreatePin);
}

void ChangePinControllerImpl::Reset(bool success) {
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(success);
  }

  model_->SetStep(Step::kNotStarted);
  rapt_.reset();
}

void ChangePinControllerImpl::OnGpmPinChanged(bool success) {
  if (!success) {
    model_->SetStep(Step::kGPMError);
    return;
  }
  Reset(/*success=*/true);
}
