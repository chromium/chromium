// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace trusted_vault {

namespace {

std::string GetReasonSuffix(TrustedVaultURLFetchReasonForUMA reason) {
  switch (reason) {
    case TrustedVaultURLFetchReasonForUMA::kUnspecified:
      return std::string();
    case TrustedVaultURLFetchReasonForUMA::kRegisterDevice:
      return "RegisterDevice";
    case TrustedVaultURLFetchReasonForUMA::
        kRegisterUnspecifiedAuthenticationFactor:
      return "RegisterUnspecifiedAuthenticationFactor";
    case TrustedVaultURLFetchReasonForUMA::kDownloadKeys:
      return "DownloadKeys";
    case TrustedVaultURLFetchReasonForUMA::kDownloadIsRecoverabilityDegraded:
      return "DownloadIsRecoverabilityDegraded";
  }
}

}  // namespace

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason) {
  // TODO(crbug.com/1423343): eventually histograms under
  // components/trusted_vault should start using their own prefix instead of
  // "Sync." and migrated to the dedicated histograms.xml file.
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultHintDegradedRecoverabilityChangedReason2",
      hint_degraded_recoverability_changed_reason);
}

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationState",
                                registration_state);
}

void RecordTrustedVaultDeviceRegistrationOutcome(
    TrustedVaultDeviceRegistrationOutcomeForUMA registration_outcome) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationOutcome",
                                registration_outcome);
}

void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason) {
  DCHECK_LE(net_error, 0);
  DCHECK_GE(http_response_code, 0);

  const int value = http_response_code == 0 ? net_error : http_response_code;
  const std::string suffix = GetReasonSuffix(reason);

  base::UmaHistogramSparse("Sync.TrustedVaultURLFetchResponse", value);

  if (!suffix.empty()) {
    base::UmaHistogramSparse(
        base::StrCat({"Sync.TrustedVaultURLFetchResponse", ".", suffix}),
        value);
  }
}

void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status,
    bool also_log_with_v1_suffix) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDownloadKeysStatus", status);
  if (also_log_with_v1_suffix) {
    base::UmaHistogramEnumeration("Sync.TrustedVaultDownloadKeysStatusV1",
                                  status);
  }
}

void RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA status,
                                    bool also_log_with_v1_suffix) {
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultVerifyDeviceRegistrationState", status);

  if (also_log_with_v1_suffix) {
    base::UmaHistogramEnumeration(
        "Sync.TrustedVaultVerifyDeviceRegistrationStateV1", status);
  }
}

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultFileReadStatus", status);
}

}  // namespace trusted_vault
