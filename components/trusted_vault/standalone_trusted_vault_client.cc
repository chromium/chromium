// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/standalone_trusted_vault_frontend.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    SecurityDomainId security_domain,
    scoped_refptr<StandaloneTrustedVaultFrontend> frontend)
    : security_domain_(security_domain), frontend_(std::move(frontend)) {
  CHECK(frontend_);
  frontend_->RegisterClient(security_domain_, this);
}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() {
  frontend_->UnregisterClient(security_domain_, this);
}

void StandaloneTrustedVaultClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void StandaloneTrustedVaultClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void StandaloneTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  frontend_->FetchKeys(account_info, security_domain_, std::move(cb));
}

void StandaloneTrustedVaultClient::StoreKeys(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version,
    std::optional<TrustedVaultUserActionTriggerForUMA> trigger) {
  frontend_->StoreKeys(gaia_id, security_domain_, keys, last_key_version);
  NotifyTrustedVaultKeysChanged(trigger);
}

void StandaloneTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  frontend_->MarkLocalKeysAsStale(account_info, security_domain_,
                                  std::move(cb));
}

void StandaloneTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  frontend_->GetIsRecoverabilityDegraded(account_info, security_domain_,
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::AddTrustedRecoveryMethod(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  frontend_->AddTrustedRecoveryMethod(gaia_id, security_domain_, public_key,
                                      method_type_hint, std::move(cb));
}

void StandaloneTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  frontend_->ClearLocalDataForAccount(account_info);
}

void StandaloneTrustedVaultClient::WaitForIdleForTesting(
    base::OnceClosure cb) const {
  frontend_->WaitForIdleForTesting(std::move(cb));
}

void StandaloneTrustedVaultClient::
    GetLastAddedRecoveryMethodPublicKeyForTesting(
        base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  frontend_->GetLastAddedRecoveryMethodPublicKeyForTesting(std::move(callback));
}

void StandaloneTrustedVaultClient::GetLastKeyVersionForTesting(
    const GaiaId& gaia_id,
    base::OnceCallback<void(int last_key_version)> callback) {
  frontend_->GetLastKeyVersionForTesting(gaia_id, security_domain_,
                                         std::move(callback));
}

void StandaloneTrustedVaultClient::NotifyTrustedVaultKeysChanged(
    std::optional<TrustedVaultUserActionTriggerForUMA> trigger) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged(trigger);
  }
}

void StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged() {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

}  // namespace trusted_vault
