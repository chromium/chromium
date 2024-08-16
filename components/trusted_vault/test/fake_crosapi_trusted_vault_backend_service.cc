// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend_service.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"

namespace trusted_vault {

FakeCrosapiTrustedVaultBackendService::FakeCrosapiTrustedVaultBackendService(
    TrustedVaultClient* chrome_sync_trusted_vault_client,
    TrustedVaultClient* passkeys_trusted_vault_client)
    : chrome_sync_trusted_vault_backend_(
          std::make_unique<trusted_vault::FakeCrosapiTrustedVaultBackend>(
              chrome_sync_trusted_vault_client)),
      passkeys_trusted_vault_backend_(
          std::make_unique<trusted_vault::FakeCrosapiTrustedVaultBackend>(
              passkeys_trusted_vault_client)) {}

FakeCrosapiTrustedVaultBackendService::
    ~FakeCrosapiTrustedVaultBackendService() = default;

void FakeCrosapiTrustedVaultBackendService::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackendService>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendService>
FakeCrosapiTrustedVaultBackendService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

FakeCrosapiTrustedVaultBackend&
FakeCrosapiTrustedVaultBackendService::chrome_sync_backend() {
  return *chrome_sync_trusted_vault_backend_;
}

FakeCrosapiTrustedVaultBackend&
FakeCrosapiTrustedVaultBackendService::passkeys_backend() {
  return *passkeys_trusted_vault_backend_;
}

void FakeCrosapiTrustedVaultBackendService::GetTrustedVaultBackend(
    crosapi::mojom::SecurityDomainId security_domain,
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> backend) {
  switch (security_domain) {
    case crosapi::mojom::SecurityDomainId::kUnknown:
      NOTREACHED();
    case crosapi::mojom::SecurityDomainId::kChromeSync:
      chrome_sync_trusted_vault_backend_->BindReceiver(std::move(backend));
      break;
    case crosapi::mojom::SecurityDomainId::kPasskeys:
      passkeys_trusted_vault_backend_->BindReceiver(std::move(backend));
      break;
  }
}

}  // namespace trusted_vault
