// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_H_

#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {

enum class SecurityDomainId;

// Extends the `TrustedVaultConnection` interface by client side throttling.
class TrustedVaultThrottlingConnection : public TrustedVaultConnection {
 public:
  TrustedVaultThrottlingConnection() = default;
  TrustedVaultThrottlingConnection(
      const TrustedVaultThrottlingConnection& other) = delete;
  TrustedVaultThrottlingConnection& operator=(
      const TrustedVaultThrottlingConnection& other) = delete;
  ~TrustedVaultThrottlingConnection() override = default;

  // Returns true if the last failed request time implies that upcoming requests
  // should be throttled now (certain amount of time should pass since the last
  // failed request). Handles the situation when last failed request time is
  // set to the future.
  //
  // Note: It's the client's responsibility to not make any requests to this
  // connection if this method returns true. Such requests would not be blocked.
  virtual bool AreRequestsThrottled(const CoreAccountInfo& account_info,
                                    SecurityDomainId domain) = 0;
  // Records domain-scoped request failure time for throttling.
  virtual void RecordFailedRequestForThrottling(
      const CoreAccountInfo& account_info,
      SecurityDomainId domain) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_H_
