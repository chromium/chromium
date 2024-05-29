// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_

#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"

namespace ash::nearby::presence::metrics {

void RecordSharedCredentialUploadAttemptFailureReason(
    ash::nearby::NearbyHttpResult failure_reason);
void RecordSharedCredentialUploadTotalAttemptsNeededCount(int attempt_count);
void RecordSharedCredentialUploadResult(bool success);
void RecordSharedCredentialUploadDuration(base::TimeDelta upload_duration);
void RecordSharedCredentialDownloadFailureReason(
    ash::nearby::NearbyHttpResult failure_reason);
void RecordSharedCredentialDownloadTotalAttemptsNeededCount(int attempt_count);
void RecordSharedCredentialDownloadResult(bool success);
void RecordSharedCredentialDownloadDuration(base::TimeDelta download_duration);
void RecordFirstTimeRegistrationFlowResult(bool success);
void RecordFirstTimeServerRegistrationFailureReason(
    ash::nearby::NearbyHttpResult failure_reason);
void RecordFirstTimeServerRegistrationTotalAttemptsNeededCount(
    int attempt_count);
void RecordFirstTimeServerRegistrationDuration(
    base::TimeDelta registration_duration);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// the NearbyPresenceFirstTimeRegistrationResult enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class FirstTimeRegistrationResult {
  kSuccess = 0,
  kRegistrationWithServerFailure = 1,
  kLocalCredentialGenerationFailure = 2,
  kUploadLocalCredentialsFailure = 3,
  kDownloadRemoteCredentialsFailure = 4,
  kSaveRemoteCredentialsFailure = 5,
  kMaxValue = kSaveRemoteCredentialsFailure,
};
void RecordFirstTimeRegistrationFlowResult(FirstTimeRegistrationResult result);
void RecordFirstTimeServerRegistrationFailureReason(
    ash::nearby::NearbyHttpResult failure_reason);
void RecordFirstTimeServerRegistrationTotalAttemptsNeededCount(
    int attempt_count);
void RecordScanRequestResult(enums::StatusCode result);
void RecordDeviceFoundLatency(base::TimeDelta device_found_latency);
void RecordNearbyProcessShutdownReason(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
        shutdown_reason);

}  // namespace ash::nearby::presence::metrics

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_
