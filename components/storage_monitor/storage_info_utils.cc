// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_info_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"

namespace storage_monitor {

namespace {

// Device root path constant.
const char kRootPath[] = "/";

// Returns the storage identifier of the device from the given |storage_name|.
// E.g. If the |storage_name| is "usb:2,2:65537", the storage identifier is
// "65537".
std::string GetStorageIdFromStorageName(const std::string& storage_name) {
  std::vector<base::StringPiece> name_parts = base::SplitStringPiece(
      storage_name, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return name_parts.size() == 3 ? std::string(name_parts[2]) : std::string();
}

// Returns the |data_store_id| string in the required format.
// If the |data_store_id| is 65537, this function returns " (65537)".
std::string GetFormattedIdString(const std::string& data_store_id) {
  return ("(" + data_store_id + ")");
}

}  // namespace

// Constructs and returns the location of the device using the |storage_name|.
std::string GetDeviceLocationFromStorageName(const std::string& storage_name) {
  // Construct a dummy device path using the storage name. This is only used
  // for registering device media file system.
  // E.g.: If the |storage_name| is "usb:2,2:12345" then "/usb:2,2:12345" is the
  // device location.
  DCHECK(!storage_name.empty());
  return kRootPath + storage_name;
}

// Returns a unique device id from the given |storage_info|.
std::string GetDeviceIdFromStorageInfo(
    const device::mojom::MtpStorageInfo& storage_info) {
  const std::string storage_id =
      GetStorageIdFromStorageName(storage_info.storage_name);
  if (storage_id.empty())
    return std::string();

  // Some devices have multiple data stores. Therefore, include storage id as
  // part of unique id along with vendor, model and volume information.
  const std::string vendor_id = base::NumberToString(storage_info.vendor_id);
  const std::string model_id = base::NumberToString(storage_info.product_id);
  return StorageInfo::MakeDeviceId(
      StorageInfo::MTP_OR_PTP,
      kVendorModelVolumeStoragePrefix + vendor_id + ":" + model_id + ":" +
          storage_info.volume_identifier + ":" + storage_id);
}

// Helper function to get device label from storage information.
std::u16string GetDeviceLabelFromStorageInfo(
    const device::mojom::MtpStorageInfo& storage_info) {
  std::string device_label;
  const std::string& vendor_name = storage_info.vendor;
  device_label = vendor_name;

  const std::string& product_name = storage_info.product;
  if (!product_name.empty()) {
    if (!device_label.empty())
      device_label += " ";
    device_label += product_name;
  }

  // Add the data store id to the device label.
  if (!device_label.empty()) {
    const std::string& volume_id = storage_info.volume_identifier;
    if (!volume_id.empty()) {
      device_label += GetFormattedIdString(volume_id);
    } else {
      const std::string data_store_id =
          GetStorageIdFromStorageName(storage_info.storage_name);
      if (!data_store_id.empty())
        device_label += GetFormattedIdString(data_store_id);
    }
  }
  return base::UTF8ToUTF16(device_label);
}

}  // namespace storage_monitor
