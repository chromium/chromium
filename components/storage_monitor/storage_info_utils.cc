// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_info_utils.h"

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
  std::vector<std::string_view> name_parts = base::SplitStringPiece(
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
  // We'll add a storage_id suffix to our return value. The function name says
  // "device" but storage_monitor ultimately wants to identify a unit of
  // logical storage, not necessarily identify a physical device.
  //
  // MTP devices (e.g. phones) can have multiple physical storages (e.g. disks)
  // and each physical storage can have multiple logical storages.
  //
  // The storage_id returned by GetStorageIdFromStorageName often looks like
  // "65537", the string form of 0x0001_0001 that encodes a pair of uint16_t
  // identifiers for physical and logical storage. Zero values are reserved.
  // See the MTP spec (MTPforUSB-IFv1.1.pdf) section 5.2.1 Storage IDs.
  const std::string storage_id =
      GetStorageIdFromStorageName(storage_info.storage_name);
  if (storage_id.empty()) {
    return std::string();
  }

  // The serial number (and the storage_id) should form a unique id.
  if (!storage_info.serial_number.empty()) {
    return StorageInfo::MakeDeviceId(
        StorageInfo::MTP_OR_PTP,
        base::StrCat({storage_info.serial_number, ":", storage_id}));
  }

  // If no serial number is available, fall back to a combination of vendor,
  // model and volume (and the storage_id). This isn't always unique: attaching
  // two "Google Pixel" phones to a Chromebook at the same time can produce the
  // same (vendor, model, volume, storage) 4-tuple. The collision can cause
  // problems with other code that assumes "device ids" are unique. See
  // https://crbug.com/1184941
  //
  // Nonetheless, it's better than nothing. It also matches how Chrome OS
  // behaved (not using the serial number) for some years up until 2022.
  const std::string vendor_id = base::NumberToString(storage_info.vendor_id);
  const std::string model_id = base::NumberToString(storage_info.product_id);
  return StorageInfo::MakeDeviceId(
      StorageInfo::MTP_OR_PTP,
      base::StrCat({kVendorModelVolumeStoragePrefix, vendor_id, ":", model_id,
                    ":", storage_info.volume_identifier, ":", storage_id}));
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
