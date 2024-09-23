// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux implementation.

#include "components/storage_monitor/storage_monitor_linux.h"

#include <mntent.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/udev_util_linux.h"
#include "device/udev_linux/scoped_udev.h"

namespace storage_monitor {

using MountPointDeviceMap = MtabWatcherLinux::MountPointDeviceMap;

namespace {

// udev device property constants.
const char kBlockSubsystemKey[] = "block";
const char kDiskDeviceTypeKey[] = "disk";
const char kFsUUID[] = "ID_FS_UUID";
const char kLabel[] = "ID_FS_LABEL";
const char kModel[] = "ID_MODEL";
const char kModelID[] = "ID_MODEL_ID";
const char kRemovableSysAttr[] = "removable";
const char kSerialShort[] = "ID_SERIAL_SHORT";
const char kSizeSysAttr[] = "size";
const char kVendor[] = "ID_VENDOR";
const char kVendorID[] = "ID_VENDOR_ID";

// Construct a device id using label or manufacturer (vendor and model) details.
std::string MakeDeviceUniqueId(struct udev_device* device) {
  std::string uuid = device::UdevDeviceGetPropertyValue(device, kFsUUID);
  if (!uuid.empty())
    return kFSUniqueIdPrefix + uuid;

  // If one of the vendor, model, serial information is missing, its value
  // in the string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialShortInfo
  // E.g.: VendorModelSerial:Kn:DataTravel_12.10:8000000000006CB02CDB
  std::string vendor = device::UdevDeviceGetPropertyValue(device, kVendorID);
  std::string model = device::UdevDeviceGetPropertyValue(device, kModelID);
  std::string serial_short =
      device::UdevDeviceGetPropertyValue(device, kSerialShort);
  if (vendor.empty() && model.empty() && serial_short.empty())
    return std::string();

  return kVendorModelSerialPrefix + vendor + ":" + model + ":" + serial_short;
}

// Returns the storage partition size of the device specified by |device_path|.
// If the requested information is unavailable, returns 0.
uint64_t GetDeviceStorageSize(const base::FilePath& device_path,
                              struct udev_device* device) {
  // sysfs provides the device size in units of 512-byte blocks.
  const std::string partition_size =
      device::UdevDeviceGetSysattrValue(device, kSizeSysAttr);

  uint64_t total_size_in_bytes = 0;
  if (!base::StringToUint64(partition_size, &total_size_in_bytes))
    return 0;
  return (total_size_in_bytes <= std::numeric_limits<uint64_t>::max() / 512)
             ? total_size_in_bytes * 512
             : 0;
}

// Gets the device information using udev library.
std::unique_ptr<StorageInfo> GetDeviceInfo(const base::FilePath& device_path,
                                           const base::FilePath& mount_point) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!device_path.empty());

  std::unique_ptr<StorageInfo> storage_info;

  device::ScopedUdevPtr udev_obj(device::udev_new());
  if (!udev_obj.get())
    return storage_info;

  struct stat device_stat;
  if (stat(device_path.value().c_str(), &device_stat) < 0)
    return storage_info;

  char device_type;
  if (S_ISCHR(device_stat.st_mode))
    device_type = 'c';
  else if (S_ISBLK(device_stat.st_mode))
    device_type = 'b';
  else
    return storage_info;  // Not a supported type.

  device::ScopedUdevDevicePtr device(
      device::udev_device_new_from_devnum(udev_obj.get(), device_type,
                                          device_stat.st_rdev));
  if (!device.get())
    return storage_info;

  std::u16string volume_label = base::UTF8ToUTF16(
      device::UdevDeviceGetPropertyValue(device.get(), kLabel));
  std::u16string vendor_name = base::UTF8ToUTF16(
      device::UdevDeviceGetPropertyValue(device.get(), kVendor));
  std::u16string model_name = base::UTF8ToUTF16(
      device::UdevDeviceGetPropertyValue(device.get(), kModel));

  std::string unique_id = MakeDeviceUniqueId(device.get());
  const char* value =
      device::udev_device_get_sysattr_value(device.get(), kRemovableSysAttr);
  if (!value) {
    // |parent_device| is owned by |device| and does not need to be cleaned
    // up.
    struct udev_device* parent_device =
        device::udev_device_get_parent_with_subsystem_devtype(
            device.get(),
            kBlockSubsystemKey,
            kDiskDeviceTypeKey);
    value = device::udev_device_get_sysattr_value(parent_device,
                                                  kRemovableSysAttr);
  }
  const bool is_removable = (value && atoi(value) == 1);

  StorageInfo::Type type = StorageInfo::FIXED_MASS_STORAGE;
  if (is_removable) {
    type = MediaStorageUtil::HasDcim(mount_point)
               ? StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM
               : StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;
  }

  storage_info = std::make_unique<StorageInfo>(
      StorageInfo::MakeDeviceId(type, unique_id), mount_point.value(),
      volume_label, vendor_name, model_name,
      GetDeviceStorageSize(device_path, device.get()));
  return storage_info;
}

// Runs |callback| with the |new_mtab| on |storage_monitor_task_runner|.
void BounceMtabUpdateToStorageMonitorTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> storage_monitor_task_runner,
    const MtabWatcherLinux::UpdateMtabCallback& callback,
    const MtabWatcherLinux::MountPointDeviceMap& new_mtab) {
  storage_monitor_task_runner->PostTask(FROM_HERE,
                                        base::BindOnce(callback, new_mtab));
}

std::unique_ptr<MtabWatcherLinux> CreateMtabWatcherLinuxOnMtabWatcherTaskRunner(
    const base::FilePath& mtab_path,
    scoped_refptr<base::SequencedTaskRunner> storage_monitor_task_runner,
    const MtabWatcherLinux::UpdateMtabCallback& callback) {
  return std::make_unique<MtabWatcherLinux>(
      mtab_path,
      base::BindRepeating(&BounceMtabUpdateToStorageMonitorTaskRunner,
                          storage_monitor_task_runner, callback));
}

StorageMonitor::EjectStatus EjectPathOnBlockingTaskRunner(
    const base::FilePath& path,
    const base::FilePath& device) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Note: Linux LSB says umount should exist in /bin.
  static const char kUmountBinary[] = "/bin/umount";
  std::vector<std::string> command{kUmountBinary, path.value()};

  base::LaunchOptions options;
  base::Process process = base::LaunchProcess(command, options);
  if (!process.IsValid())
    return StorageMonitor::EJECT_FAILURE;

  static constexpr auto kEjectTimeout = base::Seconds(3);
  int exit_code = -1;
  if (!process.WaitForExitWithTimeout(kEjectTimeout, &exit_code)) {
    process.Terminate(-1, false);
    base::EnsureProcessTerminated(std::move(process));
    return StorageMonitor::EJECT_FAILURE;
  }

  // TODO(gbillock): Make sure this is found in documentation
  // somewhere. Experimentally it seems to hold that exit code
  // 1 means device is in use.
  if (exit_code == 1)
    return StorageMonitor::EJECT_IN_USE;
  if (exit_code != 0)
    return StorageMonitor::EJECT_FAILURE;

  return StorageMonitor::EJECT_OK;
}

}  // namespace

StorageMonitorLinux::StorageMonitorLinux(const base::FilePath& path)
    : mtab_path_(path),
      get_device_info_callback_(base::BindRepeating(&GetDeviceInfo)),
      mtab_watcher_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

StorageMonitorLinux::~StorageMonitorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mtab_watcher_task_runner_->DeleteSoon(FROM_HERE, mtab_watcher_.release());
}

void StorageMonitorLinux::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!mtab_path_.empty());

  mtab_watcher_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateMtabWatcherLinuxOnMtabWatcherTaskRunner, mtab_path_,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     base::BindRepeating(&StorageMonitorLinux::UpdateMtab,
                                         weak_ptr_factory_.GetWeakPtr())),
      base::BindOnce(&StorageMonitorLinux::OnMtabWatcherCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool StorageMonitorLinux::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_info);

  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  while (!base::Contains(mount_info_map_, current) &&
         current != current.DirName())
    current = current.DirName();

  auto mount_info = mount_info_map_.find(current);
  if (mount_info == mount_info_map_.end())
    return false;
  *device_info = mount_info->second.storage_info;
  return true;
}

void StorageMonitorLinux::SetGetDeviceInfoCallbackForTest(
    const GetDeviceInfoCallback& get_device_info_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  get_device_info_callback_ = get_device_info_callback;
}

void StorageMonitorLinux::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(EjectStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StorageInfo::Type type;
  if (!StorageInfo::CrackDeviceId(device_id, &type, nullptr)) {
    std::move(callback).Run(EJECT_FAILURE);
    return;
  }

  DCHECK_NE(type, StorageInfo::MTP_OR_PTP);

  // Find the mount point for the given device ID.
  base::FilePath path;
  base::FilePath device;
  for (auto mount_info = mount_info_map_.begin();
       mount_info != mount_info_map_.end(); ++mount_info) {
    if (mount_info->second.storage_info.device_id() == device_id) {
      path = mount_info->first;
      device = mount_info->second.mount_device;
      mount_info_map_.erase(mount_info);
      break;
    }
  }

  if (path.empty()) {
    std::move(callback).Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  receiver()->ProcessDetach(device_id);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&EjectPathOnBlockingTaskRunner, path, device),
      std::move(callback));
}

void StorageMonitorLinux::OnMtabWatcherCreated(
    std::unique_ptr<MtabWatcherLinux> watcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mtab_watcher_ = std::move(watcher);
}

void StorageMonitorLinux::UpdateMtab(const MountPointDeviceMap& new_mtab) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check existing mtab entries for unaccounted mount points.
  // These mount points must have been removed in the new mtab.
  std::list<base::FilePath> mount_points_to_erase;
  std::list<base::FilePath> multiple_mounted_devices_needing_reattachment;
  for (MountMap::const_iterator old_iter = mount_info_map_.begin();
       old_iter != mount_info_map_.end(); ++old_iter) {
    const base::FilePath& mount_point = old_iter->first;
    const base::FilePath& mount_device = old_iter->second.mount_device;
    auto new_iter = new_mtab.find(mount_point);
    // |mount_point| not in |new_mtab| or |mount_device| is no longer mounted at
    // |mount_point|.
    if (new_iter == new_mtab.end() || (new_iter->second != mount_device)) {
      auto priority = mount_priority_map_.find(mount_device);
      CHECK(priority != mount_priority_map_.end(), base::NotFatalUntil::M130);
      ReferencedMountPoint::const_iterator has_priority =
          priority->second.find(mount_point);
      if (StorageInfo::IsRemovableDevice(
              old_iter->second.storage_info.device_id())) {
        CHECK(has_priority != priority->second.end(),
              base::NotFatalUntil::M130);
        if (has_priority->second) {
          receiver()->ProcessDetach(old_iter->second.storage_info.device_id());
        }
        if (priority->second.size() > 1)
          multiple_mounted_devices_needing_reattachment.push_back(mount_device);
      }
      priority->second.erase(mount_point);
      if (priority->second.empty())
        mount_priority_map_.erase(mount_device);
      mount_points_to_erase.push_back(mount_point);
    }
  }

  // Erase the |mount_info_map_| entries afterwards. Erasing in the loop above
  // using the iterator is slightly more efficient, but more tricky, since
  // calling std::map::erase() on an iterator invalidates it.
  for (std::list<base::FilePath>::const_iterator it =
           mount_points_to_erase.begin();
       it != mount_points_to_erase.end();
       ++it) {
    mount_info_map_.erase(*it);
  }

  // For any multiply mounted device where the mount that we had notified
  // got detached, send a notification of attachment for one of the other
  // mount points.
  for (std::list<base::FilePath>::const_iterator it =
           multiple_mounted_devices_needing_reattachment.begin();
       it != multiple_mounted_devices_needing_reattachment.end();
       ++it) {
    auto first_mount_point_info = mount_priority_map_.find(*it)->second.begin();
    const base::FilePath& mount_point = first_mount_point_info->first;
    first_mount_point_info->second = true;

    const StorageInfo& mount_info =
        mount_info_map_.find(mount_point)->second.storage_info;
    DCHECK(StorageInfo::IsRemovableDevice(mount_info.device_id()));
    receiver()->ProcessAttach(mount_info);
  }

  // Check new mtab entries against existing ones.
  scoped_refptr<base::SequencedTaskRunner> mounting_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  for (auto new_iter = new_mtab.begin(); new_iter != new_mtab.end();
       ++new_iter) {
    const base::FilePath& mount_point = new_iter->first;
    const base::FilePath& mount_device = new_iter->second;
    auto old_iter = mount_info_map_.find(mount_point);
    if (old_iter == mount_info_map_.end() ||
        old_iter->second.mount_device != mount_device) {
      // New mount point found or an existing mount point found with a new
      // device.
      if (IsDeviceAlreadyMounted(mount_device)) {
        HandleDeviceMountedMultipleTimes(mount_device, mount_point);
      } else {
        mounting_task_runner->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(get_device_info_callback_, mount_device,
                           mount_point),
            base::BindOnce(&StorageMonitorLinux::AddNewMount,
                           weak_ptr_factory_.GetWeakPtr(), mount_device));
      }
    }
  }

  // Note: Relies on scheduled tasks on the |mounting_task_runner| being
  // sequential. This block needs to follow the for loop, so that the DoNothing
  // call on the |mounting_task_runner| happens after the scheduled metadata
  // retrievals, meaning that the reply callback will then happen after all the
  // AddNewMount calls.
  if (!IsInitialized()) {
    mounting_task_runner->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&StorageMonitorLinux::MarkInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool StorageMonitorLinux::IsDeviceAlreadyMounted(
    const base::FilePath& mount_device) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(mount_priority_map_, mount_device);
}

void StorageMonitorLinux::HandleDeviceMountedMultipleTimes(
    const base::FilePath& mount_device,
    const base::FilePath& mount_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto priority = mount_priority_map_.find(mount_device);
  CHECK(priority != mount_priority_map_.end(), base::NotFatalUntil::M130);
  const base::FilePath& other_mount_point = priority->second.begin()->first;
  priority->second[mount_point] = false;
  mount_info_map_[mount_point] =
      mount_info_map_.find(other_mount_point)->second;
}

void StorageMonitorLinux::AddNewMount(
    const base::FilePath& mount_device,
    std::unique_ptr<StorageInfo> storage_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!storage_info)
    return;

  DCHECK(!storage_info->device_id().empty());

  bool removable = StorageInfo::IsRemovableDevice(storage_info->device_id());
  const base::FilePath mount_point(storage_info->location());

  MountPointInfo mount_point_info;
  mount_point_info.mount_device = mount_device;
  mount_point_info.storage_info = *storage_info;
  mount_info_map_[mount_point] = mount_point_info;
  mount_priority_map_[mount_device][mount_point] = removable;
  receiver()->ProcessAttach(*storage_info);
}

StorageMonitor* StorageMonitor::CreateInternal() {
  const base::FilePath kDefaultMtabPath("/etc/mtab");
  return new StorageMonitorLinux(kDefaultMtabPath);
}

}  // namespace storage_monitor
