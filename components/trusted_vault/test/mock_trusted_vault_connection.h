// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_MOCK_TRUSTED_VAULT_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_MOCK_TRUSTED_VAULT_CONNECTION_H_

#include <memory>

#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace trusted_vault {

class MockTrustedVaultConnection : public TrustedVaultConnection {
 public:
  MockTrustedVaultConnection();
  ~MockTrustedVaultConnection() override;
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterAuthenticationFactor,
              (const CoreAccountInfo& account_info,
               const MemberKeysSource& member_keys_source,
               const SecureBoxPublicKey& authentication_factor_public_key,
               AuthenticationFactorType authentication_factor_type,
               RegisterAuthenticationFactorCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterLocalDeviceWithoutKeys,
              (const CoreAccountInfo& account_info,
               const SecureBoxPublicKey& device_public_key,
               RegisterAuthenticationFactorCallback callback),
              (override));
  MOCK_METHOD(
      std::unique_ptr<Request>,
      DownloadNewKeys,
      (const CoreAccountInfo& account_info,
       const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
       std::unique_ptr<SecureBoxKeyPair> device_key_pair,
       DownloadNewKeysCallback callback),
      (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadIsRecoverabilityDegraded,
              (const CoreAccountInfo& account_info,
               IsRecoverabilityDegradedCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadAuthenticationFactorsRegistrationState,
              (const CoreAccountInfo& account_info,
               DownloadAuthenticationFactorsRegistrationStateCallback callback),
              (override));
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_MOCK_TRUSTED_VAULT_CONNECTION_H_
