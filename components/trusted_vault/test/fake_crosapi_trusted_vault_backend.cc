// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"

#include <utility>

#include "base/check.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"

namespace trusted_vault {

FakeCrosapiTrustedVaultBackend::FakeCrosapiTrustedVaultBackend(
    TrustedVaultClient* client)
    : trusted_vault_client_(client) {
  CHECK(trusted_vault_client_);
  trusted_vault_client_->AddObserver(this);
}

FakeCrosapiTrustedVaultBackend::~FakeCrosapiTrustedVaultBackend() {
  trusted_vault_client_->RemoveObserver(this);
}

void FakeCrosapiTrustedVaultBackend::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

mojo::PendingRemote<crosapi::mojom::TrustedVaultBackend>
FakeCrosapiTrustedVaultBackend::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeCrosapiTrustedVaultBackend::FlushMojo() {
  receiver_.FlushForTesting();
  observer_.FlushForTesting();
}

void FakeCrosapiTrustedVaultBackend::SetPrimaryAccountInfo(
    const CoreAccountInfo& primary_account_info) {
  primary_account_info_ = primary_account_info;
}

void FakeCrosapiTrustedVaultBackend::OnTrustedVaultKeysChanged() {
  if (observer_.is_bound()) {
    observer_->OnTrustedVaultKeysChanged();
  }
}

void FakeCrosapiTrustedVaultBackend::OnTrustedVaultRecoverabilityChanged() {
  if (observer_.is_bound()) {
    observer_->OnTrustedVaultRecoverabilityChanged();
  }
}

void FakeCrosapiTrustedVaultBackend::AddObserver(
    mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendObserver> observer) {
  observer_.Bind(std::move(observer));
}

void FakeCrosapiTrustedVaultBackend::FetchKeys(
    crosapi::mojom::AccountKeyPtr account_key,
    FetchKeysCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(std::vector<std::vector<uint8_t>>());
    return;
  }
  trusted_vault_client_->FetchKeys(primary_account_info_, std::move(callback));
}

void FakeCrosapiTrustedVaultBackend::MarkLocalKeysAsStale(
    crosapi::mojom::AccountKeyPtr account_key,
    MarkLocalKeysAsStaleCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(false);
    return;
  }
  trusted_vault_client_->MarkLocalKeysAsStale(primary_account_info_,
                                              std::move(callback));
}

void FakeCrosapiTrustedVaultBackend::StoreKeys(
    crosapi::mojom::AccountKeyPtr account_key,
    const std::vector<std::vector<uint8_t>>& keys,
    int32_t last_key_version) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    return;
  }
  trusted_vault_client_->StoreKeys(primary_account_info_.gaia, keys,
                                   last_key_version);
}

void FakeCrosapiTrustedVaultBackend::GetIsRecoverabilityDegraded(
    crosapi::mojom::AccountKeyPtr account_key,
    GetIsRecoverabilityDegradedCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(false);
    return;
  }
  trusted_vault_client_->GetIsRecoverabilityDegraded(primary_account_info_,
                                                     std::move(callback));
}

void FakeCrosapiTrustedVaultBackend::AddTrustedRecoveryMethod(
    crosapi::mojom::AccountKeyPtr account_key,
    const std::vector<uint8_t>& public_key,
    int32_t method_type_hint,
    AddTrustedRecoveryMethodCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run();
    return;
  }
  trusted_vault_client_->AddTrustedRecoveryMethod(primary_account_info_.gaia,
                                                  public_key, method_type_hint,
                                                  std::move(callback));
}

void FakeCrosapiTrustedVaultBackend::ClearLocalDataForAccount(
    crosapi::mojom::AccountKeyPtr account_key) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    return;
  }
  trusted_vault_client_->ClearLocalDataForAccount(primary_account_info_);
}

bool FakeCrosapiTrustedVaultBackend::ValidateAccountKeyIsPrimaryAccount(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const std::optional<account_manager::AccountKey> account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  return account_key.has_value() &&
         account_key->account_type() == account_manager::AccountType::kGaia &&
         account_key->id() == primary_account_info_.gaia;
}

}  // namespace trusted_vault
