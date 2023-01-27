// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_chromeos.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/disks/disk.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/mtp_manager_client_chromeos.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"

namespace storage_monitor {

namespace {

using ::ash::disks::Disk;
using ::ash::disks::DiskMountManager;

// Constructs a device id using uuid or manufacturer (vendor and product) id
// details.
std::string MakeDeviceUniqueId(const Disk& disk) {
  std::string uuid = disk.fs_uuid();
  if (!uuid.empty())
    return kFSUniqueIdPrefix + uuid;

  // If one of the vendor or product information is missing, its value in the
  // string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialInfo
  // TODO(kmadhusu) Extract serial information for the disks and append it to
  // the device unique id.
  const std::string& vendor = disk.vendor_id();
  const std::string& product = disk.product_id();
  if (vendor.empty() && product.empty())
    return std::string();
  return kVendorModelSerialPrefix + vendor + ":" + product + ":";
}

// Returns whether the requested device is valid. On success |info| will contain
// device information.
bool GetDeviceInfo(const DiskMountManager::MountPoint& mount_info,
                   bool has_dcim,
                   StorageInfo* info) {
  DCHECK(info);
  std::string source_path = mount_info.source_path;

  const Disk* disk =
      DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == ash::DeviceType::kUnknown)
    return false;

  std::string unique_id = MakeDeviceUniqueId(*disk);
  if (unique_id.empty())
    return false;

  StorageInfo::Type type = has_dcim ?
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;

  *info = StorageInfo(
      StorageInfo::MakeDeviceId(type, unique_id), mount_info.mount_path,
      base::UTF8ToUTF16(disk->device_label()),
      base::UTF8ToUTF16(disk->vendor_name()),
      base::UTF8ToUTF16(disk->product_name()), disk->total_size_in_bytes());
  return true;
}

// Returns whether the requested device is valid. On success |info| will contain
// fixed storage device information.
bool GetFixedStorageInfo(const Disk& disk, StorageInfo* info) {
  DCHECK(info);

  std::string unique_id = MakeDeviceUniqueId(disk);
  if (unique_id.empty())
    return false;

  *info = StorageInfo(
      StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE, unique_id),
      disk.mount_path(), base::UTF8ToUTF16(disk.device_label()),
      base::UTF8ToUTF16(disk.vendor_name()),
      base::UTF8ToUTF16(disk.product_name()), disk.total_size_in_bytes());
  return true;
}

}  // namespace

StorageMonitorCros::StorageMonitorCros() = default;

StorageMonitorCros::~StorageMonitorCros() {
  DiskMountManager* manager = DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void StorageMonitorCros::Init() {
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPoints();

  // Tests may have already set a MTP manager.
  if (!mtp_device_manager_) {
    // Set up the connection with mojofied MtpManager.
    content::GetDeviceService().BindMtpManager(
        mtp_device_manager_.BindNewPipeAndPassReceiver());
  }
  // |mtp_manager_client_| needs to be initialized for both tests and
  // production code, so keep it out of the if condition.
  mtp_manager_client_ = std::make_unique<MtpManagerClientChromeOS>(
      receiver(), mtp_device_manager_.get());
}

void StorageMonitorCros::CheckExistingMountPoints() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& disk : DiskMountManager::GetInstance()->disks()) {
    if (disk->IsStatefulPartition()) {
      AddFixedStorageDisk(*disk);
      break;
    }
  }

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  for (const auto& mount_point :
       DiskMountManager::GetInstance()->mount_points()) {
    blocking_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MediaStorageUtil::HasDcim,
                       base::FilePath(mount_point.mount_path)),
        base::BindOnce(&StorageMonitorCros::AddMountedPath,
                       weak_ptr_factory_.GetWeakPtr(), mount_point));
  }

  // Note: Relies on scheduled tasks on the |blocking_task_runner| being
  // sequential. This block needs to follow the for loop, so that the DoNothing
  // call on the |blocking_task_runner| happens after the scheduled metadata
  // retrievals, meaning that the reply callback will then happen after all the
  // AddMountedPath calls.

  blocking_task_runner->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&StorageMonitorCros::MarkInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StorageMonitorCros::OnBootDeviceDiskEvent(
    DiskMountManager::DiskEvent event,
    const Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!disk.IsStatefulPartition())
    return;

  switch (event) {
    case DiskMountManager::DiskEvent::DISK_ADDED: {
      AddFixedStorageDisk(disk);
      break;
    }
    case DiskMountManager::DiskEvent::DISK_REMOVED: {
      RemoveFixedStorageDisk(disk);
      break;
    }
    case DiskMountManager::DiskEvent::DISK_CHANGED: {
      // Although boot disks never change, this event is fired when the device
      // is woken from suspend and re-enumerates the set of disks. The event
      // could be changed to only fire when an actual change occurs, but that's
      // not currently possible because the "re-enumerate on wake" behaviour is
      // relied on to re-mount external media that was unmounted when the system
      // was suspended.
      break;
    }
  }
}

void StorageMonitorCros::OnMountEvent(
    DiskMountManager::MountEvent event,
    ash::MountError error_code,
    const DiskMountManager::MountPoint& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ignore mount points that are not devices.
  if (mount_info.mount_type != ash::MountType::kDevice)
    return;
  // Ignore errors.
  if (error_code != ash::MountError::kSuccess)
    return;
  if (mount_info.mount_error != ash::MountError::kSuccess)
    return;

  switch (event) {
    case DiskMountManager::MOUNTING: {
      if (base::Contains(mount_map_, mount_info.mount_path)) {
        return;
      }

      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&MediaStorageUtil::HasDcim,
                         base::FilePath(mount_info.mount_path)),
          base::BindOnce(&StorageMonitorCros::AddMountedPath,
                         weak_ptr_factory_.GetWeakPtr(), mount_info));
      break;
    }
    case DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      receiver()->ProcessDetach(it->second.device_id());
      mount_map_.erase(it);
      break;
    }
  }
}

void StorageMonitorCros::SetMediaTransferProtocolManagerForTest(
    mojo::PendingRemote<device::mojom::MtpManager> test_manager) {
  DCHECK(!mtp_device_manager_);
  mtp_device_manager_.Bind(std::move(test_manager));
}

bool StorageMonitorCros::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  DCHECK(device_info);

  if (mtp_manager_client_->GetStorageInfoForPath(path, device_info)) {
    return true;
  }

  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  while (!base::Contains(mount_map_, current.value()) &&
         current != current.DirName()) {
    current = current.DirName();
  }

  MountMap::const_iterator info_it = mount_map_.find(current.value());
  if (info_it == mount_map_.end())
    return false;

  *device_info = info_it->second;
  return true;
}

// Callback executed when the unmount call is run by DiskMountManager.
// Forwards result to |EjectDevice| caller.
void NotifyUnmountResult(
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback,
    ash::MountError error_code) {
  if (error_code == ash::MountError::kSuccess)
    std::move(callback).Run(StorageMonitor::EJECT_OK);
  else
    std::move(callback).Run(StorageMonitor::EJECT_FAILURE);
}

void StorageMonitorCros::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(EjectStatus)> callback) {
  StorageInfo::Type type;
  if (!StorageInfo::CrackDeviceId(device_id, &type, nullptr)) {
    std::move(callback).Run(EJECT_FAILURE);
    return;
  }

  if (type == StorageInfo::MTP_OR_PTP) {
    mtp_manager_client_->EjectDevice(device_id, std::move(callback));
    return;
  }

  std::string mount_path;
  for (MountMap::const_iterator info_it = mount_map_.begin();
       info_it != mount_map_.end(); ++info_it) {
    if (info_it->second.device_id() == device_id)
      mount_path = info_it->first;
  }

  if (mount_path.empty()) {
    std::move(callback).Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  DiskMountManager* manager = DiskMountManager::GetInstance();
  if (!manager) {
    std::move(callback).Run(EJECT_FAILURE);
    return;
  }

  manager->UnmountPath(
      mount_path, base::BindOnce(NotifyUnmountResult, std::move(callback)));
}

device::mojom::MtpManager*
StorageMonitorCros::media_transfer_protocol_manager() {
  return mtp_device_manager_.get();
}

void StorageMonitorCros::AddMountedPath(
    const DiskMountManager::MountPoint& mount_info,
    bool has_dcim) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::Contains(mount_map_, mount_info.mount_path)) {
    // CheckExistingMountPoints() added the mount point information in the map
    // before the device attached handler is called. Therefore, an entry for
    // the device already exists in the map.
    return;
  }

  // Get the media device uuid and label if exists.
  StorageInfo info;
  if (!GetDeviceInfo(mount_info, has_dcim, &info))
    return;

  if (info.device_id().empty())
    return;

  mount_map_.insert(std::make_pair(mount_info.mount_path, info));

  receiver()->ProcessAttach(info);
}

void StorageMonitorCros::AddFixedStorageDisk(const Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(disk.IsStatefulPartition());

  StorageInfo info;
  if (!GetFixedStorageInfo(disk, &info))
    return;

  if (base::Contains(mount_map_, disk.mount_path()))
    return;

  mount_map_.insert(std::make_pair(disk.mount_path(), info));
  receiver()->ProcessAttach(info);
}

void StorageMonitorCros::RemoveFixedStorageDisk(const Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(disk.IsStatefulPartition());

  StorageInfo info;
  if (!GetFixedStorageInfo(disk, &info))
    return;

  size_t erased_count = mount_map_.erase(disk.mount_path());
  if (!erased_count)
    return;

  receiver()->ProcessDetach((info.device_id()));
}

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorCros();
}

}  // namespace storage_monitor
