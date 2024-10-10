// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/disk_mount_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/suspend_unmount_manager.h"

namespace ash::disks {
namespace {

using base::BindOnce;

std::string Redact(std::string_view s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

DiskMountManager* g_disk_mount_manager = nullptr;

struct UnmountDeviceRecursivelyCallbackData {
  explicit UnmountDeviceRecursivelyCallbackData(
      DiskMountManager::UnmountDeviceRecursivelyCallbackType in_callback)
      : callback(std::move(in_callback)) {}

  DiskMountManager::UnmountDeviceRecursivelyCallbackType callback;
  MountError error_code = MountError::kSuccess;
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
  NOTREACHED_IN_MIGRATION()
      << "Unknown filesystem type " << static_cast<int>(filesystem);
  return "";
}

// The DiskMountManager implementation.
class DiskMountManagerImpl : public DiskMountManager,
                             public CrosDisksClient::Observer {
 public:
  DiskMountManagerImpl() { cros_disks_client_->AddObserver(this); }

  DiskMountManagerImpl(const DiskMountManagerImpl&) = delete;
  DiskMountManagerImpl& operator=(const DiskMountManagerImpl&) = delete;

  ~DiskMountManagerImpl() override { cros_disks_client_->RemoveObserver(this); }

  using DiskMountManager::Observer;

  // DiskMountManager override.
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  // DiskMountManager override.
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  // DiskMountManager override.
  void RegisterArcDelegate(DiskMountManager::ArcDelegate* delegate) override {
    arc_delegate_ = delegate;
  }

  // DiskMountManager override.
  void UnregisterArcDelegate() override { arc_delegate_ = nullptr; }

  // DiskMountManager override.
  void MountPath(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 MountType type,
                 MountAccessMode access_mode,
                 MountPathCallback callback) override {
    if (const auto [_, ok] =
            mount_callbacks_.try_emplace(source_path, std::move(callback));
        !ok) {
      LOG(ERROR) << "Disk '" << source_path << "' is already being mounted";
      std::move(callback).Run(MountError::kPathAlreadyMounted,
                              {source_path, "", type});
      return;
    }

    const Disks::const_iterator it = disks_.find(source_path);
    if (it != disks_.end()) {
      VLOG(1) << "Disk '" << source_path << "' is already registered";
      DCHECK(*it);
      DCHECK_EQ((*it)->device_path(), source_path);
    } else {
      VLOG(1) << "Disk '" << source_path << "' is not registered yet";
    }

    // Hidden and non-existent devices should not be mounted.
    if (type == MountType::kDevice &&
        (it == disks_.end() || (*it)->is_hidden())) {
      VLOG(1) << "Disk '" << source_path << "' should not be mounted";
      OnMountCompleted({source_path, {}, type, MountError::kInternalError});
      return;
    }

    VLOG(1) << "Mounting '" << source_path << "'...";
    cros_disks_client_->Mount(
        source_path, source_format, mount_label, mount_options, access_mode,
        RemountOption::kMountNewDevice,
        BindOnce(&DiskMountManagerImpl::OnMount, weak_ptr_factory_.GetWeakPtr(),
                 source_path, type));
  }

  // DiskMountManager override.
  void UnmountPath(const std::string& mount_path,
                   UnmountPathCallback callback) override {
    UnmountChildMounts(mount_path);
    VLOG(1) << "Unmounting '" << mount_path << "'...";

    const base::FilePath mount_file_path(mount_path);
    if (arc_delegate_ &&
        cros_disks_client_->GetRemovableDiskMountPoint().IsParent(
            mount_file_path)) {
      VLOG(1) << "Dropping ARC caches for " << Redact(mount_path);
      arc_delegate_->PrepareForRemovableMediaUnmount(
          mount_file_path, base::Seconds(3) /* timeout */,
          BindOnce(&DiskMountManagerImpl::UnmountPathContinue,
                   weak_ptr_factory_.GetWeakPtr(), mount_path,
                   std::move(callback)));
      return;
    }

    UnmountPathContinue(mount_path, std::move(callback), true /* success */);
  }

  void UnmountPathContinue(const std::string& mount_path,
                           UnmountPathCallback callback,
                           bool success) {
    if (!success) {
      LOG(ERROR) << "Cannot drop ARC caches for " << Redact(mount_path);
    }
    cros_disks_client_->Unmount(mount_path,
                                BindOnce(&DiskMountManagerImpl::OnUnmountPath,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback), mount_path));
  }

  void RemountAllRemovableDrives(MountAccessMode mode) override {
    // TODO(yamaguchi): Retry for tentative remount failures. crbug.com/661455
    for (const auto& disk : disks_) {
      DCHECK(disk);
      if (disk->is_read_only_hardware()) {
        // Read-only devices can be mounted in RO mode only. No need to remount.
        continue;
      }
      if (!disk->is_mounted()) {
        continue;
      }
      RemountRemovableDrive(*disk, mode);
    }
  }

  // DiskMountManager override.
  void FormatMountedDevice(const std::string& mount_path,
                           FormatFileSystemType filesystem,
                           const std::string& label) override {
    MountPoints::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      LOG(ERROR) << "Cannot find mount point " << Redact(mount_path);
      // We can't call OnFormatCompleted until |pending_format_changes_| has
      // been populated.
      NotifyFormatStatusUpdate(FORMAT_COMPLETED, FormatError::kUnknownError,
                               mount_path, label);
      return;
    }

    std::string device_path = mount_point->source_path;
    const std::string filesystem_str = FormatFileSystemTypeToString(filesystem);
    pending_format_changes_[device_path] = {filesystem_str, label};

    Disks::const_iterator disk = disks_.find(device_path);
    if (disk == disks_.end()) {
      LOG(ERROR) << "Cannot find device '" << device_path << "'";
      OnFormatCompleted(FormatError::kUnknownError, device_path);
      return;
    }
    if (disk->get()->is_read_only()) {
      LOG(ERROR) << "Device '" << device_path << "' is read-only";
      OnFormatCompleted(FormatError::kDeviceNotAllowed, device_path);
      return;
    }

    if (filesystem == FormatFileSystemType::kUnknown) {
      LOG(ERROR) << "Unknown filesystem passed to FormatMountedDevice";
      OnFormatCompleted(FormatError::kUnsupportedFilesystem, device_path);
      return;
    }

    UnmountPath(disk->get()->mount_path(),
                BindOnce(&DiskMountManagerImpl::OnUnmountPathForFormat,
                         weak_ptr_factory_.GetWeakPtr(), device_path,
                         filesystem, label));
  }

  // DiskMountManager override.
  void SinglePartitionFormatDevice(const std::string& device_path,
                                   FormatFileSystemType filesystem,
                                   const std::string& label) override {
    Disks::const_iterator disk_iter = disks_.find(device_path);
    if (disk_iter == disks_.end()) {
      LOG(ERROR) << "Cannot find device '" << device_path << "'";
      OnPartitionCompleted(device_path, filesystem, label,
                           PartitionError::kInvalidDevicePath);
      return;
    }

    UnmountDeviceRecursively(
        device_path,
        BindOnce(&DiskMountManagerImpl::OnUnmountDeviceForSinglePartitionFormat,
                 weak_ptr_factory_.GetWeakPtr(), device_path, filesystem,
                 label));
  }

  void RenameMountedDevice(const std::string& mount_path,
                           const std::string& volume_name) override {
    MountPoints::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      LOG(ERROR) << "Cannot find mount point '" << mount_path << "'";
      // We can't call OnRenameCompleted until |pending_rename_changes_| has
      // been populated.
      NotifyRenameStatusUpdate(RENAME_COMPLETED, RenameError::kUnknownError,
                               mount_path, volume_name);
      return;
    }

    std::string device_path = mount_point->source_path;
    pending_rename_changes_[device_path] = volume_name;

    Disks::const_iterator iter = disks_.find(device_path);
    if (iter == disks_.end()) {
      LOG(ERROR) << "Cannot find device '" << device_path << "'";
      OnRenameCompleted(RenameError::kUnknownError, device_path);
      return;
    }

    if (iter->get()->is_read_only()) {
      LOG(ERROR) << "Device '" << device_path << "' is read-only";
      OnRenameCompleted(RenameError::kDeviceNotAllowed, device_path);
      return;
    }

    UnmountPath(
        iter->get()->mount_path(),
        BindOnce(&DiskMountManagerImpl::OnUnmountPathForRename,
                 weak_ptr_factory_.GetWeakPtr(), device_path, volume_name));
  }

  // DiskMountManager override.
  void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) override {
    std::vector<std::string> devices_to_unmount;

    // Get list of all devices to unmount.
    for (const auto& disk : disks_) {
      DCHECK(disk);
      if (!disk->mount_path().empty() &&
          base::StartsWith(disk->device_path(), device_path)) {
        devices_to_unmount.push_back(disk->mount_path());
      }
    }

    // Is there anything to unmount?
    if (devices_to_unmount.empty()) {
      const auto it = disks_.find(device_path);
      if (it == disks_.end()) {
        LOG(ERROR) << "Cannot find device '" << device_path << "'";
        std::move(callback).Run(MountError::kInvalidDevicePath);
        return;
      }

      // Nothing to unmount.
      DCHECK(*it);
      DCHECK_EQ((*it)->device_path(), device_path);
      DCHECK_EQ((*it)->mount_path(), "");
      LOG(WARNING) << "Disk '" << device_path << "' is already unmounted";
      std::move(callback).Run(MountError::kSuccess);
      return;
    }

    // There is something to unmount.
    std::unique_ptr<UnmountDeviceRecursivelyCallbackData> cb_data =
        std::make_unique<UnmountDeviceRecursivelyCallbackData>(
            std::move(callback));
    UnmountDeviceRecursivelyCallbackData* raw_cb_data = cb_data.get();
    base::RepeatingClosure done_callback = base::BarrierClosure(
        devices_to_unmount.size(),
        BindOnce(&OnAllUnmountDeviceRecursively, std::move(cb_data)));

    for (const std::string& device : devices_to_unmount) {
      VLOG(1) << "Unmounting '" << device << "'...";
      cros_disks_client_->Unmount(
          device, BindOnce(&DiskMountManagerImpl::OnUnmountDeviceRecursively,
                           weak_ptr_factory_.GetWeakPtr(), raw_cb_data, device,
                           done_callback));
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
          BindOnce(&DiskMountManagerImpl::RefreshAfterEnumerateDevices,
                   weak_ptr_factory_.GetWeakPtr()),
          BindOnce(&DiskMountManagerImpl::RefreshCompleted,
                   weak_ptr_factory_.GetWeakPtr(), false));
    }
  }

  // DiskMountManager override.
  const Disks& disks() const override { return disks_; }

  // DiskMountManager override.
  const Disk* FindDiskBySourcePath(
      const std::string& source_path) const override {
    const Disks::const_iterator it = disks_.find(source_path);
    return it == disks_.end() ? nullptr : it->get();
  }

  // DiskMountManager override.
  const MountPoints& mount_points() const override { return mount_points_; }

  // DiskMountManager override.
  bool AddDiskForTest(std::unique_ptr<Disk> disk) override {
    const auto [it, ok] = disks_.insert(std::move(disk));
    LOG_IF(ERROR, !ok) << "Cannot add a duplicate disk '"
                       << (*it)->device_path() << "'";
    return ok;
  }

  // DiskMountManager override.
  // Corresponding disk should be added to the manager before this is called.
  bool AddMountPointForTest(const MountPoint& mount_point) override {
    if (mount_point.mount_type == MountType::kDevice &&
        disks_.count(mount_point.source_path) == 0) {
      LOG(ERROR) << "Device mount point '" << mount_point.mount_path
                 << "' should have a disk entry '" << mount_point.source_path
                 << "'";
      return false;
    }

    const auto [it, ok] = mount_points_.insert(mount_point);
    DCHECK_EQ(it->mount_path, mount_point.mount_path);
    LOG_IF(ERROR, !ok) << "Cannot add a duplicate mount point '"
                       << mount_point.mount_path << "'";
    return ok;
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

  // Stores device path are being partitioning.
  // It allows preventing auto-mount of the disks in this set.
  std::set<std::string> pending_partitioning_disks_;

  // Stores new volume name for a device on which renaming is invoked on, so
  // that OnRenameCompleted can set it back to |disks_|. The key is a
  // device_path and the value is new volume_name.
  std::map<std::string, std::string> pending_rename_changes_;

  // Called on D-Bus CrosDisksClient::Mount() is done.
  void OnMount(const std::string& source_path, MountType type, bool result) {
    // When succeeds, OnMountCompleted will be called by "MountCompleted",
    // signal instead. Do nothing now.
    if (result) {
      return;
    }

    OnMountCompleted({source_path, {}, type, MountError::kInternalError});
  }

  void RemountRemovableDrive(const Disk& disk, MountAccessMode access_mode) {
    const std::string& mount_path = disk.mount_path();
    MountPoints::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      // Not in mount_points_. This happens when the mount_points and disks_ are
      // inconsistent.
      LOG(ERROR) << "Cannot find mount point " << Redact(mount_path);
      OnMountCompleted({disk.device_path(), mount_path, MountType::kDevice,
                        MountError::kPathNotMounted});
      return;
    }

    cros_disks_client_->Mount(
        mount_point->source_path, std::string(), std::string(), {}, access_mode,
        RemountOption::kRemountExistingDevice,
        BindOnce(&DiskMountManagerImpl::OnMount, weak_ptr_factory_.GetWeakPtr(),
                 mount_point->source_path, mount_point->mount_type));
  }

  // Unmounts all mount points whose source path is transitively parented by
  // |mount_path|.
  void UnmountChildMounts(std::string mount_path) {
    DCHECK(!mount_path.empty());

    // Let's make sure mount path has trailing slash.
    if (mount_path.back() != '/') {
      mount_path += '/';
    }

    // Paths to unmount, indexed by source path.
    std::map<std::string, std::string> paths_to_unmount;

    // For the already known mount points, use the mount path.
    for (const MountPoint& mount_point : mount_points_) {
      if (base::StartsWith(mount_point.source_path, mount_path)) {
        paths_to_unmount.try_emplace(mount_point.source_path,
                                     mount_point.mount_path);
      }
    }

    // For the mount points that are not registered yet, use the source path.
    for (const auto& [source_path, _] : mount_callbacks_) {
      if (base::StartsWith(source_path, mount_path)) {
        paths_to_unmount.try_emplace(source_path, source_path);
      }
    }

    for (const auto& [_, path_to_unmount] : paths_to_unmount) {
      UnmountPath(path_to_unmount, {});
    }
  }

  // Callback for UnmountDeviceRecursively.
  void OnUnmountDeviceRecursively(UnmountDeviceRecursivelyCallbackData* cb_data,
                                  const std::string& mount_path,
                                  base::OnceClosure done_callback,
                                  const MountError error) {
    if (error == MountError::kPathNotMounted ||
        error == MountError::kInvalidPath || error == MountError::kSuccess) {
      // Do standard processing for Unmount event.
      OnUnmountPath(UnmountPathCallback(), mount_path, error);
    } else {
      LOG(ERROR) << "Cannot unmount " << Redact(mount_path) << ": " << error;
      // This causes the last non-success error to be reported.
      cb_data->error_code = error;
    }

    std::move(done_callback).Run();
  }

  // CrosDisksClient::Observer override.
  void OnMountCompleted(const MountPoint& entry) override {
    if (const auto it = deferred_mount_events_.find(entry.source_path);
        it != deferred_mount_events_.end()) {
      it->second.push_back(entry);
      VLOG(1) << "Added mount_path '" << entry.mount_path
              << "' to deferred mount events";
      return;
    }

    bool want_to_keep = entry.mount_error == MountError::kSuccess;
    MountError mount_error = MountError::kSuccess;
    if (entry.mount_type == MountType::kDevice) {
      if (entry.mount_error == MountError::kUnknownFilesystem) {
        mount_error = MountError::kUnknownFilesystem;
        want_to_keep = true;
      } else if (entry.mount_error == MountError::kUnsupportedFilesystem) {
        mount_error = MountError::kUnsupportedFilesystem;
        want_to_keep = true;
      }
    }

    const MountPoint mount_info{
        entry.source_path, entry.mount_path, entry.mount_type, mount_error, 100,
        entry.read_only};

    // If the device is corrupted but it's still possible to format it, it will
    // be fake mounted.
    if (want_to_keep) {
      VLOG(1) << "Mounted '" << mount_info.source_path << "' as '"
              << mount_info.mount_path << "'";
      const auto [it, ok] = mount_points_.insert(mount_info);
      if (!ok) {
        DCHECK_EQ(it->mount_path, mount_info.mount_path);
        // const_cast is Ok since we're not modifying it->mount_path.
        const_cast<MountPoint&>(*it) = mount_info;
        VLOG(1) << "Updated mount point '" << mount_info.mount_path << "'";
      }
    } else {
      if (base::SysInfo::IsRunningOnChromeOS()) {
        LOG(ERROR) << "Cannot mount " << Redact(mount_info.source_path)
                   << " as " << Redact(mount_info.mount_path) << ": "
                   << entry.mount_error;
      }
      if (const MountPoints::const_iterator it =
              mount_points_.find(mount_info.mount_path);
          it != mount_points_.end()) {
        VLOG(1) << "Removed mount point '" << mount_info.mount_path << "'";
        mount_points_.erase(it);
      }
    }

    const Disks::const_iterator disk_it = disks_.find(mount_info.source_path);
    Disk* const disk = disk_it != disks_.end() ? disk_it->get() : nullptr;

    if (want_to_keep && mount_info.mount_type == MountType::kDevice &&
        !mount_info.source_path.empty() && !mount_info.mount_path.empty() &&
        disk) {
      DCHECK(disk);
      DCHECK_EQ(disk->device_path(), mount_info.source_path);

      // Store whether the disk was mounted in read-only mode due to a policy.
      disk->set_write_disabled_by_policy(!disk->is_read_only_hardware() &&
                                         entry.read_only);

      // Right now, a number of operations (format, rename, unmount) rely on the
      // mount path being set even if the disk isn't mounted. cros-disks also
      // does some tracking of non-mounted mount paths.
      disk->SetMountPath(mount_info.mount_path);
      disk->set_mounted(entry.mount_error == MountError::kSuccess);
    }

    // Observers may read the values of disks_. So notify them after tweaking
    // values of disks_.
    if (auto it = mount_callbacks_.find(entry.source_path);
        it != mount_callbacks_.end()) {
      DCHECK_EQ(it->first, entry.source_path);
      VLOG(1) << "Calling mount callback for '" << entry.source_path
              << "' with result = " << entry.mount_error;
      std::move(it->second).Run(entry.mount_error, mount_info);
      mount_callbacks_.erase(std::move(it));
    } else {
      VLOG(1) << "No mount callback for " << Redact(entry.source_path);
    }

    NotifyMountStatusUpdate(MOUNTING, entry.mount_error, mount_info);

    if (disk) {
      DCHECK(disk_it != disks_.end());
      disk->set_is_first_mount(false);
      if (!want_to_keep) {
        VLOG(1) << "Removed Disk '" << disk->device_path() << "'";
        disks_.erase(disk_it);
      }
    }
  }

  // CrosDisksClient::Observer override.
  void OnMountProgress(const MountPoint& entry) override {
    DCHECK_EQ(entry.mount_error, MountError::kInProgress);

    const auto [it, ok] = mount_points_.insert(
        {entry.source_path, entry.mount_path, entry.mount_type,
         MountError::kInProgress, entry.progress_percent, entry.read_only});
    if (ok) {
      DCHECK_EQ(it->mount_path, entry.mount_path);
      VLOG(1) << "Added in-progress mount point '" << entry.mount_path
              << "' with " << entry.progress_percent << "%";
      return;
    }

    // const_cast is Ok since we're not modifying it->mount_path.
    MountPoint& mount_point = const_cast<MountPoint&>(*it);
    DCHECK_EQ(mount_point.mount_path, entry.mount_path);
    DCHECK_EQ(mount_point.source_path, entry.source_path);
    DCHECK_EQ(mount_point.mount_type, entry.mount_type);
    DCHECK_EQ(mount_point.mount_error, MountError::kInProgress);
    DCHECK_EQ(mount_point.read_only, entry.read_only);

    mount_point.progress_percent = entry.progress_percent;
    VLOG(1) << "Updated in-progress mount point '" << entry.mount_path
            << "' to " << entry.progress_percent << "%";
  }

  // Callback for UnmountPath.
  void OnUnmountPath(UnmountPathCallback callback,
                     const std::string& mount_path,
                     MountError error) {
    if (error == MountError::kSuccess) {
      VLOG(1) << "Unmounted '" << mount_path << "'";
    } else {
      LOG(ERROR) << "Cannot unmount " << Redact(mount_path) << ": " << error;
      if (error == MountError::kPathNotMounted ||
          error == MountError::kInvalidPath) {
        // The path was already unmounted by something else.
        error = MountError::kSuccess;
      }
    }

    if (const MountPoints::const_iterator mount_point =
            mount_points_.find(mount_path);
        mount_point != mount_points_.end()) {
      NotifyMountStatusUpdate(UNMOUNTING, error, *mount_point);

      if (error == MountError::kSuccess) {
        if (const Disks::const_iterator disk =
                disks_.find(mount_point->source_path);
            disk != disks_.end()) {
          DCHECK(*disk);
          (*disk)->clear_mount_path();
          (*disk)->set_mounted(false);
        }

        mount_points_.erase(mount_point);
      }
    }

    if (callback) {
      std::move(callback).Run(error);
    }
  }

  void OnUnmountPathForFormat(const std::string& device_path,
                              FormatFileSystemType filesystem,
                              const std::string& label,
                              MountError error_code) {
    if (error_code == MountError::kSuccess && disks_.count(device_path) != 0) {
      FormatUnmountedDevice(device_path, filesystem, label);
    } else {
      OnFormatCompleted(FormatError::kUnknownError, device_path);
    }
  }

  void OnUnmountDeviceForSinglePartitionFormat(const std::string& device_path,
                                               FormatFileSystemType filesystem,
                                               const std::string& label,
                                               MountError error_code) {
    if (error_code != MountError::kSuccess || disks_.count(device_path) == 0) {
      OnPartitionCompleted(device_path, filesystem, label,
                           PartitionError::kUnknownError);
      return;
    }

    SinglePartitionFormatUnmountedDevice(device_path, filesystem, label);
  }

  // Starts device formatting.
  void FormatUnmountedDevice(const std::string& device_path,
                             FormatFileSystemType filesystem,
                             const std::string& label) {
    Disks::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->get()->mount_path().empty());

    base::UmaHistogramEnumeration("FileBrowser.FormatFileSystemType",
                                  filesystem);

    cros_disks_client_->Format(
        device_path, FormatFileSystemTypeToString(filesystem), label,
        BindOnce(&DiskMountManagerImpl::OnFormatStarted,
                 weak_ptr_factory_.GetWeakPtr(), device_path, label));
  }

  // Callback for Format.
  void OnFormatStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) {
    if (!success) {
      OnFormatCompleted(FormatError::kUnknownError, device_path);
      return;
    }

    NotifyFormatStatusUpdate(FORMAT_STARTED, FormatError::kSuccess, device_path,
                             device_label);
  }

  // CrosDisksClient::Observer override.
  void OnFormatCompleted(FormatError error_code,
                         const std::string& device_path) override {
    std::string device_label;
    auto pending_change = pending_format_changes_.find(device_path);
    if (pending_change != pending_format_changes_.end()) {
      device_label = pending_change->second.volume_name;
    }

    auto iter = disks_.find(device_path);

    // disk might have been removed by now?
    if (iter != disks_.end()) {
      Disk* const disk = iter->get();
      DCHECK(disk);

      if (pending_change != pending_format_changes_.end() &&
          error_code == FormatError::kSuccess) {
        disk->set_device_label(pending_change->second.volume_name);
        disk->set_file_system_type(pending_change->second.file_system_type);
      }
    }

    pending_format_changes_.erase(device_path);

    EnsureMountInfoRefreshed(base::DoNothing(), true /* force */);

    NotifyFormatStatusUpdate(FORMAT_COMPLETED, error_code, device_path,
                             device_label);
  }

  void SinglePartitionFormatUnmountedDevice(const std::string& device_path,
                                            FormatFileSystemType filesystem,
                                            const std::string& label) {
    Disks::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->get()->mount_path().empty());

    pending_partitioning_disks_.insert(disk->get()->device_path());

    NotifyPartitionStatusUpdate(PARTITION_STARTED, PartitionError::kSuccess,
                                device_path, label);

    cros_disks_client_->SinglePartitionFormat(
        disk->get()->file_path(),
        BindOnce(&DiskMountManagerImpl::OnPartitionCompleted,
                 weak_ptr_factory_.GetWeakPtr(), device_path, filesystem,
                 label));
  }

  void OnPartitionCompleted(const std::string& device_path,
                            FormatFileSystemType filesystem,
                            const std::string& label,
                            PartitionError error_code) {
    auto iter = disks_.find(device_path);

    // disk might have been removed by now?
    if (iter != disks_.end()) {
      Disk* const disk = iter->get();
      DCHECK(disk);

      if (error_code == PartitionError::kSuccess) {
        EnsureMountInfoRefreshed(
            BindOnce(&DiskMountManagerImpl::OnRefreshAfterPartition,
                     weak_ptr_factory_.GetWeakPtr(), device_path, filesystem,
                     label),
            true /* force */);
      }

    } else {
      // Remove disk from pending partitioning list if disk removed.
      pending_partitioning_disks_.erase(device_path);
    }

    NotifyPartitionStatusUpdate(PARTITION_COMPLETED, error_code, device_path,
                                label);
  }

  void OnRefreshAfterPartition(const std::string& device_path,
                               FormatFileSystemType filesystem,
                               const std::string& label,
                               bool success) {
    Disks::const_iterator device_disk = disks_.find(device_path);
    if (device_disk == disks_.end()) {
      LOG(ERROR) << "Device not found, maybe ejected";
      pending_partitioning_disks_.erase(device_path);
      NotifyPartitionStatusUpdate(PARTITION_COMPLETED,
                                  PartitionError::kInvalidDevicePath,
                                  device_path, label);
      return;
    }

    std::string new_partition_device_path;
    // Find new partition using common storage path with parent device.
    for (const auto& candidate : disks_) {
      if (candidate->storage_device_path() ==
              device_disk->get()->storage_device_path() &&
          !candidate->is_parent()) {
        new_partition_device_path = candidate->device_path();
        break;
      }
    }

    if (new_partition_device_path.empty()) {
      LOG(ERROR) << "New partition couldn't be found";
      pending_partitioning_disks_.erase(device_path);
      NotifyPartitionStatusUpdate(PARTITION_COMPLETED,
                                  PartitionError::kInvalidDevicePath,
                                  device_path, label);
      return;
    }

    const std::string filesystem_str = FormatFileSystemTypeToString(filesystem);
    pending_format_changes_[new_partition_device_path] = {filesystem_str,
                                                          label};

    // It's expected the disks (parent device and new partition) are not
    // mounted, but try unmounting before starting format if it got
    // mounted through another flow.
    UnmountDeviceRecursively(
        device_path, BindOnce(&DiskMountManagerImpl::OnUnmountPathForFormat,
                              weak_ptr_factory_.GetWeakPtr(),
                              new_partition_device_path, filesystem, label));

    // It's ok to remove it from pending partitioning as format flow started.
    pending_partitioning_disks_.erase(device_path);
  }

  void OnUnmountPathForRename(const std::string& device_path,
                              const std::string& volume_name,
                              MountError error_code) {
    if (error_code != MountError::kSuccess || disks_.count(device_path) == 0) {
      OnRenameCompleted(RenameError::kUnknownError, device_path);
      return;
    }

    RenameUnmountedDevice(device_path, volume_name);
  }

  // Start device renaming
  void RenameUnmountedDevice(const std::string& device_path,
                             const std::string& volume_name) {
    const Disks::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->get()->mount_path().empty());

    cros_disks_client_->Rename(
        device_path, volume_name,
        BindOnce(&DiskMountManagerImpl::OnRenameStarted,
                 weak_ptr_factory_.GetWeakPtr(), device_path, volume_name));
  }

  // Callback for Rename.
  void OnRenameStarted(const std::string& device_path,
                       const std::string& volume_name,
                       bool success) {
    if (!success) {
      OnRenameCompleted(RenameError::kUnknownError, device_path);
      return;
    }

    NotifyRenameStatusUpdate(RENAME_STARTED, RenameError::kSuccess, device_path,
                             volume_name);
  }

  // CrosDisksClient::Observer override.
  void OnRenameCompleted(RenameError error_code,
                         const std::string& device_path) override {
    std::string device_label;
    auto pending_change = pending_rename_changes_.find(device_path);
    if (pending_change != pending_rename_changes_.end()) {
      device_label = pending_change->second;
    }

    auto iter = disks_.find(device_path);

    // disk might have been removed by now?
    if (iter != disks_.end()) {
      Disk* const disk = iter->get();
      DCHECK(disk);

      if (pending_change != pending_rename_changes_.end() &&
          error_code == RenameError::kSuccess) {
        disk->set_device_label(pending_change->second);
      }
    }

    pending_rename_changes_.erase(device_path);

    NotifyRenameStatusUpdate(RENAME_COMPLETED, error_code, device_path,
                             device_label);
  }

  // Fire observer mount events that were deferred due to an in-progress
  // GetDeviceProperties() call.
  void RunDeferredMountEvents(const std::string& device_path) {
    auto mount_events_iter = deferred_mount_events_.find(device_path);
    if (mount_events_iter == deferred_mount_events_.end()) {
      return;
    }
    std::vector<MountPoint> entries = std::move(mount_events_iter->second);
    deferred_mount_events_.erase(mount_events_iter);
    for (const MountPoint& entry : entries) {
      OnMountCompleted(entry);
    }
  }

  // Callback for GetDeviceProperties.
  void OnGetDeviceProperties(const DiskInfo& disk_info) {
    if (disk_info.is_virtual()) {
      RunDeferredMountEvents(disk_info.device_path());
      return;
    }

    VLOG(1) << "Found disk '" << disk_info.device_path() << "'";
    // Delete previous disk info for this path:
    bool is_new = true;
    bool is_first_mount = false;
    std::string base_mount_path = std::string();

    if (Disks::iterator it = disks_.find(disk_info.device_path());
        it != disks_.end()) {
      is_first_mount = (*it)->is_first_mount();
      base_mount_path = (*it)->base_mount_path();
      disks_.erase(std::move(it));
      is_new = false;
    }

    // If the device was mounted by the instance, apply recorded parameter.
    // Otherwise, default to false.
    // Lookup by |device_path| which we pass to cros-disks when mounting a
    // device in |VolumeManager::OnDiskEvent()|.
    const MountPoints::const_iterator mount_point =
        mount_points_.find(disk_info.mount_path());
    const bool write_disabled_by_policy =
        mount_point != mount_points_.end() && mount_point->read_only;
    std::unique_ptr<Disk> disk = std::make_unique<Disk>(
        disk_info, write_disabled_by_policy, base_mount_path);
    if (!is_new) {
      disk->set_is_first_mount(is_first_mount);
    }

    const auto [it, ok] = disks_.insert(std::move(disk));
    DCHECK(ok);
    NotifyDiskStatusUpdate(is_new ? DISK_ADDED : DISK_CHANGED, **it);
    RunDeferredMountEvents(disk_info.device_path());
  }

  // Part of EnsureMountInfoRefreshed(). Called after the list of devices are
  // enumerated.
  void RefreshAfterEnumerateDevices(const std::vector<std::string>& devices) {
    std::set<std::string> current_device_set(devices.begin(), devices.end());
    for (Disks::iterator iter = disks_.begin(); iter != disks_.end();) {
      if (current_device_set.count(iter->get()->device_path()) == 0) {
        iter = disks_.erase(iter);
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
          BindOnce(&DiskMountManagerImpl::RefreshAfterEnumerateMountEntries,
                   weak_ptr_factory_.GetWeakPtr()),
          BindOnce(&DiskMountManagerImpl::RefreshCompleted,
                   weak_ptr_factory_.GetWeakPtr(), false));
      return;
    }

    cros_disks_client_->GetDeviceProperties(
        devices[index],
        BindOnce(&DiskMountManagerImpl::RefreshAfterGetDeviceProperties,
                 weak_ptr_factory_.GetWeakPtr(), devices, index + 1),
        BindOnce(&DiskMountManagerImpl::RefreshDeviceAtIndex,
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
      const std::vector<MountPoint>& entries) {
    for (const MountPoint& entry : entries) {
      OnMountCompleted(entry);
    }
    RefreshCompleted(true);
  }

  // Part of EnsureMountInfoRefreshed(). Called when the refreshing is done.
  void RefreshCompleted(bool success) {
    already_refreshed_ = true;
    for (auto& callback : refresh_callbacks_) {
      std::move(callback).Run(success);
    }
    refresh_callbacks_.clear();
  }

  // CrosDisksClient::Observer override.
  void OnMountEvent(const MountEventType event,
                    const std::string& device_path) override {
    VLOG(1) << "OnMountEvent: " << event << " for '" << device_path << "'";

    switch (event) {
      case MountEventType::kDiskAdded:
        // Ensure we have an entry indicating we're waiting for
        // GetDeviceProperties() to complete.
        deferred_mount_events_[device_path];
        cros_disks_client_->GetDeviceProperties(
            device_path,
            BindOnce(&DiskMountManagerImpl::OnGetDeviceProperties,
                     weak_ptr_factory_.GetWeakPtr()),
            base::DoNothing());
        return;

      case MountEventType::kDiskRemoved:
        // Search and remove disks that are no longer present.
        if (Disks::const_iterator it = disks_.find(device_path);
            it != disks_.end()) {
          DCHECK(*it);
          NotifyDiskStatusUpdate(DISK_REMOVED, **it);
          disks_.erase(std::move(it));
        }
        return;

      case MountEventType::kDeviceAdded:
        NotifyDeviceStatusUpdate(DEVICE_ADDED, device_path);
        return;

      case MountEventType::kDeviceRemoved:
        NotifyDeviceStatusUpdate(DEVICE_REMOVED, device_path);
        return;

      case MountEventType::kDeviceScanned:
        NotifyDeviceStatusUpdate(DEVICE_SCANNED, device_path);
        return;

      case MountEventType::kDiskChanged:
        break;
    }

    LOG(ERROR) << "Unexpected mount event " << event << " for '" << device_path
               << "'";
  }

  // Notifies all observers about disk status update.
  void NotifyDiskStatusUpdate(DiskEvent event, const Disk& disk) {
    for (Observer& observer : observers_) {
      // Skip mounting of new partitioned disks while waiting for the format.
      if (IsPendingPartitioningDisk(disk.device_path())) {
        continue;
      }
      disk.is_auto_mountable() ? observer.OnAutoMountableDiskEvent(event, disk)
                               : observer.OnBootDeviceDiskEvent(event, disk);
    }
  }

  // Notifies all observers about device status update.
  void NotifyDeviceStatusUpdate(DeviceEvent event,
                                const std::string& device_path) {
    for (Observer& observer : observers_) {
      observer.OnDeviceEvent(event, device_path);
    }
  }

  // Notifies all observers about mount completion.
  void NotifyMountStatusUpdate(MountEvent event,
                               MountError error_code,
                               const MountPoint& mount_info) {
    for (Observer& observer : observers_) {
      observer.OnMountEvent(event, error_code, mount_info);
    }
  }

  void NotifyFormatStatusUpdate(FormatEvent event,
                                FormatError error_code,
                                const std::string& device_path,
                                const std::string& device_label) {
    for (Observer& observer : observers_) {
      observer.OnFormatEvent(event, error_code, device_path, device_label);
    }
  }

  void NotifyPartitionStatusUpdate(PartitionEvent event,
                                   PartitionError error_code,
                                   const std::string& device_path,
                                   const std::string& device_label) {
    for (Observer& observer : observers_) {
      observer.OnPartitionEvent(event, error_code, device_path, device_label);
    }
  }

  void NotifyRenameStatusUpdate(RenameEvent event,
                                RenameError error_code,
                                const std::string& device_path,
                                const std::string& device_label) {
    for (Observer& observer : observers_) {
      observer.OnRenameEvent(event, error_code, device_path, device_label);
    }
  }

  bool IsPendingPartitioningDisk(const std::string& device_path) {
    if (base::Contains(pending_partitioning_disks_, device_path)) {
      return true;
    }

    // If device path doesn't match check whether if it's a child path.
    for (const auto& disk : pending_partitioning_disks_) {
      if (base::StartsWith(device_path, disk, base::CompareCase::SENSITIVE)) {
        return true;
      }
    }
    return false;
  }

  // Mount event change observers.
  base::ObserverList<Observer> observers_;

  raw_ptr<DiskMountManager::ArcDelegate> arc_delegate_;

  const raw_ptr<CrosDisksClient> cros_disks_client_ = CrosDisksClient::Get();

  // The list of disks found.
  Disks disks_;

  // Callbacks of mount points in the process of being mounted, indexed by
  // source path.
  using MountCallbacks = std::map<std::string, MountPathCallback>;
  MountCallbacks mount_callbacks_;

  // Known mount points.
  MountPoints mount_points_;

  // A map entry with a key of the device path will be created upon calling
  // GetDeviceProperties(), for deferring mount events, and removed once it has
  // completed. This prevents a race resulting in mount events being fired with
  // the corresponding Disk entry unexpectedly missing.
  std::map<std::string, std::vector<MountPoint>> deferred_mount_events_;

  bool already_refreshed_ = false;
  std::vector<EnsureMountInfoRefreshedCallback> refresh_callbacks_;

  SuspendUnmountManager suspend_unmount_manager_{this};

  base::WeakPtrFactory<DiskMountManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

DiskMountManager::Observer::~Observer() {
  DCHECK(!IsInObserverList());
}

bool DiskMountManager::AddDiskForTest(std::unique_ptr<Disk> disk) {
  return false;
}

bool DiskMountManager::AddMountPointForTest(const MountPoint& mount_point) {
  return false;
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
  g_disk_mount_manager = nullptr;
  VLOG(1) << "DiskMountManager Shutdown completed";
}

// static
DiskMountManager* DiskMountManager::GetInstance() {
  return g_disk_mount_manager;
}

}  // namespace ash::disks
