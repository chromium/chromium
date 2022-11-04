// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_

#include <windows.h>

#include <memory>

#include "components/storage_monitor/storage_monitor.h"

namespace base {
class FilePath;
}

namespace storage_monitor {

class PortableDeviceWatcherWin;
class TestStorageMonitorWin;
class VolumeMountWatcherWin;

class StorageMonitorWin : public StorageMonitor {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  // To support unit tests, this constructor takes |volume_mount_watcher| and
  // |portable_device_watcher| objects. These params are either constructed in
  // unit tests or in StorageMonitorWin CreateInternal() function.
  StorageMonitorWin(
      std::unique_ptr<VolumeMountWatcherWin> volume_mount_watcher,
      std::unique_ptr<PortableDeviceWatcherWin> portable_device_watcher);

  StorageMonitorWin(const StorageMonitorWin&) = delete;
  StorageMonitorWin& operator=(const StorageMonitorWin&) = delete;

  ~StorageMonitorWin() override;

  // Must be called after the file thread is created.
  void Init() override;

  // StorageMonitor:
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;
  bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      std::wstring* device_location,
      std::wstring* storage_object_id) const override;
  void EjectDevice(const std::string& device_id,
                   base::OnceCallback<void(EjectStatus)> callback) override;

 private:
  class PortableDeviceNotifications;
  friend class TestStorageMonitorWin;

  void MediaChangeNotificationRegister();

  // Gets the removable storage information given a |device_path|. On success,
  // returns true and fills in |info|.
  bool GetDeviceInfo(const base::FilePath& device_path,
                     StorageInfo* info) const;

  static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam);

  LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                           LPARAM lparam);

  void OnDeviceChange(UINT event_type, LPARAM data);
  void OnMediaChange(WPARAM wparam, LPARAM lparam);

  // The window class of |window_|.
  ATOM window_class_ = 0;

  // The handle of the module that contains the window procedure of |window_|.
  HMODULE instance_ = nullptr;
  HWND window_ = nullptr;

  // The handle of a registration for shell notifications.
  ULONG shell_change_notify_id_ = 0;

  // The volume mount point watcher, used to manage the mounted devices.
  const std::unique_ptr<VolumeMountWatcherWin> volume_mount_watcher_;

  // The portable device watcher, used to manage media transfer protocol
  // devices.
  const std::unique_ptr<PortableDeviceWatcherWin> portable_device_watcher_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_
