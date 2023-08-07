// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_

#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"

namespace ash::nearby::presence::metrics {

void RecordSharedCredentialUploadAttemptFailureReason(
    ash::nearby::NearbyHttpResult failure_reason);
void RecordSharedCredentialUploadTotalAttemptsNeededCount(int attempt_count);
void RecordSharedCredentialUploadResult(bool success);

}  // namespace ash::nearby::presence::metrics

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_METRICS_NEARBY_PRESENCE_METRICS_H_
