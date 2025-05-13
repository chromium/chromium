// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"

#include <cstddef>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/securebox.h"

namespace trusted_vault {

TrustedVaultThrottlingConnectionImpl::TrustedVaultThrottlingConnectionImpl(
    std::unique_ptr<TrustedVaultConnection> delegate,
    raw_ptr<StandaloneTrustedVaultStorage> storage)
    : TrustedVaultThrottlingConnectionImpl(std::move(delegate),
                                           storage,
                                           base::DefaultClock::GetInstance()) {}

TrustedVaultThrottlingConnectionImpl::TrustedVaultThrottlingConnectionImpl(
    std::unique_ptr<TrustedVaultConnection> delegate,
    raw_ptr<StandaloneTrustedVaultStorage> storage,
    raw_ptr<base::Clock> clock)
    : delegate_(std::move(delegate)), storage_(storage), clock_(clock) {
  CHECK(delegate_);
  CHECK(storage_);
  CHECK(clock_);
}

// static
std::unique_ptr<TrustedVaultThrottlingConnectionImpl>
TrustedVaultThrottlingConnectionImpl::CreateForTesting(
    std::unique_ptr<TrustedVaultConnection> delegate,
    raw_ptr<StandaloneTrustedVaultStorage> storage,
    raw_ptr<base::Clock> clock) {
  return base::WrapUnique(new TrustedVaultThrottlingConnectionImpl(
      std::move(delegate), storage, clock));
}

TrustedVaultThrottlingConnectionImpl::~TrustedVaultThrottlingConnectionImpl() =
    default;

bool TrustedVaultThrottlingConnectionImpl::AreRequestsThrottled(
    const CoreAccountInfo& account_info) {
  auto* per_user_vault = storage_->FindUserVault(account_info.gaia);
  CHECK(per_user_vault);

  const base::Time current_time = clock_->Now();
  base::Time last_failed_request_time = ProtoTimeToTime(
      per_user_vault->last_failed_request_millis_since_unix_epoch());

  // Fix |last_failed_request_time| if it's set to the future.
  if (last_failed_request_time > current_time) {
    // Immediately unthrottle, but don't write new state to the file.
    last_failed_request_time = base::Time();
  }

  return last_failed_request_time + kThrottlingDuration > current_time;
}

void TrustedVaultThrottlingConnectionImpl::RecordFailedRequestForThrottling(
    const CoreAccountInfo& account_info) {
  auto* per_user_vault = storage_->FindUserVault(account_info.gaia);
  CHECK(per_user_vault);

  per_user_vault->set_last_failed_request_millis_since_unix_epoch(
      TimeToProtoTime(clock_->Now()));
  storage_->WriteDataToDisk();
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::RegisterAuthenticationFactor(
    const CoreAccountInfo& account_info,
    const MemberKeysSource& member_keys_source,
    const SecureBoxPublicKey& authentication_factor_public_key,
    AuthenticationFactorTypeAndRegistrationParams
        authentication_factor_type_and_registration_params,
    RegisterAuthenticationFactorCallback callback) {
  return delegate_->RegisterAuthenticationFactor(
      account_info, member_keys_source, authentication_factor_public_key,
      authentication_factor_type_and_registration_params, std::move(callback));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::RegisterLocalDeviceWithoutKeys(
    const CoreAccountInfo& account_info,
    const SecureBoxPublicKey& device_public_key,
    RegisterAuthenticationFactorCallback callback) {
  return delegate_->RegisterLocalDeviceWithoutKeys(
      account_info, device_public_key, std::move(callback));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::DownloadNewKeys(
    const CoreAccountInfo& account_info,
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadNewKeysCallback callback) {
  return delegate_->DownloadNewKeys(
      account_info, last_trusted_vault_key_and_version,
      std::move(device_key_pair), std::move(callback));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::DownloadIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    IsRecoverabilityDegradedCallback callback) {
  return delegate_->DownloadIsRecoverabilityDegraded(account_info,
                                                     std::move(callback));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::
    DownloadAuthenticationFactorsRegistrationState(
        const CoreAccountInfo& account_info,
        DownloadAuthenticationFactorsRegistrationStateCallback callback,
        base::RepeatingClosure keep_alive_callback) {
  return delegate_->DownloadAuthenticationFactorsRegistrationState(
      account_info, std::move(callback), keep_alive_callback);
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultThrottlingConnectionImpl::
    DownloadAuthenticationFactorsRegistrationState(
        const CoreAccountInfo& account_info,
        std::set<trusted_vault_pb::SecurityDomainMember_MemberType>
            recovery_factor_filter,
        DownloadAuthenticationFactorsRegistrationStateCallback callback,
        base::RepeatingClosure keep_alive_callback) {
  return delegate_->DownloadAuthenticationFactorsRegistrationState(
      account_info, recovery_factor_filter, std::move(callback),
      keep_alive_callback);
}

}  // namespace trusted_vault
