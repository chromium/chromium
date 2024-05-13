// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_service.h"

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace chromeos {

PasskeyService::PasskeyService(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    trusted_vault::TrustedVaultClient* trusted_vault_client,
    std::unique_ptr<trusted_vault::TrustedVaultConnection>
        trusted_vault_connection)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      trusted_vault_client_(trusted_vault_client),
      trusted_vault_connection_(std::move(trusted_vault_connection)) {
  CHECK(identity_manager_);
  CHECK(sync_service_);
  CHECK(trusted_vault_client_);

  trusted_vault_client_->AddObserver(this);
  UpdatePrimaryAccount();
}

PasskeyService::~PasskeyService() = default;

void PasskeyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasskeyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PasskeyService::GpmPasskeysAvailable() {
  return primary_account_.has_value() &&
         sync_service_->IsSyncFeatureEnabled() &&
         sync_service_->IsSyncFeatureActive() &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

void PasskeyService::FetchAccountState(AccountStateCallback callback) {
  CHECK(primary_account_);

  // First check if we have current security domain keys. If we do, we don't
  // need to download the security domain registration state.
  if (!trusted_vault_keys_.empty()) {
    std::move(callback).Run(AccountState::kReady);
    return;
  }

  pending_account_state_callbacks_.push_back(std::move(callback));
  MaybeFetchTrustedVaultKeys();
}

std::optional<std::vector<uint8_t>>
PasskeyService::GetCachedSecurityDomainSecret() {
  if (trusted_vault_keys_.empty()) {
    return std::nullopt;
  }
  return trusted_vault_keys_.back();
}

void PasskeyService::UpdatePrimaryAccount() {
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.IsEmpty() || primary_account == primary_account_) {
    return;
  }
  primary_account_ = primary_account;
  trusted_vault_keys_.clear();
}

void PasskeyService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdatePrimaryAccount();
}

void PasskeyService::OnTrustedVaultKeysChanged() {
  if (!primary_account_) {
    return;
  }
  trusted_vault_keys_.clear();
  MaybeFetchTrustedVaultKeys();
}

void PasskeyService::OnTrustedVaultRecoverabilityChanged() {}

void PasskeyService::MaybeFetchTrustedVaultKeys() {
  if (pending_fetch_trusted_vault_keys_) {
    return;
  }
  pending_fetch_trusted_vault_keys_ = true;
  trusted_vault_client_->FetchKeys(
      *primary_account_,
      base::BindOnce(&PasskeyService::OnFetchTrustedVaultKeys,
                     weak_factory_.GetWeakPtr()));
}

void PasskeyService::OnFetchTrustedVaultKeys(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys) {
  CHECK(pending_fetch_trusted_vault_keys_);
  pending_fetch_trusted_vault_keys_ = false;

  trusted_vault_keys_ = trusted_vault_keys;
  if (trusted_vault_keys_.empty()) {
    // Download security domain state to satisfy any pending account state
    // callbacks.
    MaybeDownloadAccountState();
    return;
  }

  for (Observer& observer : observers_) {
    observer.OnHavePasskeysDomainSecret();
  }

  RunPendingAccountStateCallbacks(AccountState::kReady);
}

void PasskeyService::MaybeDownloadAccountState() {
  CHECK(primary_account_);
  // Account state should only be downloaded if the security domain secret isn't
  // already known.
  CHECK(trusted_vault_keys_.empty());

  if (pending_account_state_callbacks_.empty() ||
      download_account_state_request_) {
    // No outstanding requests, or there is a download pending that will satisfy
    // them.
    return;
  }

  download_account_state_request_ =
      trusted_vault_connection_->DownloadAuthenticationFactorsRegistrationState(
          *primary_account_,
          base::BindOnce(&PasskeyService::OnDownloadAccountState,
                         weak_factory_.GetWeakPtr()));
}

void PasskeyService::OnDownloadAccountState(
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  CHECK(download_account_state_request_);
  download_account_state_request_.reset();

  AccountState state;
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  switch (result.state) {
    case Result::State::kError:
      state = AccountState::kError;
      break;
    case Result::State::kEmpty:
      // TODO: Check if this is the primary Lacros profile and whether there is
      // a suitable LSKF before proceeding. If there isn't, reply with
      // kEmptyAndNoLocalRecoveryFactors.
      state = AccountState::kEmpty;
      break;
    case Result::State::kRecoverable:
      state = AccountState::kNeedsRecovery;
      break;
    case Result::State::kIrrecoverable:
      state = AccountState::kIrrecoverable;
      break;
  }

  RunPendingAccountStateCallbacks(state);
}

void PasskeyService::RunPendingAccountStateCallbacks(AccountState state) {
  std::vector<AccountStateCallback> callbacks =
      std::move(pending_account_state_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(state);
  }
}

}  // namespace chromeos
