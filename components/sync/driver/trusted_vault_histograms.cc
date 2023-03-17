// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

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

void RecordTrustedVaultHistogramBooleanWithMigrationSuffix(
    const std::string& histogram_name,
    bool sample,
    const SyncStatus& sync_status) {
  base::UmaHistogramBoolean(histogram_name, sample);

  // Note that the proto field may be unset: in this case the migration time
  // will point to the Unix epoch and will be later ignored.
  const base::Time migration_time =
      ProtoTimeToTime(sync_status.trusted_vault_debug_info.migration_time());
  const base::TimeDelta time_delta_since_migration =
      base::Time::Now() - migration_time;

  if (time_delta_since_migration.is_negative()) {
    return;
  }

  if (time_delta_since_migration < base::Days(180)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast180Days", sample);
  }

  if (time_delta_since_migration < base::Days(90)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast90Days", sample);
  }

  if (time_delta_since_migration < base::Days(28)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast28Days", sample);
  }

  if (time_delta_since_migration < base::Days(7)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast7Days", sample);
  }

  if (time_delta_since_migration < base::Days(3)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast3Days", sample);
  }

  if (time_delta_since_migration < base::Days(1)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLastDay", sample);
  }
}

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultFileReadStatus", status);
}

}  // namespace syncer
