// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "device/fido/features.h"

using Step = AuthenticatorRequestDialogModel::Step;

ChangePinControllerImpl::ChangePinControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ChangePinControllerImpl>(*web_contents),
      enclave_enabled_(
          base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator)) {
  if (!enclave_enabled_) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  enclave_manager_ =
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile);
  model_ = std::make_unique<AuthenticatorRequestDialogModel>(
      web_contents->GetPrimaryMainFrame());
  model_observation_.Observe(model_.get());
}

ChangePinControllerImpl::~ChangePinControllerImpl() {
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(false);
  }
}

bool ChangePinControllerImpl::IsChangePinFlowAvailable() {
  if (!enclave_enabled_) {
    return false;
  }
  return enclave_enabled_ && enclave_manager_->is_registered() &&
         enclave_manager_->is_ready() && enclave_manager_->has_wrapped_pin();
}

void ChangePinControllerImpl::StartChangePin(SuccessCallback callback) {
  if (!IsChangePinFlowAvailable()) {
    std::move(callback).Run(false);
    return;
  }
  notify_pin_change_callback_ = std::move(callback);
  model_->SetStep(Step::kGPMReauthForPinReset);
  RecordHistogram(ChangePinEvent::kFlowStartedFromSettings);
}

void ChangePinControllerImpl::CancelAuthenticatorRequest() {
  // User clicked "Cancel" in the GPM dialog.
  Reset(/*success=*/false);
  RecordHistogram(ChangePinEvent::kNewPinCancelled);
}

void ChangePinControllerImpl::OnReauthComplete(std::string rapt) {
  CHECK_EQ(model_->step(), Step::kGPMReauthForPinReset);
  rapt_ = std::move(rapt);
  model_->SetStep(Step::kGPMChangePin);
  RecordHistogram(ChangePinEvent::kReauthCompleted);
}

void ChangePinControllerImpl::OnRecoverSecurityDomainClosed() {
  // User closed the reauth window.
  Reset(/*success=*/false);
  RecordHistogram(ChangePinEvent::kReauthCancelled);
}

void ChangePinControllerImpl::OnGPMPinEntered(const std::u16string& pin) {
  CHECK(rapt_.has_value() && (model_->step() == Step::kGPMChangePin ||
                              model_->step() == Step::kGPMChangeArbitraryPin));
  model_->DisableUiOrShowLoadingDialog();
  enclave_manager_->ChangePIN(
      base::UTF16ToUTF8(pin), std::move(*rapt_),
      base::BindOnce(&ChangePinControllerImpl::OnGpmPinChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  rapt_.reset();
  RecordHistogram(ChangePinEvent::kNewPinEntered);
}

void ChangePinControllerImpl::OnGPMPinOptionChanged(bool is_arbitrary) {
  CHECK(model_->step() == Step::kGPMChangePin ||
        model_->step() == Step::kGPMChangeArbitraryPin);
  model_->SetStep(is_arbitrary ? Step::kGPMChangeArbitraryPin
                               : Step::kGPMChangePin);
}

// static
void ChangePinControllerImpl::RecordHistogram(ChangePinEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.Enclave.ChangePinEvents",
                                event);
}

void ChangePinControllerImpl::Reset(bool success) {
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(success);
  }

  rapt_.reset();
  model_->SetStep(Step::kNotStarted);
}

void ChangePinControllerImpl::OnGpmPinChanged(bool success) {
  if (!success) {
    model_->SetStep(Step::kGPMError);
    RecordHistogram(ChangePinEvent::kFailed);
    return;
  }
  Reset(/*success=*/true);
  RecordHistogram(ChangePinEvent::kCompletedSuccessfully);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChangePinControllerImpl);
