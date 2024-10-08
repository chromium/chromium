// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DISKS_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DISKS_DISK_MOUNT_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string_view>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk.h"

namespace ash {
namespace disks {

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
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DISKS) DiskMountManager {
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
    FORMAT_COMPLETED,
  };

  enum PartitionEvent {
    PARTITION_STARTED,
    PARTITION_COMPLETED,
  };

  enum RenameEvent { RENAME_STARTED, RENAME_COMPLETED };

  // Comparator sorting Disk objects by device_path.
  struct SortByDevicePath {
    using is_transparent = void;

    template <typename A, typename B>
    bool operator()(const A& a, const B& b) const {
      return GetKey(a) < GetKey(b);
    }

    static std::string_view GetKey(std::string_view a) { return a; }

    static std::string_view GetKey(const std::unique_ptr<Disk>& disk) {
      DCHECK(disk);
      return disk->device_path();
    }
  };

  using Disks = std::set<std::unique_ptr<Disk>, SortByDevicePath>;

  using MountPoint = ::ash::MountPoint;

  // Comparator sorting MountPoint objects by mount_path.
  struct SortByMountPath {
    using is_transparent = void;

    template <typename A, typename B>
    bool operator()(const A& a, const B& b) const {
      return GetKey(a) < GetKey(b);
    }

    static std::string_view GetKey(std::string_view a) { return a; }

    static std::string_view GetKey(const MountPoint& mp) {
      return mp.mount_path;
    }
  };

  // MountPoints indexed by mount_path.
  typedef std::set<MountPoint, SortByMountPath> MountPoints;

  // A callback function type which is called after UnmountDeviceRecursively
  // finishes.
  typedef base::OnceCallback<void(MountError error_code)>
      UnmountDeviceRecursivelyCallbackType;

  typedef base::OnceCallback<void(MountError error_code,
                                  const MountPoint& mount_info)>
      MountPathCallback;

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
                              const MountPoint& mount_info) {}
    // Called on format process events.
    virtual void OnFormatEvent(FormatEvent event,
                               FormatError error_code,
                               const std::string& device_path,
                               const std::string& device_label) {}
    virtual void OnPartitionEvent(PartitionEvent event,
                                  PartitionError error_code,
                                  const std::string& device_path,
                                  const std::string& device_label) {}
    // Called on rename process events.
    virtual void OnRenameEvent(RenameEvent event,
                               RenameError error_code,
                               const std::string& device_path,
                               const std::string& device_label) {}

   protected:
    ~Observer() override;
  };

  // Delegate class for ARC-side operations.
  class ArcDelegate {
   public:
    typedef base::OnceCallback<void(bool success)> PreparationCallback;

    // Instruct ARC to prpeare for removable media unmount mounted on
    // `mount_path` by dropping any references to the volume.
    virtual void PrepareForRemovableMediaUnmount(
        const base::FilePath& mount_path,
        const base::TimeDelta& timeout,
        PreparationCallback callback) {}
  };

  virtual ~DiskMountManager() = default;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Registers a delegate.
  virtual void RegisterArcDelegate(ArcDelegate* delegate) {}

  // Unregisters the delegate.
  virtual void UnregisterArcDelegate() {}

  // Gets the list of disks found.
  virtual const Disks& disks() const = 0;

  // Returns Disk object corresponding to |source_path| or NULL on failure.
  virtual const Disk* FindDiskBySourcePath(
      const std::string& source_path) const = 0;

  // Gets the list of mount points.
  virtual const MountPoints& mount_points() const = 0;

  // Refreshes all the information about mounting if it is not yet done and
  // invokes |callback| when finished. If the information is already refreshed
  // and |force| is false, it just runs |callback| immediately.
  virtual void EnsureMountInfoRefreshed(
      EnsureMountInfoRefreshedCallback callback,
      bool force) = 0;

  // Mounts a device or an archive file.
  // |source_path| specifies either a device or an archive file path.
  // When |type|=MountType::kArchive, caller may set two optional
  // arguments: |source_format| and |mount_label|. See CrosDisksClient::Mount
  // for detail. |access_mode| specifies read-only or read-write mount mode for
  // a device. Note that the mount operation may fail. To find out the result,
  // one should observe DiskMountManager for |Observer::OnMountEvent| event,
  // which will be raised upon the mount operation completion.
  virtual void MountPath(const std::string& source_path,
                         const std::string& source_format,
                         const std::string& mount_label,
                         const std::vector<std::string>& mount_options,
                         MountType type,
                         MountAccessMode access_mode,
                         MountPathCallback callback) = 0;

  // Unmounts a mounted disk.
  // When the method is complete, |callback| will be called and observers'
  // |OnMountEvent| will be raised.
  //
  // |callback| may be empty, in which case it gets ignored.
  virtual void UnmountPath(const std::string& mount_path,
                           UnmountPathCallback callback) = 0;

  // Remounts mounted removable devices to change the read-only mount option.
  // Devices that can be mounted only in its read-only mode will be ignored.
  virtual void RemountAllRemovableDrives(MountAccessMode mode) = 0;

  // Formats device mounted at |mount_path| with the given filesystem and label.
  // Also unmounts the device before formatting.
  // Example: mount_path: /media/VOLUME_LABEL
  //          filesystem: FormatFileSystemType::kNtfs
  //          label: MYUSB
  virtual void FormatMountedDevice(const std::string& mount_path,
                                   FormatFileSystemType filesystem,
                                   const std::string& label) = 0;

  // Deletes partitions of the device, create a partition taking whole device
  // and format it as single volume. It converts devices with multiple child
  // volumes to a single volume disk. It unmounts the mounted child volumes
  // before erasing.
  // Example: device_path: /sys/devices/pci0000:00/0000:00:14.0/usb1/1-3/
  //                       1-3:1.0/host0/target0:0:0/0:0:0:0
  //          filesystem: FormatFileSystemType::kNtfs
  //          label: MYUSB
  virtual void SinglePartitionFormatDevice(const std::string& device_path,
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
  virtual bool AddMountPointForTest(const MountPoint& mount_point);

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
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DISKS_DISK_MOUNT_MANAGER_H_
