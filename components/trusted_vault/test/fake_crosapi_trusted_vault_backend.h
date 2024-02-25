// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_H_

#include "chromeos/crosapi/mojom/account_manager.mojom-forward.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace trusted_vault {

// Fake implementation of TrustedVaultBackend mojo interface, that allows to
// test Lacros counterpart of counterpart (real implementation lives in Ash).
class FakeCrosapiTrustedVaultBackend
    : public crosapi::mojom::TrustedVaultBackend,
      public TrustedVaultClient::Observer {
 public:
  explicit FakeCrosapiTrustedVaultBackend(
      TrustedVaultClient* client);
  ~FakeCrosapiTrustedVaultBackend() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> receiver);
  mojo::PendingRemote<crosapi::mojom::TrustedVaultBackend>
  BindNewPipeAndPassRemote();
  void FlushMojo();
  void SetPrimaryAccountInfo(const CoreAccountInfo& primary_account_info);

  // TrustedVaultClient::Observer implementation.
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

  // crosapi::mojom::TrustedVaultBackend implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendObserver> observer)
      override;
  void FetchKeys(crosapi::mojom::AccountKeyPtr account_key,
                 FetchKeysCallback callback) override;
  void MarkLocalKeysAsStale(crosapi::mojom::AccountKeyPtr account_key,
                            MarkLocalKeysAsStaleCallback callback) override;
  void StoreKeys(crosapi::mojom::AccountKeyPtr account_key,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int32_t last_key_version) override;
  void GetIsRecoverabilityDegraded(
      crosapi::mojom::AccountKeyPtr account_key,
      GetIsRecoverabilityDegradedCallback callback) override;
  void AddTrustedRecoveryMethod(
      crosapi::mojom::AccountKeyPtr account_key,
      const std::vector<uint8_t>& public_key,
      int32_t method_type_hint,
      AddTrustedRecoveryMethodCallback callback) override;
  void ClearLocalDataForAccount(
      crosapi::mojom::AccountKeyPtr account_key) override;

 private:
  bool ValidateAccountKeyIsPrimaryAccount(
      const crosapi::mojom::AccountKeyPtr& account_key) const;

  CoreAccountInfo primary_account_info_;
  const raw_ptr<TrustedVaultClient> trusted_vault_client_;

  mojo::Receiver<crosapi::mojom::TrustedVaultBackend> receiver_{this};
  mojo::Remote<crosapi::mojom::TrustedVaultBackendObserver> observer_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_H_
