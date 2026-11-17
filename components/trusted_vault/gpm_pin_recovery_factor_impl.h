// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_IMPL_H_
#define COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_IMPL_H_

#include "components/trusted_vault/gpm_pin_recovery_factor.h"

struct CoreAccountInfo;

namespace trusted_vault {

class KeyStorage;
class RecoveryKeyStoreConnection;
class TrustedVaultConnection;

// Production implementation of `GpmPinRecoveryFactor` used on
// `StandaloneTrustedVaultBackend`'s sequence.
//
// TODO(crbug.com/570170982): Implement local storage backing, SDS sync, Vault
// upload, and periodic Vault cohort renewal. Until then every operation is a
// fail-closed stub.
class GpmPinRecoveryFactorImpl : public GpmPinRecoveryFactor {
 public:
  // `key_storage` must not be null and must outlive this object. `connection`
  // and `recovery_key_store_connection` may be null if network operations are
  // disabled; if non-null, they must outlive this object.
  GpmPinRecoveryFactorImpl(
      const CoreAccountInfo& primary_account,
      KeyStorage* key_storage,
      TrustedVaultConnection* connection,
      RecoveryKeyStoreConnection* recovery_key_store_connection);
  GpmPinRecoveryFactorImpl(const GpmPinRecoveryFactorImpl&) = delete;
  GpmPinRecoveryFactorImpl& operator=(const GpmPinRecoveryFactorImpl&) = delete;
  ~GpmPinRecoveryFactorImpl() override;

  // GpmPinRecoveryFactor:
  void FetchPinState(FetchPinStateCallback callback) override;
  void UploadPin(HashedPin hashed_pin, UploadPinCallback callback) override;
  int RecordFailedAttempt() override;
  void ResetFailedAttempts() override;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_IMPL_H_
