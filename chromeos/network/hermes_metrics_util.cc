// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/hermes_metrics_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace hermes_metrics {

void LogInstallViaQrCodeResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.InstallViaQrCode.Result",
                                status);
}

void LogInstallPendingProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.InstallPendingProfile.Result", status);
}

void LogEnableProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.EnableProfile.Result",
                                status);
}

void LogDisableProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.DisableProfile.Result",
                                status);
}

void LogUninstallProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.UninstallProfile.Result",
                                status);
}

void LogRequestPendingProfilesResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.RequestPendingProfiles.Result", status);
}

}  // namespace hermes_metrics
}  // namespace chromeos
