// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of VolumeMountWatcherWin to expose some
// functionality for testing.

#ifndef COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
#define COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "components/storage_monitor/volume_mount_watcher_win.h"

namespace base {
class FilePath;
}

namespace storage_monitor {

class TestVolumeMountWatcherWin : public VolumeMountWatcherWin {
 public:
  TestVolumeMountWatcherWin();

  TestVolumeMountWatcherWin(const TestVolumeMountWatcherWin&) = delete;
  TestVolumeMountWatcherWin& operator=(const TestVolumeMountWatcherWin&) =
      delete;

  ~TestVolumeMountWatcherWin() override;

  static bool GetDeviceRemovable(const base::FilePath& device_path,
                                 bool* removable);

  void AddDeviceForTesting(const base::FilePath& device_path,
                           const std::string& device_id,
                           const std::u16string& device_name,
                           uint64_t total_size_in_bytes);

  void SetAttachedDevicesFake();

  const std::vector<base::FilePath>& devices_checked() const {
      return devices_checked_;
  }

  void BlockDeviceCheckForTesting();

  void ReleaseDeviceCheck();

  // VolumeMountWatcherWin:
  void DeviceCheckComplete(const base::FilePath& device_path) override;
  GetAttachedDevicesCallbackType GetAttachedDevicesCallback() const override;
  GetDeviceDetailsCallbackType GetDeviceDetailsCallback() const override;

 private:
  std::vector<base::FilePath> devices_checked_;
  std::unique_ptr<base::WaitableEvent> device_check_complete_event_;
  bool attached_devices_fake_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
