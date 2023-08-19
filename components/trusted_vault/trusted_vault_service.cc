// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_service.h"

#include <memory>
#include <utility>

#include "components/trusted_vault/trusted_vault_client.h"

namespace trusted_vault {

TrustedVaultService::TrustedVaultService(
    std::unique_ptr<TrustedVaultClient> chrome_sync_security_domain_client)
    : chrome_sync_security_domain_client_(
          std::move(chrome_sync_security_domain_client)) {
  CHECK(chrome_sync_security_domain_client_);
}

TrustedVaultService::~TrustedVaultService() = default;

trusted_vault::TrustedVaultClient*
TrustedVaultService::GetTrustedVaultClient() {
  return chrome_sync_security_domain_client_.get();
}

}  // namespace trusted_vault
