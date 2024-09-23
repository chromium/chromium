// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/metrics/nearby_presence_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash::nearby::presence::metrics {

void RecordSharedCredentialUploadAttemptFailureReason(
    ash::nearby::NearbyHttpResult failure_reason) {
  base::UmaHistogramEnumeration(
      "Nearby.Presence.Credentials.Upload.FailureReason", failure_reason);
}

void RecordSharedCredentialUploadTotalAttemptsNeededCount(int attempt_count) {
  base::UmaHistogramExactLinear(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount", attempt_count,
      /*exclusive_max=*/10);
}

void RecordSharedCredentialUploadResult(bool success) {
  base::UmaHistogramBoolean("Nearby.Presence.Credentials.Upload.Result",
                            success);
}

void RecordSharedCredentialUploadDuration(base::TimeDelta upload_duration) {
  base::UmaHistogramTimes(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration",
      upload_duration);
}

void RecordSharedCredentialDownloadFailureReason(
    ash::nearby::NearbyHttpResult failure_reason) {
  base::UmaHistogramEnumeration(
      "Nearby.Presence.Credentials.Download.FailureReason", failure_reason);
}

void RecordSharedCredentialDownloadTotalAttemptsNeededCount(int attempt_count) {
  base::UmaHistogramExactLinear(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount", attempt_count,
      /*exclusive_max=*/10);
}

void RecordSharedCredentialDownloadResult(bool success) {
  base::UmaHistogramBoolean("Nearby.Presence.Credentials.Download.Result",
                            success);
}

void RecordFirstTimeRegistrationFlowResult(FirstTimeRegistrationResult result) {
  base::UmaHistogramEnumeration(
      "Nearby.Presence.Credentials.FirstTimeRegistration.Result", result);
}

void RecordSharedCredentialDownloadDuration(base::TimeDelta download_duration) {
  base::UmaHistogramTimes(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration",
      download_duration);
}

void RecordFirstTimeServerRegistrationFailureReason(
    ash::nearby::NearbyHttpResult failure_reason) {
  base::UmaHistogramEnumeration(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration.FailureReason",
      failure_reason);
}

void RecordFirstTimeServerRegistrationTotalAttemptsNeededCount(
    int attempt_count) {
  base::UmaHistogramExactLinear(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "AttemptsNeededCount",
      attempt_count,
      /*exclusive_max=*/10);
}

void RecordFirstTimeServerRegistrationDuration(
    base::TimeDelta registration_duration) {
  base::UmaHistogramTimes(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "ServerRequestDuration",
      registration_duration);
}

void RecordScanRequestResult(enums::StatusCode result) {
  base::UmaHistogramEnumeration("Nearby.Presence.ScanRequest.Result", result);
}

void RecordDeviceFoundLatency(base::TimeDelta device_found_latency) {
  base::UmaHistogramTimes("Nearby.Presence.DeviceFound.Latency",
                          device_found_latency);
}

void RecordNearbyProcessShutdownReason(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
        shutdown_reason) {
  base::UmaHistogramEnumeration("Nearby.Presence.Process.Shutdown.Reason",
                                shutdown_reason);
}

}  // namespace ash::nearby::presence::metrics
