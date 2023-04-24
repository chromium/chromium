// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_histograms.h"

#include "components/sync/driver/trusted_vault_histograms.h"

namespace trusted_vault {

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason) {
  syncer::RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
      hint_degraded_recoverability_changed_reason);
}

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  syncer::RecordTrustedVaultDeviceRegistrationState(registration_state);
}

void RecordTrustedVaultDeviceRegistrationOutcome(
    TrustedVaultDeviceRegistrationOutcomeForUMA registration_outcome) {
  syncer::RecordTrustedVaultDeviceRegistrationOutcome(registration_outcome);
}

void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason) {
  syncer::RecordTrustedVaultURLFetchResponse(http_response_code, net_error,
                                             reason);
}

void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status,
    bool also_log_with_v1_suffix) {
  syncer::RecordTrustedVaultDownloadKeysStatus(status, also_log_with_v1_suffix);
}

void RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA status,
                                    bool also_log_with_v1_suffix) {
  syncer::RecordVerifyRegistrationStatus(status, also_log_with_v1_suffix);
}

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status) {
  syncer::RecordTrustedVaultFileReadStatus(status);
}

}  // namespace trusted_vault
