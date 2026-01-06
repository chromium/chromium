// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestStorageMonitorWin implementation.

#include "components/storage_monitor/test_storage_monitor_win.h"

#include <utility>

#include "components/storage_monitor/test_volume_mount_watcher_win.h"

namespace storage_monitor {

TestStorageMonitorWin::TestStorageMonitorWin(
    std::unique_ptr<TestVolumeMountWatcherWin> volume_mount_watcher)
    : StorageMonitorWin(std::move(volume_mount_watcher)) {
  DCHECK(volume_mount_watcher_);
}

TestStorageMonitorWin::~TestStorageMonitorWin() = default;

void TestStorageMonitorWin::InjectDeviceChange(UINT event_type, LPARAM data) {
  OnDeviceChange(event_type, data);
}

VolumeMountWatcherWin*
TestStorageMonitorWin::volume_mount_watcher() {
  return volume_mount_watcher_.get();
}

StorageMonitor::Receiver* TestStorageMonitorWin::receiver() const {
  return StorageMonitor::receiver();
}

}  // namespace storage_monitor
