// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TRUSTED_VAULT_HISTOGRAMS_H_
#define COMPONENTS_SYNC_DRIVER_TRUSTED_VAULT_HISTOGRAMS_H_

namespace syncer {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Exposed publicly for testing.
enum class TrustedVaultDeviceRegistrationStateForUMA {
  kAlreadyRegistered = 0,
  kLocalKeysAreStale = 1,
  kThrottledClientSide = 2,
  kAttemptingRegistrationWithNewKeyPair = 3,
  kAttemptingRegistrationWithExistingKeyPair = 4,
  kAttemptingRegistrationWithPersistentAuthError = 5,
  kAlreadyRegisteredV1 = 6,
  kMaxValue = kAlreadyRegisteredV1,
};

// Used to provide UMA metric breakdowns.
enum class TrustedVaultURLFetchReasonForUMA {
  kUnspecified,
  kRegisterDevice,
  kRegisterUnspecifiedAuthenticationFactor,
  kDownloadKeys,
  kDownloadIsRecoverabilityDegraded,
};

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state);

// Records url fetch response status (combined http and net error code). If
// |http_response_code| is non-zero, it will be recorded, otherwise |net_error|
// will be recorded.
void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason =
        TrustedVaultURLFetchReasonForUMA::kUnspecified);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TRUSTED_VAULT_HISTOGRAMS_H_
