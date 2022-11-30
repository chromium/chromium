// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageInfoUtil provides some general functions to get information
// from device::mojom::MtpStorageInfo needed by storage_monitor::StorageInfo.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_UTILS_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_UTILS_H_

#include <string>

#include "services/device/public/mojom/mtp_storage_info.mojom.h"

namespace storage_monitor {

// Constructs and returns the location of the device using the |storage_name|.
std::string GetDeviceLocationFromStorageName(const std::string& storage_name);

// Returns a unique device id from the given |storage_info|.
std::string GetDeviceIdFromStorageInfo(
    const device::mojom::MtpStorageInfo& storage_info);

// Helper function to get device label from storage information.
std::u16string GetDeviceLabelFromStorageInfo(
    const device::mojom::MtpStorageInfo& storage_info);

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_INFO_UTILS_H_
