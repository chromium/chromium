// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_permissions.h"

#include "base/containers/span.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

namespace chromeos {
namespace extensions_api_permissions {

namespace {

using extensions::APIPermissionInfo;
using extensions::mojom::APIPermissionID;

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo kPermissionsToRegister[] = {
    // Telemetry System Extension permissions.
    {APIPermissionID::kChromeOSAttachedDeviceInfo, "os.attached_device_info"},
    {APIPermissionID::kChromeOSBluetoothPeripheralsInfo,
     "os.bluetooth_peripherals_info"},
    {APIPermissionID::kChromeOSDiagnostics, "os.diagnostics"},
    {APIPermissionID::kChromeOSDiagnosticsNetworkInfoForMlab,
     "os.diagnostics.network_info_mlab"},
    {APIPermissionID::kChromeOSEvents, "os.events"},
    {APIPermissionID::kChromeOSManagementAudio, "os.management.audio"},
    {APIPermissionID::kChromeOSTelemetry, "os.telemetry"},
    {APIPermissionID::kChromeOSTelemetrySerialNumber,
     "os.telemetry.serial_number"},
    {APIPermissionID::kChromeOSTelemetryNetworkInformation,
     "os.telemetry.network_info"}};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return kPermissionsToRegister;
}

}  // namespace extensions_api_permissions
}  // namespace chromeos
