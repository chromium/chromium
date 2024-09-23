// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/fwupd/histogram_util.h"

#include <string>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

const char kHistogramName[] = "ChromeOS.FirmwareUpdateUi.";

}  // namespace
namespace ash::firmware_update::metrics {

void EmitDeviceCount(int num_devices, bool is_startup) {
  base::UmaHistogramCounts100(
      GetSourceStr(is_startup) + std::string(".DeviceCount"), num_devices);
}

void EmitUpdateCount(int num_updates,
                     int num_critical_updates,
                     bool is_startup) {
  const auto source_str = GetSourceStr(is_startup);
  base::UmaHistogramCounts100(
      base::StrCat({source_str, ".CriticalUpdateCount"}), num_critical_updates);
  base::UmaHistogramCounts100(
      base::StrCat({source_str, ".NonCriticalUpdateCount"}),
      num_updates - num_critical_updates);
}

void EmitInstallFailedWithStatus(FwupdStatus last_fwupd_status) {
  base::UmaHistogramEnumeration(
      "ChromeOS.FirmwareUpdateUi.InstallFailedWithStatus", last_fwupd_status);
}

void EmitInstallResult(MethodResult result) {
  base::UmaHistogramSparse("ChromeOS.FirmwareUpdateUi.InstallResult",
                           static_cast<int>(result));
}

void EmitRefreshRemoteResult(MethodResult result) {
  base::UmaHistogramSparse("ChromeOS.FirmwareUpdateUi.RefreshRemoteResult",
                           static_cast<int>(result));
}

void EmitDeviceRequest(firmware_update::mojom::DeviceRequestPtr request) {
  std::string kind_string = "Unknown";
  if (request->kind == mojom::DeviceRequestKind::kImmediate) {
    kind_string = "Immediate";
  } else if (request->kind == mojom::DeviceRequestKind::kPost) {
    kind_string = "Post";
  }
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ChromeOS.FirmwareUpdateUi.RequestReceived.Kind", kind_string}),
      request->id);
}

void EmitFailedDeviceRequestDuration(const base::TimeDelta& request_duration,
                                     mojom::DeviceRequestId request_id) {
  base::UmaHistogramLongTimes100(
      base::StrCat({"ChromeOS.FirmwareUpdateUi."
                    "InstallFailedWithDurationAfterRequest.RequestId",
                    GetRequestIdString(request_id)}),
      request_duration);
}

void EmitDeviceRequestSuccessfulWithDuration(
    const base::TimeDelta& request_duration,
    mojom::DeviceRequestId request_id) {
  base::UmaHistogramLongTimes100(
      base::StrCat(
          {"ChromeOS.FirmwareUpdateUi.RequestSucceededWithDuration.RequestId",
           GetRequestIdString(request_id)}),
      request_duration);
}

std::string GetSourceStr(bool is_startup) {
  return std::string(kHistogramName) +
         std::string(is_startup ? "OnStartup" : "OnRefresh");
}

std::string GetRequestIdString(mojom::DeviceRequestId request_id) {
  switch (request_id) {
    case mojom::DeviceRequestId::kDoNotPowerOff:
      return "DoNotPowerOff";
    case mojom::DeviceRequestId::kReplugInstall:
      return "ReplugInstall";
    case mojom::DeviceRequestId::kInsertUSBCable:
      return "InsertUSBCable";
    case mojom::DeviceRequestId::kRemoveUSBCable:
      return "RemoveUSBCable";
    case mojom::DeviceRequestId::kPressUnlock:
      return "PressUnlock";
    case mojom::DeviceRequestId::kRemoveReplug:
      return "RemoveReplug";
    default:
      return "Unknown";
  }
}

}  // namespace ash::firmware_update::metrics
