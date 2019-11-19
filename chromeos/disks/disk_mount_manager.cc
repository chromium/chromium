// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/disk_mount_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/suspend_unmount_manager.h"

namespace chromeos {
namespace disks {

namespace {

constexpr char kDeviceNotFound[] = "Device could not be found";
DiskMountManager* g_disk_mount_manager = NULL;

struct UnmountDeviceRecursivelyCallbackData {
  UnmountDeviceRecursivelyCallbackData(
      DiskMountManager::UnmountDeviceRecursivelyCallbackType in_callback)
      : callback(std::move(in_callback)) {}

  DiskMountManager::UnmountDeviceRecursivelyCallbackType callback;
  MountError error_code = MOUNT_ERROR_NONE;
};

void OnAllUnmountDeviceRecursively(
    std::unique_ptr<UnmountDeviceRecursivelyCallbackData> cb_data) {
  std::move(cb_data->callback).Run(cb_data->error_code);
}

std::string FormatFileSystemTypeToString(FormatFileSystemType filesystem) {
  switch (filesystem) {
    case FormatFileSystemType::kUnknown:
      return "";
    case FormatFileSystemType::kVfat:
      return "vfat";
    case FormatFileSystemType::kExfat:
      return "exfat";
    case FormatFileSystemType::kNtfs:
      return "ntfs";
  }
  NOTREACHED() << "Unknown filesystem type " << static_cast<int>(filesystem);
  return "";
}

// The DiskMountManager implementation.
class DiskMountManagerImpl : public DiskMountManager,
                             public CrosDisksClient::Observer {
 public:
  DiskMountManagerImpl() : already_refreshed_(false) {
    DBusThreadManager* dbus_thread_manager = DBusThreadManager::Get();
    cros_disks_client_ = dbus_thread_manager->GetCrosDisksClient();
    suspend_unmount_manager_.reset(new SuspendUnmountManager(this));

    cros_disks_client_->AddObserver(this);
  }

  ~DiskMountManagerImpl() override { cros_disks_client_->RemoveObserver(this); }

  // DiskMountManager override.
  void AddObserver(DiskMountManager::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  // DiskMountManager override.
  void RemoveObserver(DiskMountManager::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  // DiskMountManager override.
  void MountPath(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 MountType type,
                 MountAccessMode access_mode) override {
    // Hidden and non-existent devices should not be mounted.
    if (type == MOUNT_TYPE_DEVICE) {
      DiskMap::const_iterator it = disks_.find(source_path);
      if (it == disks_.end() || it->second->is_hidden()) {
        OnMountCompleted(MountEntry(MOUNT_ERROR_INTERNAL, source_path, type,
                                    ""));
        return;
      }
    }
    std::vector<std::string> options = mount_options;
    if (base::FeatureList::IsEnabled(chromeos::features::kFsNosymfollow))
      options.push_back("nosymfollow");
    cros_disks_client_->Mount(
        source_path, source_format, mount_label, options, access_mode,
        REMOUNT_OPTION_MOUNT_NEW_DEVICE,
        base::BindOnce(&DiskMountManagerImpl::OnMount,
                       weak_ptr_factory_.GetWeakPtr(), source_path, type));

    // Record the access mode option passed to CrosDisks.
    // This is needed because CrosDisks service methods doesn't return the info
    // via DBus.
    access_modes_.insert(std::make_pair(source_path, access_mode));
  }

  // DiskMountManager override.
  void UnmountPath(const std::string& mount_path,
                   UnmountPathCallback callback) override {
    UnmountChildMounts(mount_path);
    cros_disks_client_->Unmount(
        mount_path, base::BindOnce(&DiskMountManagerImpl::OnUnmountPath,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback), mount_path));
  }

  void RemountAllRemovableDrives(MountAccessMode mode) override {
    // TODO(yamaguchi): Retry for tentative remount failures. crbug.com/661455
    for (const auto& device_path_and_disk : disks_) {
      const Disk& disk = *device_path_and_disk.second;
      if (disk.is_read_only_hardware()) {
        // Read-only devices can be mounted in RO mode only. No need to remount.
        continue;
      }
      if (!disk.is_mounted()) {
        continue;
      }
      RemountRemovableDrive(disk, mode);
    }
  }

  // DiskMountManager override.
  void FormatMountedDevice(const std::string& mount_path,
                           FormatFileSystemType filesystem,
                           const std::string& label) override {
    if (filesystem == FormatFileSystemType::kUnknown) {
      LOG(ERROR) << "Unknown filesystem passed to FormatMountedDevice";
      OnFormatCompleted(FORMAT_ERROR_UNSUPPORTED_FILESYSTEM, mount_path);
      return;
    }

    MountPointMap::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      LOG(ERROR) << "Mount point with path \"" << mount_path << "\" not found.";
      OnFormatCompleted(FORMAT_ERROR_UNKNOWN, mount_path);
      return;
    }

    std::string device_path = mount_point->second.source_path;
    DiskMap::const_iterator disk = disks_.find(device_path);
    if (disk == disks_.end()) {
      LOG(ERROR) << "Device with path \"" << device_path << "\" not found.";
      OnFormatCompleted(FORMAT_ERROR_UNKNOWN, device_path);
      return;
    }
    if (disk->second->is_read_only()) {
      LOG(ERROR) << "Mount point with path \"" << mount_path
                 << "\" is read-only.";
      OnFormatCompleted(FORMAT_ERROR_DEVICE_NOT_ALLOWED, mount_path);
      return;
    }

    UnmountPath(disk->second->mount_path(),
                base::BindOnce(&DiskMountManagerImpl::OnUnmountPathForFormat,
                               weak_ptr_factory_.GetWeakPtr(), device_path,
                               filesystem, label));
  }

  void RenameMountedDevice(const std::string& mount_path,
                           const std::string& volume_name) override {
    MountPointMap::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      LOG(ERROR) << "Mount point with path '" << mount_path << "' not found.";
      OnRenameCompleted(RENAME_ERROR_UNKNOWN, mount_path);
      return;
    }

    std::string device_path = mount_point->second.source_path;
    DiskMap::const_iterator iter = disks_.find(device_path);
    if (iter == disks_.end()) {
      LOG(ERROR) << "Device with path '" << device_path << "' not found.";
      OnRenameCompleted(RENAME_ERROR_UNKNOWN, device_path);
      return;
    }
    if (iter->second->is_read_only()) {
      LOG(ERROR) << "Mount point with path '" << mount_path
                 << "' is read-only.";
      OnRenameCompleted(RENAME_ERROR_DEVICE_NOT_ALLOWED, mount_path);
      return;
    }

    UnmountPath(iter->second->mount_path(),
                base::BindOnce(&DiskMountManagerImpl::OnUnmountPathForRename,
                               weak_ptr_factory_.GetWeakPtr(), device_path,
                               volume_name));
  }

  // DiskMountManager override.
  void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) override {
    std::vector<std::string> devices_to_unmount;

    // Get list of all devices to unmount.
    int device_path_len = device_path.length();
    for (DiskMap::iterator it = disks_.begin(); it != disks_.end(); ++it) {
      if (!it->second->mount_path().empty() &&
          strncmp(device_path.c_str(), it->second->device_path().c_str(),
                  device_path_len) == 0) {
        devices_to_unmount.push_back(it->second->mount_path());
      }
    }

    // We should detect at least original device.
    if (devices_to_unmount.empty()) {
      if (disks_.find(device_path) == disks_.end()) {
        LOG(WARNING) << "Unmount recursive request failed for device "
                     << device_path << ", with error: " << kDeviceNotFound;
        std::move(callback).Run(MOUNT_ERROR_INVALID_DEVICE_PATH);
        return;
      }

      // Nothing to unmount.
      std::move(callback).Run(MOUNT_ERROR_NONE);
      return;
    }

    std::unique_ptr<UnmountDeviceRecursivelyCallbackData> cb_data =
        std::make_unique<UnmountDeviceRecursivelyCallbackData>(
            std::move(callback));
    UnmountDeviceRecursivelyCallbackData* raw_cb_data = cb_data.get();
    base::RepeatingClosure done_callback = base::BarrierClosure(
        devices_to_unmount.size(),
        base::BindOnce(&OnAllUnmountDeviceRecursively, std::move(cb_data)));

    for (size_t i = 0; i < devices_to_unmount.size(); ++i) {
      cros_disks_client_->Unmount(
          devices_to_unmount[i],
          base::BindOnce(&DiskMountManagerImpl::OnUnmountDeviceRecursively,
                         weak_ptr_factory_.GetWeakPtr(), raw_cb_data,
                         devices_to_unmount[i], done_callback));
    }
  }

  // DiskMountManager override.
  void EnsureMountInfoRefreshed(EnsureMountInfoRefreshedCallback callback,
                                bool force) override {
    if (!force && already_refreshed_) {
      std::move(callback).Run(true);
      return;
    }

    refresh_callbacks_.push_back(std::move(callback));
    if (refresh_callbacks_.size() == 1) {
      // If there's no in-flight refreshing task, start it.
      cros_disks_client_->EnumerateDevices(
          base::BindOnce(&DiskMountManagerImpl::RefreshAfterEnumerateDevices,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&DiskMountManagerImpl::RefreshCompleted,
                         weak_ptr_factory_.GetWeakPtr(), false));
    }
  }

  // DiskMountManager override.
  const DiskMap& disks() const override { return disks_; }

  // DiskMountManager override.
  const Disk* FindDiskBySourcePath(
      const std::string& source_path) const override {
    DiskMap::const_iterator disk_it = disks_.find(source_path);
    return disk_it == disks_.end() ? NULL : disk_it->second.get();
  }

  // DiskMountManager override.
  const MountPointMap& mount_points() const override { return mount_points_; }

  // DiskMountManager override.
  bool AddDiskForTest(std::unique_ptr<Disk> disk) override {
    if (disks_.find(disk->device_path()) != disks_.end()) {
      LOG(ERROR) << "Attempt to add a duplicate disk";
      return false;
    }

    disks_.insert(std::make_pair(disk->device_path(), std::move(disk)));
    return true;
  }

  // DiskMountManager override.
  // Corresponding disk should be added to the manager before this is called.
  bool AddMountPointForTest(const MountPointInfo& mount_point) override {
    if (mount_points_.find(mount_point.mount_path) != mount_points_.end()) {
      LOG(ERROR) << "Attempt to add a duplicate mount point";
      return false;
    }
    if (mount_point.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
        disks_.find(mount_point.source_path) == disks_.end()) {
      LOG(ERROR) << "Device mount points must have a disk entry.";
      return false;
    }

    mount_points_.insert(std::make_pair(mount_point.mount_path, mount_point));
    return true;
  }

 private:
  // A struct to represent information about a format changes.
  struct FormatChange {
    // new file system type
    std::string file_system_type;
    // New volume name
    std::string volume_name;
  };

  // Stores new volume name and file system type for a device on which
  // formatting is invoked on, so that OnFormatCompleted can set it back to
  // |disks_|. The key is a device_path and the value is a FormatChange.
  std::map<std::string, FormatChange> pending_format_changes_;

  // Stores new volume name for a device on which renaming is invoked on, so
  // that OnRenameCompleted can set it back to |disks_|. The key is a
  // device_path and the value is new volume_name.
  std::map<std::string, std::string> pending_rename_changes_;

  // Called on D-Bus CrosDisksClient::Mount() is done.
  void OnMount(const std::string& source_path, MountType type, bool result) {
    // When succeeds, OnMountCompleted will be called by "MountCompleted",
    // signal instead. Do nothing now.
    if (result)
      return;

    OnMountCompleted(
        MountEntry(MOUNT_ERROR_INTERNAL, source_path, type, std::string()));
  }

  void RemountRemovableDrive(const Disk& disk,
                             MountAccessMode access_mode) {
    const std::string& mount_path = disk.mount_path();
    MountPointMap::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      // Not in mount_points_. This happens when the mount_points and disks_ are
      // inconsistent.
      LOG(ERROR) << "Mount point with path \"" << mount_path << "\" not found.";
      OnMountCompleted(
          MountEntry(MOUNT_ERROR_PATH_NOT_MOUNTED, disk.device_path(),
                     MOUNT_TYPE_DEVICE, mount_path));
      return;
    }
    const std::string& source_path = mount_point->second.source_path;

    // Update the access mode option passed to CrosDisks.
    // This is needed because CrosDisks service methods doesn't return the info
    // via DBus, and must be updated before issuing Mount command as it'll be
    // read by the handler of MountCompleted signal.
    access_modes_[source_path] = access_mode;

    cros_disks_client_->Mount(
        mount_point->second.source_path, std::string(), std::string(), {},
        access_mode, REMOUNT_OPTION_REMOUNT_EXISTING_DEVICE,
        base::BindOnce(&DiskMountManagerImpl::OnMount,
                       weak_ptr_factory_.GetWeakPtr(), source_path,
                       mount_point->second.mount_type));
  }

  // Unmounts all mount points whose source path is transitively parented by
  // |mount_path|.
  void UnmountChildMounts(const std::string& mount_path_in) {
    std::string mount_path = mount_path_in;
    // Let's make sure mount path has trailing slash.
    if (mount_path.back() != '/')
      mount_path += '/';

    for (MountPointMap::iterator it = mount_points_.begin();
         it != mount_points_.end();
         ++it) {
      if (base::StartsWith(it->second.source_path, mount_path,
                           base::CompareCase::SENSITIVE)) {
        // TODO(tbarzic): Handle the case where this fails.
        UnmountPath(it->second.mount_path, UnmountPathCallback());
      }
    }
  }

  // Callback for UnmountDeviceRecursively.
  void OnUnmountDeviceRecursively(UnmountDeviceRecursivelyCallbackData* cb_data,
                                  const std::string& mount_path,
                                  base::OnceClosure done_callback,
                                  MountError error_code) {
    if (error_code == MOUNT_ERROR_PATH_NOT_MOUNTED ||
        error_code == MOUNT_ERROR_INVALID_PATH) {
      // The path was already unmounted by something else.
      error_code = MOUNT_ERROR_NONE;
    }

    if (error_code == MOUNT_ERROR_NONE) {
      // Do standard processing for Unmount event.
      OnUnmountPath(UnmountPathCallback(), mount_path, MOUNT_ERROR_NONE);
      VLOG(1) << mount_path <<  " unmounted.";
    } else {
      // This causes the last non-success error to be reported.
      // TODO(amistry): We could ignore certain errors such as
      // MOUNT_ERROR_PATH_NOT_MOUNTED, or prioritise more important ones.
      cb_data->error_code = error_code;
    }

    std::move(done_callback).Run();
  }

  // CrosDisksClient::Observer override.
  void OnMountCompleted(const MountEntry& entry) override {
    MountCondition mount_condition = MOUNT_CONDITION_NONE;
    if (entry.mount_type() == MOUNT_TYPE_DEVICE) {
      if (entry.error_code() == MOUNT_ERROR_UNKNOWN_FILESYSTEM) {
        mount_condition = MOUNT_CONDITION_UNKNOWN_FILESYSTEM;
      }
      if (entry.error_code() == MOUNT_ERROR_UNSUPPORTED_FILESYSTEM) {
        mount_condition = MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
      }
    }
    const MountPointInfo mount_info(entry.source_path(),
                                    entry.mount_path(),
                                    entry.mount_type(),
                                    mount_condition);

    // If the device is corrupted but it's still possible to format it, it will
    // be fake mounted.
    if ((entry.error_code() == MOUNT_ERROR_NONE ||
         mount_info.mount_condition) &&
        mount_points_.find(mount_info.mount_path) == mount_points_.end()) {
      mount_points_.insert(MountPointMap::value_type(mount_info.mount_path,
                                                     mount_info));
    }

    Disk* disk = nullptr;
    if ((entry.error_code() == MOUNT_ERROR_NONE ||
         mount_info.mount_condition) &&
        mount_info.mount_type == MOUNT_TYPE_DEVICE &&
        !mount_info.source_path.empty() &&
        !mount_info.mount_path.empty()) {
      DiskMap::iterator iter = disks_.find(mount_info.source_path);
      if (iter != disks_.end()) {  // disk might have been removed by now?
        disk = iter->second.get();
        DCHECK(disk);
        // Currently the MountCompleted signal doesn't tell whether the device
        // is mounted in read-only mode or not. Instead use the mount option
        // recorded by DiskMountManagerImpl::MountPath().
        // |source_path| should be same as |disk->device_path| because
        // |VolumeManager::OnDiskEvent()| passes the latter to cros-disks as a
        // source path when mounting a device.
        AccessModeMap::iterator it = access_modes_.find(entry.source_path());

        // Store whether the disk was mounted in read-only mode due to a policy.
        disk->set_write_disabled_by_policy(
            it != access_modes_.end() && !disk->is_read_only_hardware()
                && it->second == MOUNT_ACCESS_MODE_READ_ONLY);
        disk->SetMountPath(mount_info.mount_path);
        // Only set the mount path if the disk is actually mounted. Right now, a
        // number of code paths (format, rename, unmount) rely on the mount path
        // being set even if the disk isn't mounted. cros-disks also does some
        // tracking of non-mounted mount paths. Making this change is
        // non-trivial.
        // TODO(amistry): Change these code paths to use device path instead of
        // mount path.
        disk->set_mounted(entry.error_code() == MOUNT_ERROR_NONE);
      }
    }
    // Observers may read the values of disks_. So notify them after tweaking
    // values of disks_.
    NotifyMountStatusUpdate(MOUNTING, entry.error_code(), mount_info);
    if (disk) {
      disk->set_is_first_mount(false);
    }
  }

  // Callback for UnmountPath.
  void OnUnmountPath(UnmountPathCallback callback,
                     const std::string& mount_path,
                     MountError error_code) {
    MountPointMap::iterator mount_points_it = mount_points_.find(mount_path);
    if (mount_points_it == mount_points_.end()) {
      // The path was unmounted, but not as a result of this unmount request,
      // so return error.
      if (!callback.is_null())
        std::move(callback).Run(MOUNT_ERROR_INTERNAL);
      return;
    }

    if (error_code == MOUNT_ERROR_PATH_NOT_MOUNTED ||
        error_code == MOUNT_ERROR_INVALID_PATH) {
      // The path was already unmounted by something else.
      error_code = MOUNT_ERROR_NONE;
    }

    NotifyMountStatusUpdate(
        UNMOUNTING, error_code,
        MountPointInfo(mount_points_it->second.source_path,
                       mount_points_it->second.mount_path,
                       mount_points_it->second.mount_type,
                       mount_points_it->second.mount_condition));

    std::string path(mount_points_it->second.source_path);

    if (error_code == MOUNT_ERROR_NONE)
      mount_points_.erase(mount_points_it);

    DiskMap::iterator disk_iter = disks_.find(path);
    if (disk_iter != disks_.end()) {
      DCHECK(disk_iter->second);
      if (error_code == MOUNT_ERROR_NONE) {
        disk_iter->second->clear_mount_path();
        disk_iter->second->set_mounted(false);
      }
    }

    if (!callback.is_null())
      std::move(callback).Run(error_code);
  }

  void OnUnmountPathForFormat(const std::string& device_path,
                              FormatFileSystemType filesystem,
                              const std::string& label,
                              MountError error_code) {
    if (error_code == MOUNT_ERROR_NONE &&
        disks_.find(device_path) != disks_.end()) {
      FormatUnmountedDevice(device_path, filesystem, label);
    } else {
      OnFormatCompleted(FORMAT_ERROR_UNKNOWN, device_path);
    }
  }

  // Starts device formatting.
  void FormatUnmountedDevice(const std::string& device_path,
                             FormatFileSystemType filesystem,
                             const std::string& label) {
    DiskMap::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->second->mount_path().empty());

    base::UmaHistogramEnumeration("FileBrowser.FormatFileSystemType",
                                  filesystem);

    const std::string filesystem_str = FormatFileSystemTypeToString(filesystem);
    pending_format_changes_[device_path] = {filesystem_str, label};

    cros_disks_client_->Format(
        device_path, filesystem_str, label,
        base::BindOnce(&DiskMountManagerImpl::OnFormatStarted,
                       weak_ptr_factory_.GetWeakPtr(), device_path));
  }

  // Callback for Format.
  void OnFormatStarted(const std::string& device_path, bool success) {
    if (!success) {
      OnFormatCompleted(FORMAT_ERROR_UNKNOWN, device_path);
      return;
    }

    NotifyFormatStatusUpdate(FORMAT_STARTED, FORMAT_ERROR_NONE, device_path);
  }

  // CrosDisksClient::Observer override.
  void OnFormatCompleted(FormatError error_code,
                         const std::string& device_path) override {
    auto iter = disks_.find(device_path);

    // disk might have been removed by now?
    if (iter != disks_.end()) {
      Disk* disk = iter->second.get();
      DCHECK(disk);

      auto pending_change = pending_format_changes_.find(device_path);
      if (pending_change != pending_format_changes_.end() &&
          error_code == FORMAT_ERROR_NONE) {
        disk->set_device_label(pending_change->second.volume_name);
        disk->set_file_system_type(pending_change->second.file_system_type);
      }
    }

    pending_format_changes_.erase(device_path);

    NotifyFormatStatusUpdate(FORMAT_COMPLETED, error_code, device_path);
  }

  void OnUnmountPathForRename(const std::string& device_path,
                              const std::string& volume_name,
                              MountError error_code) {
    if (error_code != MOUNT_ERROR_NONE ||
        disks_.find(device_path) == disks_.end()) {
      OnRenameCompleted(RENAME_ERROR_UNKNOWN, device_path);
      return;
    }

    RenameUnmountedDevice(device_path, volume_name);
  }

  // Start device renaming
  void RenameUnmountedDevice(const std::string& device_path,
                             const std::string& volume_name) {
    DiskMap::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->second->mount_path().empty());

    pending_rename_changes_[device_path] = volume_name;
    cros_disks_client_->Rename(
        device_path, volume_name,
        base::BindOnce(&DiskMountManagerImpl::OnRenameStarted,
                       weak_ptr_factory_.GetWeakPtr(), device_path));
  }

  // Callback for Rename.
  void OnRenameStarted(const std::string& device_path, bool success) {
    if (!success) {
      OnRenameCompleted(RENAME_ERROR_UNKNOWN, device_path);
      return;
    }

    NotifyRenameStatusUpdate(RENAME_STARTED, RENAME_ERROR_NONE, device_path);
  }

  // CrosDisksClient::Observer override.
  void OnRenameCompleted(RenameError error_code,
                         const std::string& device_path) override {
    auto iter = disks_.find(device_path);

    // disk might have been removed by now?
    if (iter != disks_.end()) {
      Disk* disk = iter->second.get();
      DCHECK(disk);

      auto pending_change = pending_rename_changes_.find(device_path);
      if (pending_change != pending_rename_changes_.end() &&
          error_code == RENAME_ERROR_NONE)
        disk->set_device_label(pending_change->second);
    }

    pending_rename_changes_.erase(device_path);

    NotifyRenameStatusUpdate(RENAME_COMPLETED, error_code, device_path);
  }

  // Callback for GetDeviceProperties.
  void OnGetDeviceProperties(const DiskInfo& disk_info) {
    if (disk_info.is_virtual())
      return;

    DVLOG(1) << "Found disk " << disk_info.device_path();
    // Delete previous disk info for this path:
    bool is_new = true;
    bool is_first_mount = false;
    std::string base_mount_path = std::string();
    DiskMap::iterator iter = disks_.find(disk_info.device_path());
    if (iter != disks_.end()) {
      is_first_mount = iter->second->is_first_mount();
      base_mount_path = iter->second->base_mount_path();
      disks_.erase(iter);
      is_new = false;
    }

    // If the device was mounted by the instance, apply recorded parameter.
    // Otherwise, default to false.
    // Lookup by |device_path| which we pass to cros-disks when mounting a
    // device in |VolumeManager::OnDiskEvent()|.
    auto access_mode = access_modes_.find(disk_info.device_path());
    bool write_disabled_by_policy = access_mode != access_modes_.end()
        && access_mode->second == chromeos::MOUNT_ACCESS_MODE_READ_ONLY;
    Disk* disk = new Disk(disk_info, write_disabled_by_policy,
                          base_mount_path);
    if (!is_new) {
      disk->set_is_first_mount(is_first_mount);
    }
    disks_.insert(
        std::make_pair(disk_info.device_path(), base::WrapUnique(disk)));
    NotifyDiskStatusUpdate(is_new ? DISK_ADDED : DISK_CHANGED, *disk);
  }

  // Part of EnsureMountInfoRefreshed(). Called after the list of devices are
  // enumerated.
  void RefreshAfterEnumerateDevices(const std::vector<std::string>& devices) {
    std::set<std::string> current_device_set(devices.begin(), devices.end());
    for (DiskMap::iterator iter = disks_.begin(); iter != disks_.end(); ) {
      if (current_device_set.find(iter->first) == current_device_set.end()) {
        disks_.erase(iter++);
      } else {
        ++iter;
      }
    }
    RefreshDeviceAtIndex(devices, 0);
  }

  // Part of EnsureMountInfoRefreshed(). Called for each device to refresh info.
  void RefreshDeviceAtIndex(const std::vector<std::string>& devices,
                            size_t index) {
    if (index == devices.size()) {
      // All devices info retrieved. Proceed to enumerate mount point info.
      cros_disks_client_->EnumerateMountEntries(
          base::BindOnce(
              &DiskMountManagerImpl::RefreshAfterEnumerateMountEntries,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&DiskMountManagerImpl::RefreshCompleted,
                         weak_ptr_factory_.GetWeakPtr(), false));
      return;
    }

    cros_disks_client_->GetDeviceProperties(
        devices[index],
        base::BindOnce(&DiskMountManagerImpl::RefreshAfterGetDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr(), devices, index + 1),
        base::BindOnce(&DiskMountManagerImpl::RefreshDeviceAtIndex,
                       weak_ptr_factory_.GetWeakPtr(), devices, index + 1));
  }

  // Part of EnsureMountInfoRefreshed().
  void RefreshAfterGetDeviceProperties(const std::vector<std::string>& devices,
                                       size_t next_index,
                                       const DiskInfo& disk_info) {
    OnGetDeviceProperties(disk_info);
    RefreshDeviceAtIndex(devices, next_index);
  }

  // Part of EnsureMountInfoRefreshed(). Called after mount entries are listed.
  void RefreshAfterEnumerateMountEntries(
      const std::vector<MountEntry>& entries) {
    for (size_t i = 0; i < entries.size(); ++i)
      OnMountCompleted(entries[i]);
    RefreshCompleted(true);
  }

  // Part of EnsureMountInfoRefreshed(). Called when the refreshing is done.
  void RefreshCompleted(bool success) {
    already_refreshed_ = true;
    for (size_t i = 0; i < refresh_callbacks_.size(); ++i)
      std::move(refresh_callbacks_[i]).Run(success);
    refresh_callbacks_.clear();
  }

  // CrosDisksClient::Observer override.
  void OnMountEvent(MountEventType event,
                    const std::string& device_path_arg) override {
    // Take a copy of the argument so we can modify it below.
    std::string device_path = device_path_arg;
    switch (event) {
      case CROS_DISKS_DISK_ADDED: {
        cros_disks_client_->GetDeviceProperties(
            device_path,
            base::BindOnce(&DiskMountManagerImpl::OnGetDeviceProperties,
                           weak_ptr_factory_.GetWeakPtr()),
            base::DoNothing());
        break;
      }
      case CROS_DISKS_DISK_REMOVED: {
        // Search and remove disks that are no longer present.
        DiskMountManager::DiskMap::iterator iter = disks_.find(device_path);
        if (iter != disks_.end()) {
          Disk* disk = iter->second.get();
          NotifyDiskStatusUpdate(DISK_REMOVED, *disk);
          disks_.erase(iter);
        }
        break;
      }
      case CROS_DISKS_DEVICE_ADDED: {
        NotifyDeviceStatusUpdate(DEVICE_ADDED, device_path);
        break;
      }
      case CROS_DISKS_DEVICE_REMOVED: {
        NotifyDeviceStatusUpdate(DEVICE_REMOVED, device_path);
        break;
      }
      case CROS_DISKS_DEVICE_SCANNED: {
        NotifyDeviceStatusUpdate(DEVICE_SCANNED, device_path);
        break;
      }
      default: {
        LOG(ERROR) << "Unknown event: " << event;
      }
    }
  }

  // Notifies all observers about disk status update.
  void NotifyDiskStatusUpdate(DiskEvent event, const Disk& disk) {
    for (auto& observer : observers_) {
      disk.is_auto_mountable() ? observer.OnAutoMountableDiskEvent(event, disk)
                               : observer.OnBootDeviceDiskEvent(event, disk);
    }
  }

  // Notifies all observers about device status update.
  void NotifyDeviceStatusUpdate(DeviceEvent event,
                                const std::string& device_path) {
    for (auto& observer : observers_)
      observer.OnDeviceEvent(event, device_path);
  }

  // Notifies all observers about mount completion.
  void NotifyMountStatusUpdate(MountEvent event,
                               MountError error_code,
                               const MountPointInfo& mount_info) {
    for (auto& observer : observers_)
      observer.OnMountEvent(event, error_code, mount_info);
  }

  void NotifyFormatStatusUpdate(FormatEvent event,
                                FormatError error_code,
                                const std::string& device_path) {
    for (auto& observer : observers_)
      observer.OnFormatEvent(event, error_code, device_path);
  }

  void NotifyRenameStatusUpdate(RenameEvent event,
                                RenameError error_code,
                                const std::string& device_path) {
    for (auto& observer : observers_)
      observer.OnRenameEvent(event, error_code, device_path);
  }

  // Mount event change observers.
  base::ObserverList<DiskMountManager::Observer> observers_;

  CrosDisksClient* cros_disks_client_;

  // The list of disks found.
  DiskMountManager::DiskMap disks_;

  DiskMountManager::MountPointMap mount_points_;

  bool already_refreshed_;
  std::vector<EnsureMountInfoRefreshedCallback> refresh_callbacks_;

  std::unique_ptr<SuspendUnmountManager> suspend_unmount_manager_;

  // Whether the instance attempted to mount a device in read-only mode for
  // each source path.
  typedef std::map<std::string, chromeos::MountAccessMode> AccessModeMap;
  AccessModeMap access_modes_;

  base::WeakPtrFactory<DiskMountManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiskMountManagerImpl);
};

}  // namespace

DiskMountManager::Observer::~Observer() {
  DCHECK(!IsInObserverList());
}

bool DiskMountManager::AddDiskForTest(std::unique_ptr<Disk> disk) {
  return false;
}

bool DiskMountManager::AddMountPointForTest(const MountPointInfo& mount_point) {
  return false;
}

// static
std::string DiskMountManager::MountConditionToString(MountCondition condition) {
  switch (condition) {
    case MOUNT_CONDITION_NONE:
      return "";
    case MOUNT_CONDITION_UNKNOWN_FILESYSTEM:
      return "unknown_filesystem";
    case MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM:
      return "unsupported_filesystem";
    default:
      NOTREACHED();
  }
  return "";
}

// static
std::string DiskMountManager::DeviceTypeToString(DeviceType type) {
  switch (type) {
    case DEVICE_TYPE_USB:
      return "usb";
    case DEVICE_TYPE_SD:
      return "sd";
    case DEVICE_TYPE_OPTICAL_DISC:
      return "optical";
    case DEVICE_TYPE_MOBILE:
      return "mobile";
    default:
      return "unknown";
  }
}

// static
void DiskMountManager::Initialize() {
  if (g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager was already initialized";
    return;
  }
  g_disk_mount_manager = new DiskMountManagerImpl();
  VLOG(1) << "DiskMountManager initialized";
}

// static
void DiskMountManager::InitializeForTesting(
    DiskMountManager* disk_mount_manager) {
  if (g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager was already initialized";
    return;
  }
  g_disk_mount_manager = disk_mount_manager;
  VLOG(1) << "DiskMountManager initialized";
}

// static
void DiskMountManager::Shutdown() {
  if (!g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_disk_mount_manager;
  g_disk_mount_manager = NULL;
  VLOG(1) << "DiskMountManager Shutdown completed";
}

// static
DiskMountManager* DiskMountManager::GetInstance() {
  return g_disk_mount_manager;
}

}  // namespace disks
}  // namespace chromeos
