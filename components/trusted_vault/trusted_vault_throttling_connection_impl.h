// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_IMPL_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_IMPL_H_

#include <memory>

#include "base/time/clock.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"

namespace trusted_vault {

enum class SecurityDomainId;

class TrustedVaultThrottlingConnectionImpl
    : public TrustedVaultThrottlingConnection {
 public:
  // `storage` is guaranteed to outlive this object.
  TrustedVaultThrottlingConnectionImpl(
      std::unique_ptr<TrustedVaultConnection> delegate,
      raw_ptr<StandaloneTrustedVaultStorage> storage);

  TrustedVaultThrottlingConnectionImpl(
      const TrustedVaultThrottlingConnectionImpl& other) = delete;
  TrustedVaultThrottlingConnectionImpl& operator=(
      const TrustedVaultThrottlingConnectionImpl& other) = delete;
  ~TrustedVaultThrottlingConnectionImpl() override;

  bool AreRequestsThrottled(const CoreAccountInfo& account_info) override;
  void RecordFailedRequestForThrottling(
      const CoreAccountInfo& account_info) override;

  std::unique_ptr<Request> RegisterAuthenticationFactor(
      const CoreAccountInfo& account_info,
      const MemberKeysSource& member_keys_source,
      const SecureBoxPublicKey& authentication_factor_public_key,
      AuthenticationFactorTypeAndRegistrationParams
          authentication_factor_type_and_registration_params,
      RegisterAuthenticationFactorCallback callback) override;

  std::unique_ptr<Request> RegisterLocalDeviceWithoutKeys(
      const CoreAccountInfo& account_info,
      const SecureBoxPublicKey& device_public_key,
      RegisterAuthenticationFactorCallback callback) override;

  std::unique_ptr<Request> DownloadNewKeys(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair,
      DownloadNewKeysCallback callback) override;

  std::unique_ptr<Request> DownloadIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      IsRecoverabilityDegradedCallback callback) override;

  std::unique_ptr<TrustedVaultConnection::Request>
  DownloadAuthenticationFactorsRegistrationState(
      const CoreAccountInfo& account_info,
      DownloadAuthenticationFactorsRegistrationStateCallback callback,
      base::RepeatingClosure keep_alive_callback) override;

  std::unique_ptr<Request> DownloadAuthenticationFactorsRegistrationState(
      const CoreAccountInfo& account_info,
      std::set<trusted_vault_pb::SecurityDomainMember_MemberType>
          recovery_factor_filter,
      DownloadAuthenticationFactorsRegistrationStateCallback callback,
      base::RepeatingClosure keep_alive_callback) override;

  // Specifies how long requests shouldn't be retried after encountering
  // transient error. Note, that this doesn't affect requests related to
  // degraded recoverability.
  // Exposed for testing.
  static constexpr base::TimeDelta kThrottlingDuration = base::Days(1);

  static std::unique_ptr<TrustedVaultThrottlingConnectionImpl> CreateForTesting(
      std::unique_ptr<TrustedVaultConnection> delegate,
      raw_ptr<StandaloneTrustedVaultStorage> storage,
      raw_ptr<base::Clock> clock);

 private:
  TrustedVaultThrottlingConnectionImpl(
      std::unique_ptr<TrustedVaultConnection> delegate,
      raw_ptr<StandaloneTrustedVaultStorage> storage,
      raw_ptr<base::Clock> clock);

  const std::unique_ptr<TrustedVaultConnection> delegate_;
  const raw_ptr<StandaloneTrustedVaultStorage> storage_;

  // Used to determine current time, set to base::DefaultClock in prod and can
  // be overridden in tests.
  const raw_ptr<base::Clock> clock_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_THROTTLING_CONNECTION_IMPL_H_
