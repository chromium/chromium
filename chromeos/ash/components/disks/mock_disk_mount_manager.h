// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_

#include <stdint.h>

#include <string>

#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace disks {

// TODO(tbarzic): Replace this mock with a fake implementation
// (http://crbug.com/355757)
class MockDiskMountManager : public DiskMountManager {
 public:
  MockDiskMountManager();

  MockDiskMountManager(const MockDiskMountManager&) = delete;
  MockDiskMountManager& operator=(const MockDiskMountManager&) = delete;

  ~MockDiskMountManager() override;

  // DiskMountManager override.
  void AddObserver(DiskMountManager::Observer*) override;
  void RemoveObserver(DiskMountManager::Observer*) override;
  MOCK_METHOD(const DiskMountManager::Disks&, disks, (), (const, override));
  MOCK_METHOD(const Disk*,
              FindDiskBySourcePath,
              (const std::string&),
              (const, override));
  MOCK_METHOD(const DiskMountManager::MountPoints&,
              mount_points,
              (),
              (const, override));
  MOCK_METHOD(void,
              EnsureMountInfoRefreshed,
              (EnsureMountInfoRefreshedCallback, bool),
              (override));
  MOCK_METHOD(void,
              MountPath,
              (const std::string&,
               const std::string&,
               const std::string&,
               const std::vector<std::string>&,
               MountType,
               MountAccessMode,
               DiskMountManager::MountPathCallback),
              (override));
  MOCK_METHOD(void,
              UnmountPath,
              (const std::string&, DiskMountManager::UnmountPathCallback),
              (override));
  MOCK_METHOD(void, RemountAllRemovableDrives, (MountAccessMode), (override));
  MOCK_METHOD(void,
              FormatMountedDevice,
              (const std::string&, FormatFileSystemType, const std::string&),
              (override));
  MOCK_METHOD(void,
              SinglePartitionFormatDevice,
              (const std::string&, FormatFileSystemType, const std::string&),
              (override));
  MOCK_METHOD(void,
              RenameMountedDevice,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              UnmountDeviceRecursively,
              (const std::string&,
               DiskMountManager::UnmountDeviceRecursivelyCallbackType),
              (override));

  // Invokes specified mount event.
  void NotifyMountEvent(MountEvent event,
                        MountError error_code,
                        const MountPoint& mount_info);

  // Sets up default results for mock methods.
  void SetupDefaultReplies();

  // Creates a fake disk entry for the mounted device.
  void CreateDiskEntryForMountDevice(std::unique_ptr<Disk> disk);

  // Creates a fake disk entry for the mounted device.
  void CreateDiskEntryForMountDevice(
      const DiskMountManager::MountPoint& mount_info,
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
      const DiskMountManager::MountPoint& mount_info);

 private:
  // Is used to implement disks.
  const DiskMountManager::Disks& disksInternal() const { return disks_; }

  const DiskMountManager::MountPoints& mountPointsInternal() const;

  // Returns Disk object associated with the |source_path| or NULL on failure.
  const Disk* FindDiskBySourcePathInternal(
      const std::string& source_path) const;

  // The list of observers.
  base::ObserverList<DiskMountManager::Observer> observers_;

  // The list of disks found.
  DiskMountManager::Disks disks_;

  // The list of existing mount points.
  DiskMountManager::MountPoints mount_points_;
};

}  // namespace disks
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
