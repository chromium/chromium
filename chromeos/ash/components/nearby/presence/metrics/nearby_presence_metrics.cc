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

}  // namespace ash::nearby::presence::metrics
