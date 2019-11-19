// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {
namespace disks {

class Disk;

// Condition of mounted filesystem.
enum MountCondition {
  MOUNT_CONDITION_NONE,
  MOUNT_CONDITION_UNKNOWN_FILESYSTEM,
  MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM,
};

// Possible filesystem types that can be passed to FormatMountedDevice.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FormatFileSystemType {
  kUnknown = 0,
  kVfat = 1,
  kExfat = 2,
  kNtfs = 3,
  kMaxValue = kNtfs,
};

// This class handles the interaction with cros-disks.
// Other classes can add themselves as observers.
class COMPONENT_EXPORT(CHROMEOS_DISKS) DiskMountManager {
 public:
  // Event types passed to the observers.
  enum DiskEvent {
    DISK_ADDED,
    DISK_REMOVED,
    DISK_CHANGED,
  };

  enum DeviceEvent {
    DEVICE_ADDED,
    DEVICE_REMOVED,
    DEVICE_SCANNED,
  };

  enum MountEvent {
    MOUNTING,
    UNMOUNTING,
  };

  enum FormatEvent {
    FORMAT_STARTED,
    FORMAT_COMPLETED
  };

  enum RenameEvent { RENAME_STARTED, RENAME_COMPLETED };

  typedef std::map<std::string, std::unique_ptr<Disk>> DiskMap;

  // A struct to store information about mount point.
  struct MountPointInfo {
    // Device's path.
    std::string source_path;
    // Mounted path.
    std::string mount_path;
    // Type of mount.
    MountType mount_type;
    // Condition of mount.
    MountCondition mount_condition;

    MountPointInfo(const std::string& source,
                   const std::string& mount,
                   const MountType type,
                   MountCondition condition)
        : source_path(source),
          mount_path(mount),
          mount_type(type),
          mount_condition(condition) {
    }
  };

  // MountPointMap key is mount_path.
  typedef std::map<std::string, MountPointInfo> MountPointMap;

  // A callback function type which is called after UnmountDeviceRecursively
  // finishes.
  typedef base::OnceCallback<void(MountError error_code)>
      UnmountDeviceRecursivelyCallbackType;

  // A callback type for UnmountPath method.
  typedef base::OnceCallback<void(MountError error_code)> UnmountPathCallback;

  // A callback type for EnsureMountInfoRefreshed method.
  typedef base::OnceCallback<void(bool success)>
      EnsureMountInfoRefreshedCallback;

  // Implement this interface to be notified about disk/mount related events.
  class Observer : public base::CheckedObserver {
   public:
    // Called when auto-mountable disk mount status is changed.
    virtual void OnAutoMountableDiskEvent(DiskEvent event, const Disk& disk) {}
    // Called when fixed storage disk status is changed.
    virtual void OnBootDeviceDiskEvent(DiskEvent event, const Disk& disk) {}
    // Called when device status is changed.
    virtual void OnDeviceEvent(DeviceEvent event,
                               const std::string& device_path) {}
    // Called after a mount point has been mounted or unmounted.
    virtual void OnMountEvent(MountEvent event,
                              MountError error_code,
                              const MountPointInfo& mount_info) {}
    // Called on format process events.
    virtual void OnFormatEvent(FormatEvent event,
                               FormatError error_code,
                               const std::string& device_path) {}
    // Called on rename process events.
    virtual void OnRenameEvent(RenameEvent event,
                               RenameError error_code,
                               const std::string& device_path) {}

   protected:
    ~Observer() override;
  };

  virtual ~DiskMountManager() {}

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the list of disks found.
  virtual const DiskMap& disks() const = 0;

  // Returns Disk object corresponding to |source_path| or NULL on failure.
  virtual const Disk* FindDiskBySourcePath(
      const std::string& source_path) const = 0;

  // Gets the list of mount points.
  virtual const MountPointMap& mount_points() const = 0;

  // Refreshes all the information about mounting if it is not yet done and
  // invokes |callback| when finished. If the information is already refreshed
  // and |force| is false, it just runs |callback| immediately.
  virtual void EnsureMountInfoRefreshed(
      EnsureMountInfoRefreshedCallback callback,
      bool force) = 0;

  // Mounts a device or an archive file.
  // |source_path| specifies either a device or an archive file path.
  // When |type|=MOUNT_TYPE_ARCHIVE, caller may set two optional arguments:
  // |source_format| and |mount_label|. See CrosDisksClient::Mount for detail.
  // |access_mode| specifies read-only or read-write mount mode for a device.
  // Note that the mount operation may fail. To find out the result, one should
  // observe DiskMountManager for |Observer::OnMountEvent| event, which will be
  // raised upon the mount operation completion.
  virtual void MountPath(const std::string& source_path,
                         const std::string& source_format,
                         const std::string& mount_label,
                         const std::vector<std::string>& mount_options,
                         MountType type,
                         MountAccessMode access_mode) = 0;

  // Unmounts a mounted disk.
  // When the method is complete, |callback| will be called and observers'
  // |OnMountEvent| will be raised.
  //
  // |callback| may be empty, in which case it gets ignored.
  virtual void UnmountPath(const std::string& mount_path,
                           UnmountPathCallback callback) = 0;

  // Remounts mounted removable devices to change the read-only mount option.
  // Devices that can be mounted only in its read-only mode will be ignored.
  virtual void RemountAllRemovableDrives(chromeos::MountAccessMode mode) = 0;

  // Formats device mounted at |mount_path| with the given filesystem and label.
  // Also unmounts the device before formatting.
  // Example: mount_path: /media/VOLUME_LABEL
  //          filesystem: FormatFileSystemType::kNtfs
  //          label: MYUSB
  virtual void FormatMountedDevice(const std::string& mount_path,
                                   FormatFileSystemType filesystem,
                                   const std::string& label) = 0;

  // Renames Device given its mount path.
  // Example: mount_path: /media/VOLUME_LABEL
  //          volume_name: MYUSB
  virtual void RenameMountedDevice(const std::string& mount_path,
                                   const std::string& volume_name) = 0;

  // Unmounts device_path and all of its known children.
  virtual void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) = 0;

  // Used in tests to initialize the manager's disk and mount point sets.
  // Default implementation does noting. It just fails.
  virtual bool AddDiskForTest(std::unique_ptr<Disk> disk);
  virtual bool AddMountPointForTest(const MountPointInfo& mount_point);

  // Returns corresponding string to |type| like "unknown_filesystem".
  static std::string MountConditionToString(MountCondition type);

  // Returns corresponding string to |type|, like "sd", "usb".
  static std::string DeviceTypeToString(DeviceType type);

  // Creates the global DiskMountManager instance.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // DiskMountManager such as MockDiskMountManager for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(DiskMountManager* disk_mount_manager);

  // Destroys the global DiskMountManager instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global DiskMountManager instance.
  // Initialize() should already have been called.
  static DiskMountManager* GetInstance();
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_
