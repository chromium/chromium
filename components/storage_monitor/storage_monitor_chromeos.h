// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorCros listens for mount point changes and notifies listeners
// about the addition and deletion of media devices. This class lives on the
// UI thread.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_

#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error "Should only be used on ChromeOS."
#endif

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/storage_monitor/storage_monitor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"

namespace storage_monitor {

class MtpManagerClientChromeOS;

class StorageMonitorCros : public StorageMonitor,
                           public ash::disks::DiskMountManager::Observer {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  StorageMonitorCros();

  StorageMonitorCros(const StorageMonitorCros&) = delete;
  StorageMonitorCros& operator=(const StorageMonitorCros&) = delete;

  ~StorageMonitorCros() override;

  // Sets up disk listeners and issues notifications for any discovered
  // mount points. Sets up MTP manager and listeners.
  void Init() override;

 protected:
  void SetMediaTransferProtocolManagerForTest(
      mojo::PendingRemote<device::mojom::MtpManager> test_manager);

  // ash::disks::DiskMountManager::Observer implementation.
  void OnBootDeviceDiskEvent(ash::disks::DiskMountManager::DiskEvent event,
                             const ash::disks::Disk& disk) override;
  void OnMountEvent(
      ash::disks::DiskMountManager::MountEvent event,
      ash::MountError error_code,
      const ash::disks::DiskMountManager::MountPoint& mount_info) override;

  // StorageMonitor implementation.
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;
  void EjectDevice(const std::string& device_id,
                   base::OnceCallback<void(EjectStatus)> callback) override;
  device::mojom::MtpManager* media_transfer_protocol_manager() override;

 private:
  // Mapping of mount path to removable mass storage info.
  typedef std::map<std::string, StorageInfo> MountMap;

  // Helper method that checks existing mount points to see if they are media
  // devices. Eventually calls AddMountedPath for all mount points.
  void CheckExistingMountPoints();

  // Adds the mount point in |mount_info| to |mount_map_| and send a media
  // device attach notification. |has_dcim| is true if the attached device has
  // a DCIM folder.
  void AddMountedPath(
      const ash::disks::DiskMountManager::MountPoint& mount_info,
      bool has_dcim);

  // Adds the mount point in |disk| to |mount_map_| and send a device
  // attach notification.
  void AddFixedStorageDisk(const ash::disks::Disk& disk);

  // Removes the mount point in |disk| from |mount_map_| and send a device
  // detach notification.
  void RemoveFixedStorageDisk(const ash::disks::Disk& disk);

  // Mapping of relevant mount points and their corresponding mount devices.
  MountMap mount_map_;

  mojo::Remote<device::mojom::MtpManager> mtp_device_manager_;

  std::unique_ptr<MtpManagerClientChromeOS> mtp_manager_client_;

  base::WeakPtrFactory<StorageMonitorCros> weak_ptr_factory_{this};
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
