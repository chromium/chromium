// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

namespace ash {

// Provides the trusted_vault::TrustedVaultService associated with a user to
// ChromeOS callers without forcing them to depend on
// //chrome/browser/trusted_vault's Profile-keyed TrustedVaultServiceFactory.
// The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// trusted_vault_service_provider_impl.h).
class COMPONENT_EXPORT(TRUSTED_VAULT_SERVICE_PROVIDER)
    TrustedVaultServiceProvider {
 public:
  TrustedVaultServiceProvider();
  TrustedVaultServiceProvider(const TrustedVaultServiceProvider&) = delete;
  TrustedVaultServiceProvider& operator=(const TrustedVaultServiceProvider&) =
      delete;
  virtual ~TrustedVaultServiceProvider();

  // Returns the process-wide singleton.
  static TrustedVaultServiceProvider& Get();

  // Returns the TrustedVaultService associated with `account_id`, or nullptr if
  // none is available. The returned pointer is owned by the
  // BrowserContext-keyed service infrastructure; callers must not delete it.
  virtual trusted_vault::TrustedVaultService* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_PROVIDER_H_
