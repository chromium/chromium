// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace trusted_vault {

class TrustedVaultService : public KeyedService {
 public:
  explicit TrustedVaultService(
      std::unique_ptr<TrustedVaultClient> chrome_sync_security_domain_client);

  TrustedVaultService(const TrustedVaultService&) = delete;
  TrustedVaultService& operator=(const TrustedVaultService&) = delete;

  ~TrustedVaultService() override;

  // TODO(crbug.com/1434661): bind TrustedVaultClient interface to the specific
  // security domain and allow passing a security domain here.
  TrustedVaultClient* GetTrustedVaultClient();

 private:
  std::unique_ptr<TrustedVaultClient> chrome_sync_security_domain_client_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_H_
