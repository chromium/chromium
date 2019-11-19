// Copyright 2019 The Chromium Authors. All rights reserved.
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
                   base::Callback<void(EjectStatus)> callback) override {}

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorDummy);
};

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorDummy();
}

}  // namespace storage_monitor
