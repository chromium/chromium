// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_ENUMS_NEARBY_PRESENCE_ENUMS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_ENUMS_NEARBY_PRESENCE_ENUMS_H_

#include "mojo/public/mojom/base/absl_status.mojom.h"

namespace ash::nearby::presence::enums {

// This is a super set of the absl status code found in
// //mojo/public/mojom/base/absl_status.mojom with the only difference being
// the addition of kFailedToStartProcess. Any updates to absl_status should be
// reflected here. This enum should also be kept in sync with the
// NearbyPresenceScanRequestResult enum found in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class StatusCode {
  kAbslOk = 0,
  kAbslCancelled = 1,
  kAbslUnknown = 2,
  kAbslInvalidArgument = 3,
  kAbslDeadlineExceeded = 4,
  kAbslNotFound = 5,
  kAbslAlreadyExists = 6,
  kAbslPermissionDenied = 7,
  kAbslResourceExhausted = 8,
  kAbslFailedPrecondition = 9,
  kAbslAborted = 10,
  kAbslOutOfRange = 11,
  kAbslUnimplemented = 12,
  kAbslInternal = 13,
  kAbslUnavailable = 14,
  kAbslDataLoss = 15,
  kAbslUnauthenticated = 16,
  kFailedToStartProcess = 17,
  kMaxValue = kFailedToStartProcess,
};

StatusCode ConvertToPresenceStatus(
    mojo_base::mojom::AbslStatusCode status_code);

}  // namespace ash::nearby::presence::enums

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_ENUMS_NEARBY_PRESENCE_ENUMS_H_
