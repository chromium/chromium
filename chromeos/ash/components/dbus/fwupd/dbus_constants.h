// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_DBUS_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_DBUS_CONSTANTS_H_

namespace ash {

const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kFwupdServiceInterface[] = "org.freedesktop.fwupd";
const char kFwupdDeviceAddedSignalName[] = "DeviceAdded";
const char kFwupdDeviceRequestReceivedSignalName[] = "DeviceRequest";
const char kFwupdGetUpgradesMethodName[] = "GetUpgrades";
const char kFwupdGetDevicesMethodName[] = "GetDevices";
const char kFwupdInstallMethodName[] = "Install";
const char kFwupdSetFeatureFlagsMethodName[] = "SetFeatureFlags";

// Names of keys returned by the "DeviceRequest" signal:
// The RequestID is stored in the AppstreamId key in fwupd for legacy reasons.
const char kFwupdDeviceRequestKey_AppstreamId[] = "AppstreamId";
const char kFwupdDeviceRequestKey_Created[] = "Created";
const char kFwupdDeviceRequestKey_DeviceId[] = "DeviceId";
const char kFwupdDeviceRequestKey_UpdateMessage[] = "UpdateMessage";
const char kFwupdDeviceRequestKey_RequestKind[] = "RequestKind";

// Possible values for the "AppstreamId" key (a.k.a. DeviceRequestId) in a
// DeviceRequest signal
const char kFwupdDeviceRequestId_DoNotPowerOff[] =
    "org.freedesktop.fwupd.request.do-not-power-off";
const char kFwupdDeviceRequestId_InsertUSBCable[] =
    "org.freedesktop.fwupd.request.insert-usb-cable";
const char kFwupdDeviceRequestId_PressUnlock[] =
    "org.freedesktop.fwupd.request.press-unlock";
const char kFwupdDeviceRequestId_RemoveReplug[] =
    "org.freedesktop.fwupd.request.remove-replug";
const char kFwupdDeviceRequestId_RemoveUSBCable[] =
    "org.freedesktop.fwupd.request.remove-usb-cable";
const char kFwupdDeviceRequestId_ReplugInstall[] =
    "org.freedesktop.fwupd.replug-install";
const char kFwupdDeviceRequestId_ReplugPower[] =
    "org.freedesktop.fwupd.replug-power";

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_DBUS_CONSTANTS_H_
