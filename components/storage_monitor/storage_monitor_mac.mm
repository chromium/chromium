// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_mac.h"

#include <stdint.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/storage_monitor/image_capture_device_manager.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace storage_monitor {

namespace {

const char16_t kDiskImageModelName[] = u"Disk Image";

std::u16string GetUTF16FromDictionary(CFDictionaryRef dictionary,
                                      CFStringRef key) {
  CFStringRef value =
      base::apple::GetValueFromDictionary<CFStringRef>(dictionary, key);
  if (!value)
    return std::u16string();
  return base::SysCFStringRefToUTF16(value);
}

std::u16string JoinName(const std::u16string& name,
                        const std::u16string& addition) {
  if (addition.empty())
    return name;
  if (name.empty())
    return addition;
  return name + u' ' + addition;
}

StorageInfo::Type GetDeviceType(bool is_removable, bool has_dcim) {
  if (!is_removable)
    return StorageInfo::FIXED_MASS_STORAGE;
  if (has_dcim)
    return StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  return StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;
}

StorageInfo BuildStorageInfo(
    base::apple::ScopedCFTypeRef<CFDictionaryRef> scoped_dict,
    std::string* bsd_name) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  CFDictionaryRef dict = scoped_dict.get();

  CFStringRef device_bsd_name =
      base::apple::GetValueFromDictionary<CFStringRef>(
          dict, kDADiskDescriptionMediaBSDNameKey);
  if (device_bsd_name && bsd_name)
    *bsd_name = base::SysCFStringRefToUTF8(device_bsd_name);

  CFURLRef url = base::apple::GetValueFromDictionary<CFURLRef>(
      dict, kDADiskDescriptionVolumePathKey);
  base::FilePath location = base::apple::CFURLToFilePath(url);
  CFNumberRef size_number = base::apple::GetValueFromDictionary<CFNumberRef>(
      dict, kDADiskDescriptionMediaSizeKey);
  uint64_t size_in_bytes = 0;
  if (size_number)
    CFNumberGetValue(size_number, kCFNumberLongLongType, &size_in_bytes);

  std::u16string vendor =
      GetUTF16FromDictionary(dict, kDADiskDescriptionDeviceVendorKey);
  std::u16string model =
      GetUTF16FromDictionary(dict, kDADiskDescriptionDeviceModelKey);
  std::u16string label =
      GetUTF16FromDictionary(dict, kDADiskDescriptionVolumeNameKey);

  CFUUIDRef uuid = base::apple::GetValueFromDictionary<CFUUIDRef>(
      dict, kDADiskDescriptionVolumeUUIDKey);
  std::string unique_id;
  if (uuid) {
    base::apple::ScopedCFTypeRef<CFStringRef> uuid_string(
        CFUUIDCreateString(nullptr, uuid));
    if (uuid_string.get())
      unique_id = base::SysCFStringRefToUTF8(uuid_string.get());
  }
  if (unique_id.empty()) {
    std::u16string revision =
        GetUTF16FromDictionary(dict, kDADiskDescriptionDeviceRevisionKey);
    std::u16string unique_id2 = vendor;
    unique_id2 = JoinName(unique_id2, model);
    unique_id2 = JoinName(unique_id2, revision);
    unique_id = base::UTF16ToUTF8(unique_id2);
  }

  CFBooleanRef is_removable_ref =
      base::apple::GetValueFromDictionary<CFBooleanRef>(
          dict, kDADiskDescriptionMediaRemovableKey);
  bool is_removable = is_removable_ref && CFBooleanGetValue(is_removable_ref);
  // Checking for DCIM only matters on removable devices.
  bool has_dcim = is_removable && MediaStorageUtil::HasDcim(location);
  StorageInfo::Type device_type = GetDeviceType(is_removable, has_dcim);
  std::string device_id;
  if (!unique_id.empty())
    device_id = StorageInfo::MakeDeviceId(device_type, unique_id);

  return StorageInfo(device_id, location.value(), label, vendor, model,
                     size_in_bytes);
}

struct EjectDiskOptions {
  std::string bsd_name;
  base::OnceCallback<void(StorageMonitor::EjectStatus)> callback;
  base::apple::ScopedCFTypeRef<DADiskRef> disk;
};

void PostEjectCallback(DADiskRef disk,
                       DADissenterRef dissenter,
                       void* context) {
  std::unique_ptr<EjectDiskOptions> options_deleter(
      static_cast<EjectDiskOptions*>(context));
  if (dissenter) {
    std::move(options_deleter->callback).Run(StorageMonitor::EJECT_IN_USE);
    return;
  }

  std::move(options_deleter->callback).Run(StorageMonitor::EJECT_OK);
}

void PostUnmountCallback(DADiskRef disk,
                         DADissenterRef dissenter,
                         void* context) {
  std::unique_ptr<EjectDiskOptions> options_deleter(
      static_cast<EjectDiskOptions*>(context));
  if (dissenter) {
    std::move(options_deleter->callback).Run(StorageMonitor::EJECT_IN_USE);
    return;
  }

  DADiskEject(options_deleter->disk.get(), kDADiskEjectOptionDefault,
              PostEjectCallback, options_deleter.release());
}

void EjectDisk(EjectDiskOptions* options) {
  DADiskUnmount(options->disk.get(), kDADiskUnmountOptionWhole,
                PostUnmountCallback, options);
}

}  // namespace

StorageMonitorMac::StorageMonitorMac() = default;

StorageMonitorMac::~StorageMonitorMac() {
  if (session_.get()) {
    DASessionUnscheduleFromRunLoop(session_.get(), CFRunLoopGetCurrent(),
                                   kCFRunLoopCommonModes);
  }
}

void StorageMonitorMac::Init() {
  session_.reset(DASessionCreate(nullptr));

  // Register for callbacks for attached, changed, and removed devices.
  // This will send notifications for existing devices too.
  DARegisterDiskAppearedCallback(session_.get(),
                                 kDADiskDescriptionMatchVolumeMountable,
                                 DiskAppearedCallback, this);
  DARegisterDiskDisappearedCallback(session_.get(),
                                    kDADiskDescriptionMatchVolumeMountable,
                                    DiskDisappearedCallback, this);
  DARegisterDiskDescriptionChangedCallback(
      session_.get(), kDADiskDescriptionMatchVolumeMountable,
      kDADiskDescriptionWatchVolumePath, DiskDescriptionChangedCallback, this);

  DASessionScheduleWithRunLoop(session_.get(), CFRunLoopGetCurrent(),
                               kCFRunLoopCommonModes);

  image_capture_device_manager_ = std::make_unique<ImageCaptureDeviceManager>();
  image_capture_device_manager_->SetNotifications(receiver());
}

void StorageMonitorMac::UpdateDisk(UpdateType update_type,
                                   std::string* bsd_name,
                                   const StorageInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(bsd_name);

  pending_disk_updates_--;
  bool initialization_complete = false;
  if (!IsInitialized() && pending_disk_updates_ == 0)
    initialization_complete = true;

  if (info.device_id().empty() || bsd_name->empty()) {
    if (initialization_complete)
      MarkInitialized();
    return;
  }

  std::map<std::string, StorageInfo>::iterator it =
      disk_info_map_.find(*bsd_name);
  if (it != disk_info_map_.end()) {
    // If an attached notification was previously posted then post a detached
    // notification now. This is used for devices that are being removed or
    // devices that have changed.
    if (ShouldPostNotificationForDisk(it->second)) {
      receiver()->ProcessDetach(it->second.device_id());
    }
  }

  if (update_type == UPDATE_DEVICE_REMOVED) {
    if (it != disk_info_map_.end())
      disk_info_map_.erase(it);
  } else {
    disk_info_map_[*bsd_name] = info;
    if (ShouldPostNotificationForDisk(info))
      receiver()->ProcessAttach(info);
  }

  // We're not really honestly sure we're done, but this looks the best we
  // can do. Any misses should go out through notifications.
  if (initialization_complete)
    MarkInitialized();
}

bool StorageMonitorMac::GetStorageInfoForPath(const base::FilePath& path,
                                              StorageInfo* device_info) const {
  DCHECK(device_info);

  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  const base::FilePath root(base::FilePath::kSeparators);
  while (current != root) {
    StorageInfo info;
    if (FindDiskWithMountPoint(current, &info)) {
      *device_info = info;
      return true;
    }
    current = current.DirName();
  }

  return false;
}

void StorageMonitorMac::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(EjectStatus)> callback) {
  StorageInfo::Type type;
  std::string uuid;
  if (!StorageInfo::CrackDeviceId(device_id, &type, &uuid)) {
    std::move(callback).Run(EJECT_FAILURE);
    return;
  }

  if (type == StorageInfo::MAC_IMAGE_CAPTURE &&
      image_capture_device_manager_.get()) {
    image_capture_device_manager_->EjectDevice(uuid, std::move(callback));
    return;
  }

  std::string bsd_name;
  for (std::map<std::string, StorageInfo>::iterator
      it = disk_info_map_.begin(); it != disk_info_map_.end(); ++it) {
    if (it->second.device_id() == device_id) {
      bsd_name = it->first;
      disk_info_map_.erase(it);
      break;
    }
  }

  if (bsd_name.empty()) {
    std::move(callback).Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  receiver()->ProcessDetach(device_id);

  base::apple::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(nullptr, session_.get(), bsd_name.c_str()));
  if (!disk.get()) {
    std::move(callback).Run(StorageMonitor::EJECT_FAILURE);
    return;
  }
  // Get the reference to the full disk for ejecting.
  disk.reset(DADiskCopyWholeDisk(disk.get()));
  if (!disk.get()) {
    std::move(callback).Run(StorageMonitor::EJECT_FAILURE);
    return;
  }

  EjectDiskOptions* options = new EjectDiskOptions;
  options->bsd_name = bsd_name;
  options->callback = std::move(callback);
  options->disk = std::move(disk);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(EjectDisk, options));
}

// static
void StorageMonitorMac::DiskAppearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  monitor->GetDiskInfoAndUpdate(disk, UPDATE_DEVICE_ADDED);
}

// static
void StorageMonitorMac::DiskDisappearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  monitor->GetDiskInfoAndUpdate(disk, UPDATE_DEVICE_REMOVED);
}

// static
void StorageMonitorMac::DiskDescriptionChangedCallback(DADiskRef disk,
                                                       CFArrayRef keys,
                                                       void *context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  monitor->GetDiskInfoAndUpdate(disk, UPDATE_DEVICE_CHANGED);
}

void StorageMonitorMac::GetDiskInfoAndUpdate(
    DADiskRef disk,
    StorageMonitorMac::UpdateType update_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_disk_updates_++;

  base::apple::ScopedCFTypeRef<CFDictionaryRef> dict(
      DADiskCopyDescription(disk));
  std::string* bsd_name = new std::string;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildStorageInfo, dict, bsd_name),
      base::BindOnce(&StorageMonitorMac::UpdateDisk,
                     weak_ptr_factory_.GetWeakPtr(), update_type,
                     base::Owned(bsd_name)));
}


bool StorageMonitorMac::ShouldPostNotificationForDisk(
    const StorageInfo& info) const {
  // Only post notifications about disks that have no empty fields and
  // are removable. Also exclude disk images (DMGs).
  return !info.device_id().empty() && !info.location().empty() &&
         info.model_name() != kDiskImageModelName &&
         StorageInfo::IsMassStorageDevice(info.device_id());
}

bool StorageMonitorMac::FindDiskWithMountPoint(
    const base::FilePath& mount_point,
    StorageInfo* info) const {
  for (const auto& disk_info : disk_info_map_) {
    if (disk_info.second.location() == mount_point.value()) {
      *info = disk_info.second;
      return true;
    }
  }
  return false;
}

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorMac();
}

}  // namespace storage_monitor
