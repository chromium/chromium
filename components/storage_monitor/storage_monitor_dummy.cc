// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorDummy implementation.

#include "components/storage_monitor/storage_monitor.h"

namespace storage_monitor {

class StorageMonitorDummy : public StorageMonitor {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  StorageMonitorDummy() = default;

  StorageMonitorDummy(const StorageMonitorDummy&) = delete;
  StorageMonitorDummy& operator=(const StorageMonitorDummy&) = delete;

  ~StorageMonitorDummy() override = default;

  // Must be called for StorageMonitorDummy to work.
  void Init() override {}

 private:
  // StorageMonitor implementation:
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override {
    return false;
  }

  void EjectDevice(const std::string& device_id,
                   base::OnceCallback<void(EjectStatus)> callback) override {}
};

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorDummy();
}

}  // namespace storage_monitor
