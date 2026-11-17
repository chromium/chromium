// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/gpm_pin_recovery_factor_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace trusted_vault {

GpmPinRecoveryFactorImpl::GpmPinRecoveryFactorImpl(
    const CoreAccountInfo& primary_account,
    KeyStorage* key_storage,
    TrustedVaultConnection* connection,
    RecoveryKeyStoreConnection* recovery_key_store_connection) {
  CHECK(key_storage);
  // TODO(crbug.com/570170982): Store dependencies once backing storage and
  // network operations are implemented.
}

GpmPinRecoveryFactorImpl::~GpmPinRecoveryFactorImpl() = default;

void GpmPinRecoveryFactorImpl::FetchPinState(FetchPinStateCallback callback) {
  // TODO(crbug.com/570170982): Fetch GPM PIN state from SDS with local storage
  // fallback.
  base::BindPostTaskToCurrentDefault(base::BindOnce(std::move(callback),
                                                    FetchPinStatus::kError,
                                                    /*state=*/std::nullopt))
      .Run();
}

void GpmPinRecoveryFactorImpl::UploadPin(HashedPin hashed_pin,
                                         UploadPinCallback callback) {
  // TODO(crbug.com/570170982): Upload to Vault, register on SDS, and persist in
  // local storage.
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(callback), /*success=*/false))
      .Run();
}

int GpmPinRecoveryFactorImpl::RecordFailedAttempt() {
  // TODO(crbug.com/570170982): Increment and persist in local storage.
  return kMaxFailedAttempts;
}

void GpmPinRecoveryFactorImpl::ResetFailedAttempts() {
  // TODO(crbug.com/570170982): Reset `failed_attempts` to 0 in local storage.
}

}  // namespace trusted_vault
