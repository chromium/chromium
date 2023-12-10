// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "chromecast/device/bluetooth/le/scan_filter.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockLeScanManager : public LeScanManager {
 public:
  class MockScanHandle : public ScanHandle {
   public:
    MockScanHandle() = default;
    ~MockScanHandle() override = default;
  };

  MockLeScanManager();
  ~MockLeScanManager() override;

  void AddObserver(Observer* o) override {
    DCHECK(o && !observer_);
    observer_ = o;
  }
  void RemoveObserver(Observer* o) override {
    DCHECK(o && o == observer_);
    observer_ = nullptr;
  }

  MOCK_METHOD(void,
              Initialize,
              (scoped_refptr<base::SingleThreadTaskRunner> io_task_runner),
              (override));
  MOCK_METHOD(void, Finalize, (), (override));
  MOCK_METHOD(std::unique_ptr<ScanHandle>, RequestScan, ());
  void RequestScan(RequestScanCallback cb) override {
    std::move(cb).Run(RequestScan());
  }

  MOCK_METHOD(std::vector<LeScanResult>,
              GetScanResults,
              (std::optional<ScanFilter> scan_filter));
  void GetScanResults(GetScanResultsCallback cb,
                      std::optional<ScanFilter> scan_filter) override {
    std::move(cb).Run(GetScanResults(std::move(scan_filter)));
  }
  MOCK_METHOD(void, ClearScanResults, (), (override));

  Observer* observer_ = nullptr;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_LE_SCAN_MANAGER_H_
