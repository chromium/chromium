// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/metrics_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace cellular_setup {
namespace metrics {

void LogInstallViaQrCodeResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.InstallViaQrCode.Result",
                                status);
}

void LogInstallPendingProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.InstallPendingProfile.Result", status);
}

}  // namespace metrics
}  // namespace cellular_setup
}  // namespace chromeos
