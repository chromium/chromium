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
const char kFwupdUpdateMetadataMethodName[] = "UpdateMetadata";

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

const char kFwupdErrorName_Internal[] = "org.freedesktop.fwupd.Internal";
const char kFwupdErrorName_VersionNewer[] =
    "org.freedesktop.fwupd.VersionNewer";
const char kFwupdErrorName_VersionSame[] = "org.freedesktop.fwupd.VersionSame";
const char kFwupdErrorName_AlreadyPending[] =
    "org.freedesktop.fwupd.AlreadyPending";
const char kFwupdErrorName_AuthFailed[] = "org.freedesktop.fwupd.AuthFailed";
const char kFwupdErrorName_Read[] = "org.freedesktop.fwupd.Read";
const char kFwupdErrorName_Write[] = "org.freedesktop.fwupd.Write";
const char kFwupdErrorName_InvalidFile[] = "org.freedesktop.fwupd.InvalidFile";
const char kFwupdErrorName_NotFound[] = "org.freedesktop.fwupd.NotFound";
const char kFwupdErrorName_NothingToDo[] = "org.freedesktop.fwupd.NothingToDo";
const char kFwupdErrorName_NotSupported[] =
    "org.freedesktop.fwupd.NotSupported";
const char kFwupdErrorName_SignatureInvalid[] =
    "org.freedesktop.fwupd.SignatureInvalid";
const char kFwupdErrorName_AcPowerRequired[] =
    "org.freedesktop.fwupd.AcPowerRequired";
const char kFwupdErrorName_PermissionDenied[] =
    "org.freedesktop.fwupd.PermissionDenied";
const char kFwupdErrorName_BrokenSystem[] =
    "org.freedesktop.fwupd.BrokenSystem";
const char kFwupdErrorName_BatteryLevelTooLow[] =
    "org.freedesktop.fwupd.BatteryLevelTooLow";
const char kFwupdErrorName_NeedsUserAction[] =
    "org.freedesktop.fwupd.NeedsUserAction";
const char kFwupdErrorName_AuthExpired[] = "org.freedesktop.fwupd.AuthExpired";

// "1" is the bitflag for an internal device. Defined here:
// https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h
const uint64_t kInternalDeviceFlag = 1;
// "100000000"(9th bit) is the bit representing FWUPD_DEVICE_FLAG_NEEDS_REBOOT
// in FwupdDeviceFlags. It signifies whether the device requires a system reboot
// to apply a firmware update.
// Defined here:
// https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h#L304-L311
const uint64_t kNeedsRebootDeviceFlag = 1llu << 8;

// "100000000"(9th bit) is the bit release flag for a trusted report.
// Defined here: https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h
const uint64_t kTrustedReportsReleaseFlag = 1llu << 8;

// "10000"(5th bit) is the fwupd feature flag to allow interactive requests.
// Defined here: https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h
const uint64_t kRequestsFeatureFlag = 1llu << 4;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_DBUS_CONSTANTS_H_
