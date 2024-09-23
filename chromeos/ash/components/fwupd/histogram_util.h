// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_

#include <cstdint>
#include <string>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/time/time.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

namespace ash::firmware_update::metrics {

void EmitDeviceCount(int num_devices, bool is_startup);

void EmitUpdateCount(int num_updates,
                     int num_critical_updates,
                     bool is_startup);

void EmitInstallFailedWithStatus(FwupdStatus last_fwupd_status);
void EmitInstallResult(MethodResult result);
void EmitRefreshRemoteResult(MethodResult result);
void EmitDeviceRequest(firmware_update::mojom::DeviceRequestPtr request);
void EmitFailedDeviceRequestDuration(const base::TimeDelta& request_duration,
                                     mojom::DeviceRequestId request_id);
void EmitDeviceRequestSuccessfulWithDuration(
    const base::TimeDelta& request_duration,
    mojom::DeviceRequestId request_id);

std::string GetSourceStr(bool is_startup);
std::string GetRequestIdString(mojom::DeviceRequestId request_id);

}  // namespace ash::firmware_update::metrics

#endif  // CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
