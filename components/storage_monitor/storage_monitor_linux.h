// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux processes mount point change events, notifies listeners
// about the addition and deletion of media devices, and answers queries about
// mounted devices. StorageMonitorLinux uses a MtabWatcherLinux on a separate
// background sequence that is file IO capable.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#error "Use the ChromeOS-specific implementation instead."
#endif

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/storage_monitor/mtab_watcher_linux.h"
#include "components/storage_monitor/storage_monitor.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage_monitor {

class StorageMonitorLinux : public StorageMonitor {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  // |mtab_file_path| is the path to a mtab file to watch for mount points.
  explicit StorageMonitorLinux(const base::FilePath& mtab_file_path);

  StorageMonitorLinux(const StorageMonitorLinux&) = delete;
  StorageMonitorLinux& operator=(const StorageMonitorLinux&) = delete;

  ~StorageMonitorLinux() override;

  // Must be called for StorageMonitorLinux to work.
  void Init() override;

 protected:
  // Gets device information given a |device_path| and |mount_point|.
  using GetDeviceInfoCallback =
      base::RepeatingCallback<std::unique_ptr<StorageInfo>(
          const base::FilePath& device_path,
          const base::FilePath& mount_point)>;

  void SetGetDeviceInfoCallbackForTest(
      const GetDeviceInfoCallback& get_device_info_callback);

  // Parses |new_mtab| and find all changes.
  virtual void UpdateMtab(
      const MtabWatcherLinux::MountPointDeviceMap& new_mtab);

 private:
  // Structure to save mounted device information such as device path, unique
  // identifier, device name and partition size.
  struct MountPointInfo {
    base::FilePath mount_device;
    StorageInfo storage_info;
  };

  // Mapping of mount points to MountPointInfo.
  using MountMap = std::map<base::FilePath, MountPointInfo>;

  // (mount point, priority)
  // For devices that are mounted to multiple mount points, this helps us track
  // which one we've notified system monitor about.
  using ReferencedMountPoint = std::map<base::FilePath, bool>;

  // (mount device, map of known mount points)
  // For each mount device, track the places it is mounted and which one (if
  // any) we have notified system monitor about.
  using MountPriorityMap = std::map<base::FilePath, ReferencedMountPoint>;

  // StorageMonitor implementation.
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;
  void EjectDevice(const std::string& device_id,
                   base::OnceCallback<void(EjectStatus)> callback) override;

  // Called when the MtabWatcher has been created.
  void OnMtabWatcherCreated(std::unique_ptr<MtabWatcherLinux> watcher);

  bool IsDeviceAlreadyMounted(const base::FilePath& mount_device) const;

  // Assuming |mount_device| is already mounted, and it gets mounted again at
  // |mount_point|, update the mappings.
  void HandleDeviceMountedMultipleTimes(const base::FilePath& mount_device,
                                        const base::FilePath& mount_point);

  // Adds |mount_device| to the mappings and notify listeners, if any.
  void AddNewMount(const base::FilePath& mount_device,
                   std::unique_ptr<StorageInfo> storage_info);

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Callback to get device information. Set this to a custom callback for
  // testing.
  GetDeviceInfoCallback get_device_info_callback_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mount_info_map_;

  // Because a device can be mounted to multiple places, we only want to
  // notify about one of them. If (and only if) that one is unmounted, we need
  // to notify about it's departure and notify about another one of it's mount
  // points.
  MountPriorityMap mount_priority_map_;

  // Must be created and destroyed on |mtab_watcher_task_runner_|.
  std::unique_ptr<MtabWatcherLinux> mtab_watcher_;

  scoped_refptr<base::SequencedTaskRunner> mtab_watcher_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StorageMonitorLinux> weak_ptr_factory_{this};
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
