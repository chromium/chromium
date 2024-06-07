// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_SERVICE_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_SERVICE_H_

#include <memory>

#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace trusted_vault {

// Fake implementation of TrustedVaultBackendService mojo interface, that allows
// to test Lacros counterpart of counterpart (real implementation lives in Ash).
class FakeCrosapiTrustedVaultBackendService
    : public crosapi::mojom::TrustedVaultBackendService {
 public:
  FakeCrosapiTrustedVaultBackendService(
      TrustedVaultClient* chrome_sync_trusted_vault_client,
      TrustedVaultClient* passkeys_trusted_vault_client);
  ~FakeCrosapiTrustedVaultBackendService() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackendService>
          receiver);
  mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendService>
  BindNewPipeAndPassRemote();

  FakeCrosapiTrustedVaultBackend& chrome_sync_backend();
  FakeCrosapiTrustedVaultBackend& passkeys_backend();

  // crosapi::mojom::TrustedVaultBackendService:
  void GetTrustedVaultBackend(
      crosapi::mojom::SecurityDomainId security_domain,
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> backend)
      override;

 private:
  std::unique_ptr<trusted_vault::FakeCrosapiTrustedVaultBackend>
      chrome_sync_trusted_vault_backend_;
  std::unique_ptr<trusted_vault::FakeCrosapiTrustedVaultBackend>
      passkeys_trusted_vault_backend_;

  mojo::Receiver<crosapi::mojom::TrustedVaultBackendService> receiver_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_CROSAPI_TRUSTED_VAULT_BACKEND_SERVICE_H_
