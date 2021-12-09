// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FWUPD_DBUS_CONSTANTS_H_
#define CHROMEOS_DBUS_FWUPD_DBUS_CONSTANTS_H_

namespace chromeos {

const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kFwupdServiceInterface[] = "org.freedesktop.fwupd";
const char kFwupdDeviceAddedSignalName[] = "DeviceAdded";
const char kFwupdGetUpgradesMethodName[] = "GetUpgrades";
const char kFwupdGetDevicesMethodName[] = "GetDevices";
const char kFwupdInstallMethodName[] = "Install";

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FWUPD_DBUS_CONSTANTS_H_
