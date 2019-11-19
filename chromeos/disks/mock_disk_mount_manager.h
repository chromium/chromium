// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace disks {

// TODO(tbarzic): Replace this mock with a fake implementation
// (http://crbug.com/355757)
class MockDiskMountManager : public DiskMountManager {
 public:
  MockDiskMountManager();
  virtual ~MockDiskMountManager();

  // DiskMountManager override.
  MOCK_METHOD0(Init, void(void));
  void AddObserver(DiskMountManager::Observer*) override;
  void RemoveObserver(DiskMountManager::Observer*) override;
  MOCK_CONST_METHOD0(disks, const DiskMountManager::DiskMap&(void));
  MOCK_CONST_METHOD1(FindDiskBySourcePath, const Disk*(const std::string&));
  MOCK_CONST_METHOD0(mount_points,
                     const DiskMountManager::MountPointMap&(void));
  MOCK_METHOD2(EnsureMountInfoRefreshed,
               void(EnsureMountInfoRefreshedCallback, bool));
  MOCK_METHOD6(MountPath,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::vector<std::string>&,
                    MountType,
                    MountAccessMode));
  MOCK_METHOD2(UnmountPath,
               void(const std::string&, DiskMountManager::UnmountPathCallback));
  MOCK_METHOD1(RemountAllRemovableDrives, void(MountAccessMode));
  MOCK_METHOD3(FormatMountedDevice,
               void(const std::string&,
                    FormatFileSystemType,
                    const std::string&));
  MOCK_METHOD2(RenameMountedDevice,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(UnmountDeviceRecursively,
               void(const std::string&,
                    DiskMountManager::UnmountDeviceRecursivelyCallbackType));

  // Invokes fake device insert events.
  void NotifyDeviceInsertEvents();

  // Invokes fake device remove events.
  void NotifyDeviceRemoveEvents();

  // Invokes specified mount event.
  void NotifyMountEvent(MountEvent event,
                        MountError error_code,
                        const MountPointInfo& mount_info);

  // Sets up default results for mock methods.
  void SetupDefaultReplies();

  // Creates a fake disk entry for the mounted device. This function is
  // primarily for StorageMonitorTest.
  void CreateDiskEntryForMountDevice(
      const DiskMountManager::MountPointInfo& mount_info,
      const std::string& device_id,
      const std::string& device_label,
      const std::string& vendor_name,
      const std::string& product_name,
      DeviceType device_type,
      uint64_t total_size_in_bytes,
      bool is_parent,
      bool has_media,
      bool on_boot_device,
      bool on_removable_device,
      const std::string& file_system_type);

  // Removes the fake disk entry associated with the mounted device. This
  // function is primarily for StorageMonitorTest.
  void RemoveDiskEntryForMountDevice(
      const DiskMountManager::MountPointInfo& mount_info);

 private:
  // Is used to implement AddObserver.
  void AddObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement RemoveObserver.
  void RemoveObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement disks.
  const DiskMountManager::DiskMap& disksInternal() const { return disks_; }

  const DiskMountManager::MountPointMap& mountPointsInternal() const;

  // Returns Disk object associated with the |source_path| or NULL on failure.
  const Disk* FindDiskBySourcePathInternal(
      const std::string& source_path) const;

  // Notifies observers about device status update.
  void NotifyDeviceChanged(DeviceEvent event,
                           const std::string& path);

  // Notifies observers about disk status update.
  void NotifyDiskChanged(DiskEvent event, const Disk* disk);

  // The list of observers.
  base::ObserverList<DiskMountManager::Observer> observers_;

  // The list of disks found.
  DiskMountManager::DiskMap disks_;

  // The list of existing mount points.
  DiskMountManager::MountPointMap mount_points_;

  DISALLOW_COPY_AND_ASSIGN(MockDiskMountManager);
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
