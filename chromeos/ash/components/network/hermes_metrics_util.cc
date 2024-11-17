// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hermes_metrics_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"

namespace ash::hermes_metrics {

void LogInstallViaQrCodeResult(HermesResponseStatus status,
                               dbus::DBusResult dbusResult,
                               bool is_initial_install) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.InstallViaQrCode.Result",
                                status);
  if (is_initial_install) {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.InstallViaQrCode.Result.InitialAttempt", status);
  } else {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.InstallViaQrCode.Result.Retry", status);
  }

  if (status == HermesResponseStatus::kErrorUnknownResponse) {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.InstallViaQrCode.DBusResult", dbusResult);
  }

  if (status == HermesResponseStatus::kSuccess ||
      !base::Contains(kHermesUserErrorCodes, status)) {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.Installation.NonUserErrorSuccessRate", status);
  }
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

void LogRefreshInstalledProfilesLatency(base::TimeDelta call_latency) {
  UMA_HISTOGRAM_LONG_TIMES(
      "Network.Cellular.ESim.RefreshInstalledProfilesLatency", call_latency);
}

}  // namespace ash::hermes_metrics
