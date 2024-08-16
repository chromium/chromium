// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_service.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace trusted_vault {

TrustedVaultService::TrustedVaultService(
    std::unique_ptr<TrustedVaultClient> chrome_sync_security_domain_client)
    : chrome_sync_security_domain_client_(
          std::move(chrome_sync_security_domain_client)) {
  CHECK(chrome_sync_security_domain_client_);
}

#if BUILDFLAG(IS_CHROMEOS)
TrustedVaultService::TrustedVaultService(
    std::unique_ptr<TrustedVaultClient> chrome_sync_security_domain_client,
    std::unique_ptr<TrustedVaultClient> passkeys_security_domain_client)
    : chrome_sync_security_domain_client_(
          std::move(chrome_sync_security_domain_client)),
      passkeys_security_domain_client_(
          std::move(passkeys_security_domain_client)) {
  CHECK(chrome_sync_security_domain_client_);
  // `passkeys_security_domain_client` will be null if the passkeys feature is
  // disabled.
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TrustedVaultService::~TrustedVaultService() = default;

trusted_vault::TrustedVaultClient* TrustedVaultService::GetTrustedVaultClient(
    SecurityDomainId security_domain) {
  switch (security_domain) {
    case SecurityDomainId::kChromeSync:
      return chrome_sync_security_domain_client_.get();
    case SecurityDomainId::kPasskeys:
#if BUILDFLAG(IS_CHROMEOS)
      return passkeys_security_domain_client_.get();
#else
      return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  NOTREACHED();
}

}  // namespace trusted_vault
