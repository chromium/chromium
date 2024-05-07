// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/nearby_platform_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace nearby::chrome::metrics {

void RecordGattServerScatternetDualRoleSupported(bool is_dual_role_supported) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.ScatternetDualRoleSupported",
      is_dual_role_supported);
}

}  // namespace nearby::chrome::metrics
