// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_upgrade_request_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/gpm_enclave_transaction.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/device_event_log/device_event_log.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"

using RenderFrameHost = content::RenderFrameHost;

void RecordPasskeyUpgradeResultHistogram(PasskeyUpgradeResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result", result);
}

PasskeyUpgradeRequestController::PasskeyUpgradeRequestController(
    RenderFrameHost* rfh,
    EnclaveRequestCallback enclave_request_callback)
    : frame_host_id_(rfh->GetGlobalId()),
      enclave_manager_(
          EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile())),
      enclave_request_callback_(enclave_request_callback) {
  if (enclave_manager_->is_loaded()) {
    OnEnclaveLoaded();
    return;
  }
  enclave_manager_->Load(
      base::BindOnce(&PasskeyUpgradeRequestController::OnEnclaveLoaded,
                     weak_factory_.GetWeakPtr()));
}

PasskeyUpgradeRequestController::~PasskeyUpgradeRequestController() = default;

void PasskeyUpgradeRequestController::TryUpgradePasswordToPasskey(
    std::string rp_id,
    const std::string& username,
    Delegate* delegate) {
  FIDO_LOG(EVENT) << "Passkey upgrade request started";
  CHECK(enclave_request_callback_)
      << "InitializeEnclaveRequestCallback() must be called first";
  CHECK(!pending_request_);
  CHECK(delegate);
  CHECK(!delegate_);

  pending_request_ = true;
  delegate_ = delegate;
  rp_id_ = std::move(rp_id);
  username_ = base::UTF8ToUTF16(username);

  if (!profile()->GetPrefs()->GetBoolean(
          password_manager::prefs::kAutomaticPasskeyUpgrades)) {
    FinishRequest(PasskeyUpgradeResult::kOptOut);
    return;
  }

  switch (enclave_state_) {
    case EnclaveState::kUnknown:
      // EnclaveLoaded() will invoke ContinuePendingUpgradeRequest().
      break;
    case EnclaveState::kError:
      FinishRequest(PasskeyUpgradeResult::kEnclaveNotInitialized);
      break;
    case EnclaveState::kReady:
      ContinuePendingUpgradeRequest();
      break;
  }
}

void PasskeyUpgradeRequestController::ContinuePendingUpgradeRequest() {
  CHECK(pending_request_);

  // When looking for passwords that might be eligible to be upgraded, only
  // consider passwords stored in GPM.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile());
  password_manager::PasswordStoreInterface* password_store = nullptr;
  if (password_manager::features_util::IsAccountStorageEnabled(sync_service)) {
    password_store = AccountPasswordStoreFactory::GetForProfile(
                         profile(), ServiceAccessType::EXPLICIT_ACCESS)
                         .get();
  } else if (password_manager::sync_util::
                 IsSyncFeatureEnabledIncludingPasswords(sync_service)) {
    // TODO(crbug.com/40066949): Remove this codepath once
    // `IsSyncFeatureEnabled()` is fully deprecated.
    password_store = ProfilePasswordStoreFactory::GetForProfile(
                         profile(), ServiceAccessType::EXPLICIT_ACCESS)
                         .get();
  }

  if (!password_store) {
    FinishRequest(PasskeyUpgradeResult::kPasswordStoreError);
    return;
  }

  GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(url), url);
  password_store->GetLogins(form_digest, weak_factory_.GetWeakPtr());
}

void PasskeyUpgradeRequestController::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  if (std::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    FinishRequest(PasskeyUpgradeResult::kPasswordStoreError);
    return;
  }
  password_manager::LoginsResult result =
      password_manager::GetLoginsOrEmptyListOnFailure(results_or_error);
  bool upgrade_eligible = false;
  bool match_not_recent = false;
  // A password with a matching username must have been used within the last 5
  // minutes in order for the automatic passkey upgrade to succeed.
  base::TimeDelta kLastUsedThreshold = base::Minutes(5);
  const auto min_last_used = base::Time::Now() - kLastUsedThreshold;
  for (const password_manager::PasswordForm& password_form : result) {
    if (password_form.username_value != username_) {
      continue;
    }
    // Consider multiple last use attributes for robustness. N.B.
    // `date_last_used` is updated after successful form submission on
    // Desktop, while `date_last_filled` is updated during form filling.
    if (std::max({password_form.date_created, password_form.date_last_filled,
                  password_form.date_last_used}) < min_last_used) {
      match_not_recent = true;
      continue;
    }
    upgrade_eligible = true;
    break;
  }

  if (!upgrade_eligible) {
    FinishRequest(match_not_recent
                      ? PasskeyUpgradeResult::kNoRecentlyUsedPassword
                      : PasskeyUpgradeResult::kNoMatchingPassword);
    return;
  }

  CHECK(enclave_request_callback_);
  enclave_transaction_ = std::make_unique<GPMEnclaveTransaction>(
      /*delegate=*/this, PasskeyModelFactory::GetForProfile(profile()),
      device::FidoRequestType::kMakeCredential, rp_id_,
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile()),
      /*pin=*/std::nullopt, /*selected_credential_id=*/std::nullopt,
      enclave_request_callback_);
  enclave_transaction_->Start();
}

void PasskeyUpgradeRequestController::HandleEnclaveTransactionError() {
  FinishRequest(PasskeyUpgradeResult::kEnclaveError);
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
  FinishRequest(PasskeyUpgradeResult::kSuccess);

  // Show the confirmation bubble.
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(
          content::WebContents::FromRenderFrameHost(&render_frame_host()));
  if (manage_passwords_ui_controller) {
    manage_passwords_ui_controller->OnPasskeyUpgrade(rp_id_);
  }
}

EnclaveUserVerificationMethod PasskeyUpgradeRequestController::GetUvMethod() {
  return EnclaveUserVerificationMethod::kNoUserVerificationAndNoUserPresence;
}

content::RenderFrameHost& PasskeyUpgradeRequestController::render_frame_host()
    const {
  auto* rfh = content::RenderFrameHost::FromID(frame_host_id_);
  CHECK(rfh);
  return *rfh;
}

Profile* PasskeyUpgradeRequestController::profile() const {
  return Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
}

void PasskeyUpgradeRequestController::OnEnclaveLoaded() {
  CHECK(enclave_manager_->is_loaded());
  enclave_state_ = enclave_manager_->is_ready() ? EnclaveState::kReady
                                                : EnclaveState::kError;
  if (!pending_request_) {
    return;
  }
  if (enclave_state_ == EnclaveState::kReady) {
    ContinuePendingUpgradeRequest();
  } else {
    FinishRequest(PasskeyUpgradeResult::kEnclaveNotInitialized);
  }
}

void PasskeyUpgradeRequestController::FinishRequest(
    PasskeyUpgradeResult result) {
  FIDO_LOG(ERROR) << "Passkey upgrade request complete: "
                  << static_cast<int>(result);

  RecordPasskeyUpgradeResultHistogram(result);

  if (result == PasskeyUpgradeResult::kSuccess) {
    delegate_->PasskeyUpgradeSucceeded();
  } else {
    delegate_->PasskeyUpgradeFailed();
  }
}
