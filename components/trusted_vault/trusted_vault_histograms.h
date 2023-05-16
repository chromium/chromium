// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_

#include "components/sync/service/trusted_vault_histograms.h"

namespace trusted_vault {

// Aliases defined based on the counterparts in namespace syncer, until
// https://crbug.com/1423343 is fully resolved.
using TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA =
    syncer::TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA;
using TrustedVaultDeviceRegistrationStateForUMA =
    syncer::TrustedVaultDeviceRegistrationStateForUMA;
using TrustedVaultDeviceRegistrationOutcomeForUMA =
    syncer::TrustedVaultDeviceRegistrationOutcomeForUMA;
using TrustedVaultURLFetchReasonForUMA =
    syncer::TrustedVaultURLFetchReasonForUMA;
using TrustedVaultDownloadKeysStatusForUMA =
    syncer::TrustedVaultDownloadKeysStatusForUMA;
using TrustedVaultFileReadStatusForUMA =
    syncer::TrustedVaultFileReadStatusForUMA;

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason);

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state);

void RecordTrustedVaultDeviceRegistrationOutcome(
    TrustedVaultDeviceRegistrationOutcomeForUMA registration_outcome);

// Records url fetch response status (combined http and net error code). If
// |http_response_code| is non-zero, it will be recorded, otherwise |net_error|
// will be recorded. Either |http_status| or |net_error| must be non zero.
void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason);

// Records the outcome of trying to download keys from the server.
// |also_log_with_v1_suffix| allows the caller to determine whether the local
// device's registration is a V1 registration (that is, more reliable), which
// causes a second histogram to be logged as well.
void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status,
    bool also_log_with_v1_suffix);

// Records the outcome of verifying a device registration status, which is
// achieved by trying to download keys (without actually having the need to
// download keys), which is the reason why the same enum is used.
// |also_log_with_v1_suffix| allows the caller to determine whether the local
// device's registration is a V1 registration (that is, more reliable), which
// causes a second histogram to be logged as well.
void RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA status,
                                    bool also_log_with_v1_suffix);

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_
