// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_upgrade_request_controller.h"

#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/device_event_log/device_event_log.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/fido_discovery_factory.h"

using RenderFrameHost = content::RenderFrameHost;

DOCUMENT_USER_DATA_KEY_IMPL(PasskeyUpgradeRequestController);

PasskeyUpgradeRequestController::~PasskeyUpgradeRequestController() = default;

void PasskeyUpgradeRequestController::InitializeEnclaveRequestCallback(
    device::FidoDiscoveryFactory* discovery_factory) {
  using EnclaveEventStream = device::FidoDiscoveryBase::EventStream<
      std::unique_ptr<device::enclave::CredentialRequest>>;
  std::unique_ptr<EnclaveEventStream> event_stream;
  std::tie(enclave_request_callback_, event_stream) = EnclaveEventStream::New();
  discovery_factory->set_enclave_ui_request_stream(std::move(event_stream));
}

void PasskeyUpgradeRequestController::TryUpgradePasswordToPasskey(
    std::string rp_id,
    const std::string& user_name,
    base::OnceCallback<void(bool success)> callback) {
  FIDO_LOG(EVENT) << "Passkey upgrade request started";
  CHECK(enclave_request_callback_);
  CHECK(!pending_callback_);
  pending_callback_ = std::move(callback);
  rp_id_ = std::move(rp_id);
  user_name_ = base::UTF8ToUTF16(user_name);

  switch (enclave_state_) {
    case EnclaveState::kUnknown:
      // EnclaveLoaded() will invoke ContinuePendingUpgradeRequest().
      pending_upgrade_request_ = true;
      break;
    case EnclaveState::kNotReady:
      FIDO_LOG(EVENT) << "Passkey upgrade request failed because the enclave "
                         "isn't initialized.";
      std::move(pending_callback_).Run(false);
      break;
    case EnclaveState::kReady:
      pending_upgrade_request_ = true;
      ContinuePendingUpgradeRequest();
      break;
  }
}

void PasskeyUpgradeRequestController::ContinuePendingUpgradeRequest() {
  CHECK_EQ(enclave_state_, EnclaveState::kReady);
  CHECK(pending_upgrade_request_);
  pending_upgrade_request_ = false;

  // TODO(crbug.com/377758786): The profile password store is probably wrong in
  // some cases. Find out how to query GPM specifically.
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile(), ServiceAccessType::EXPLICIT_ACCESS);
  GURL url = origin().GetURL();
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(url), url);
  password_store->GetLogins(form_digest, weak_factory_.GetWeakPtr());
}

void PasskeyUpgradeRequestController::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  if (absl::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    FIDO_LOG(EVENT) << "Passkey upgrade failed due to password store error";
  }
  CHECK(pending_callback_);
  password_manager::LoginsResult result =
      password_manager::GetLoginsOrEmptyListOnFailure(results_or_error);
  bool found = false;
  // Passwords must have been used within the last 90 days in order to be
  // eligible.
  const auto min_last_used = base::Time::Now() - base::Days(90);
  for (const password_manager::PasswordForm& password_form : result) {
    if (password_form.username_value == user_name_ &&
        password_form.date_last_used >= min_last_used) {
      found = true;
      break;
    }
  }

  if (!found) {
    FIDO_LOG(EVENT) << "Passkey upgrade request failed, no matching password";
    std::move(pending_callback_).Run(false);
    return;
  }

  CHECK(enclave_request_callback_);
  // TODO(crbug.com/377758786): Make the request up=0.
  enclave_transaction_ = std::make_unique<GPMEnclaveTransaction>(
      /*delegate=*/this, PasskeyModelFactory::GetForProfile(profile()),
      device::FidoRequestType::kMakeCredential, rp_id_,
      EnclaveUserVerificationMethod::kNone,
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile()),
      /*pin=*/std::nullopt, /*selected_credential_id=*/std::nullopt,
      enclave_request_callback_);
  enclave_transaction_->Start();
}

void PasskeyUpgradeRequestController::HandleEnclaveTransactionError() {
  if (!pending_callback_) {
    return;
  }
  FIDO_LOG(ERROR) << "Passkey upgrade failed on enclave error";
  std::move(pending_callback_).Run(false);
}

void PasskeyUpgradeRequestController::BuildUVKeyOptions(
    EnclaveManager::UVKeyOptions&) {
  // Upgrade requests don't perform user verification.
  NOTIMPLEMENTED();
}

void PasskeyUpgradeRequestController::HandlePINValidationResult(
    device::enclave::PINValidationResult) {
  // Upgrade requests don't perform user verification.
  NOTIMPLEMENTED();
}

void PasskeyUpgradeRequestController::OnPasskeyCreated(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(
          content::WebContents::FromRenderFrameHost(&render_frame_host()));
  FIDO_LOG(EVENT) << "Passkey upgrade request succeeded";
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnPasskeyUpgrade(rp_id_);
  }

  CHECK(pending_callback_);
  std::move(pending_callback_).Run(true);
}

Profile* PasskeyUpgradeRequestController::profile() const {
  return Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
}

void PasskeyUpgradeRequestController::OnEnclaveLoaded() {
  CHECK(enclave_manager_->is_loaded());
  CHECK_EQ(enclave_state_, EnclaveState::kUnknown);
  enclave_state_ = enclave_manager_->is_ready() ? EnclaveState::kReady
                                                : EnclaveState::kNotReady;
  if (pending_upgrade_request_) {
    ContinuePendingUpgradeRequest();
  }
}

PasskeyUpgradeRequestController::PasskeyUpgradeRequestController(
    RenderFrameHost* rfh)
    : content::DocumentUserData<PasskeyUpgradeRequestController>(rfh),
      enclave_manager_(
          EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile())) {
  if (enclave_manager_->is_loaded()) {
    OnEnclaveLoaded();
    return;
  }
  enclave_manager_->Load(
      base::BindOnce(&PasskeyUpgradeRequestController::OnEnclaveLoaded,
                     weak_factory_.GetWeakPtr()));
}
