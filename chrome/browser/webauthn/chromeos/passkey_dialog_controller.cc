// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_dialog_controller.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chromeos/passkey_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"

namespace chromeos {

PasskeyDialogController::PasskeyDialogController(
    AuthenticatorRequestDialogModel* dialog_model,
    PasskeyService* passkey_service,
    webauthn::PasskeyModel* passkey_model,
    const std::string& rp_id,
    device::FidoRequestType request_type,
    device::UserVerificationRequirement user_verification_requirement)
    : passkey_service_(passkey_service),
      dialog_model_(dialog_model),
      rp_id_(rp_id),
      credentials_(passkey_model->GetPasskeysForRelyingPartyId(rp_id)),
      request_type_(request_type),
      user_verification_requirement_(user_verification_requirement) {
  CHECK(passkey_model);
  CHECK(passkey_service_);
  CHECK(dialog_model_);

  passkey_service_->AddObserver(this);
  dialog_model_->observers.AddObserver(this);

  if (request_type == device::FidoRequestType::kMakeCredential ||
      !credentials_.empty()) {
    passkey_service_->FetchAccountState(
        base::BindOnce(&PasskeyDialogController::OnFetchAccountState,
                       weak_factory_.GetWeakPtr()));
  }
}

PasskeyDialogController::~PasskeyDialogController() {
  passkey_service_->RemoveObserver(this);
  dialog_model_->observers.RemoveObserver(this);
}

const std::vector<sync_pb::WebauthnCredentialSpecifics>&
PasskeyDialogController::credentials() const {
  return credentials_;
}

bool PasskeyDialogController::ready_for_ui() const {
  return account_state_.has_value();
}

PasskeyService::AccountState
PasskeyDialogController::account_state_for_testing() const {
  CHECK(account_state_.has_value());
  return *account_state_;
}

void PasskeyDialogController::OnFetchAccountState(
    PasskeyService::AccountState state) {
  account_state_ = state;
  dialog_model_->OnReadyForUI();
}

void PasskeyDialogController::OnGPMSelected() {
  using Step = AuthenticatorRequestDialogModel::Step;

  CHECK(account_state_);
  switch (*account_state_) {
    case PasskeyService::AccountState::kError:
    case PasskeyService::AccountState::kEmpty:
    case PasskeyService::AccountState::kEmptyAndNoLocalRecoveryFactors:
    case PasskeyService::AccountState::kIrrecoverable:
      NOTIMPLEMENTED();
      break;
    case PasskeyService::AccountState::kNeedsRecovery:
      dialog_model_->SetStep(Step::kRecoverSecurityDomain);
      break;
    case PasskeyService::AccountState::kReady:
      StartAuthenticatorRequest();
      break;
  }
}

void PasskeyDialogController::OnGPMPasskeySelected(
    std::vector<uint8_t> credential_id) {
  // FetchAccountState() should have completed before the UI was shown.
  CHECK(account_state_);
  switch (*account_state_) {
    case PasskeyService::AccountState::kError:
    case PasskeyService::AccountState::kEmpty:
    case PasskeyService::AccountState::kEmptyAndNoLocalRecoveryFactors:
    case PasskeyService::AccountState::kIrrecoverable:
      NOTIMPLEMENTED();
      break;
    case PasskeyService::AccountState::kNeedsRecovery:
      dialog_model_->SetStep(
          AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
      break;
    case PasskeyService::AccountState::kReady:
      StartAuthenticatorRequest();
      break;
  }
}

void PasskeyDialogController::OnHavePasskeysDomainSecret() {
  if (dialog_model_->step() !=
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain) {
    return;
  }
  CHECK_EQ(*account_state_, PasskeyService::AccountState::kNeedsRecovery);
  account_state_ = PasskeyService::AccountState::kReady;
  StartAuthenticatorRequest();
}

void PasskeyDialogController::StartAuthenticatorRequest() {
  dialog_model_->OnChromeOSGPMRequestReady();
}

}  // namespace chromeos
