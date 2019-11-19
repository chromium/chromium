// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/mtp_manager_client_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_info_utils.h"

namespace storage_monitor {

MtpManagerClientChromeOS::MtpManagerClientChromeOS(
    StorageMonitor::Receiver* receiver,
    device::mojom::MtpManager* mtp_manager)
    : mtp_manager_(mtp_manager), notifications_(receiver) {
  mtp_manager_->EnumerateStoragesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&MtpManagerClientChromeOS::OnReceivedStorages,
                     weak_ptr_factory_.GetWeakPtr()));
}

MtpManagerClientChromeOS::~MtpManagerClientChromeOS() {}

bool MtpManagerClientChromeOS::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* storage_info) const {
  DCHECK(storage_info);

  if (!path.IsAbsolute())
    return false;

  std::vector<base::FilePath::StringType> path_components;
  path.GetComponents(&path_components);
  if (path_components.size() < 2)
    return false;

  // First and second component of the path specifies the device location.
  // E.g.: If |path| is "/usb:2,2:65537/DCIM/Folder_a", "/usb:2,2:65537" is the
  // device location.
  const auto info_it =
      storage_map_.find(GetDeviceLocationFromStorageName(path_components[1]));
  if (info_it == storage_map_.end())
    return false;

  *storage_info = info_it->second;
  return true;
}

void MtpManagerClientChromeOS::EjectDevice(
    const std::string& device_id,
    base::Callback<void(StorageMonitor::EjectStatus)> callback) {
  std::string location;
  if (!GetLocationForDeviceId(device_id, &location)) {
    callback.Run(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  // TODO(thestig): Change this to tell the MTP manager to eject the device.

  StorageDetached(location);
  callback.Run(StorageMonitor::EJECT_OK);
}

// device::mojom::MtpManagerClient override.
void MtpManagerClientChromeOS::StorageAttached(
    device::mojom::MtpStorageInfoPtr mtp_storage_info) {
  if (!mtp_storage_info)
    return;

  // Create StorageMonitor format StorageInfo and update the local map.
  std::string device_id = GetDeviceIdFromStorageInfo(*mtp_storage_info);
  base::string16 storage_label =
      GetDeviceLabelFromStorageInfo(*mtp_storage_info);
  std::string location =
      GetDeviceLocationFromStorageName(mtp_storage_info->storage_name);
  base::string16 vendor_name = base::UTF8ToUTF16(mtp_storage_info->vendor);
  base::string16 product_name = base::UTF8ToUTF16(mtp_storage_info->product);

  if (device_id.empty() || storage_label.empty())
    return;

  DCHECK(!base::Contains(storage_map_, location));

  StorageInfo storage_info(device_id, location, storage_label, vendor_name,
                           product_name, 0);
  storage_map_[location] = storage_info;

  // Notify StorageMonitor observers about the event.
  notifications_->ProcessAttach(storage_info);
}

// device::mojom::MtpManagerClient override.
void MtpManagerClientChromeOS::StorageDetached(
    const std::string& storage_name) {
  DCHECK(!storage_name.empty());

  StorageLocationToInfoMap::iterator it =
      storage_map_.find(GetDeviceLocationFromStorageName(storage_name));
  if (it == storage_map_.end())
    return;

  // Notify StorageMonitor observers about the event.
  notifications_->ProcessDetach(it->second.device_id());
  storage_map_.erase(it);
}

void MtpManagerClientChromeOS::OnReceivedStorages(
    std::vector<device::mojom::MtpStorageInfoPtr> storage_info_list) {
  for (auto& storage_info : storage_info_list)
    StorageAttached(std::move(storage_info));
}

bool MtpManagerClientChromeOS::GetLocationForDeviceId(
    const std::string& device_id,
    std::string* location) const {
  for (const auto& it : storage_map_) {
    if (it.second.device_id() == device_id) {
      *location = it.first;
      return true;
    }
  }
  return false;
}

}  // namespace storage_monitor
