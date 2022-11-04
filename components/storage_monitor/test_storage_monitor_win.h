// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of StorageMonitorWin to simulate device
// changed events for testing.

#ifndef COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_
#define COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_

#include <windows.h>

#include <memory>

#include "components/storage_monitor/storage_monitor_win.h"

namespace storage_monitor {

class TestPortableDeviceWatcherWin;
class TestVolumeMountWatcherWin;

class TestStorageMonitorWin: public StorageMonitorWin {
 public:
  TestStorageMonitorWin(
      std::unique_ptr<TestVolumeMountWatcherWin> volume_mount_watcher,
      std::unique_ptr<TestPortableDeviceWatcherWin> portable_device_watcher);

  TestStorageMonitorWin(const TestStorageMonitorWin&) = delete;
  TestStorageMonitorWin& operator=(const TestStorageMonitorWin&) = delete;

  ~TestStorageMonitorWin() override;

  void InjectDeviceChange(UINT event_type, LPARAM data);

  VolumeMountWatcherWin* volume_mount_watcher();

  Receiver* receiver() const override;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_
