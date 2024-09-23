// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/device/bluetooth/le/le_scan_result.h"
#include "chromecast/device/bluetooth/le/scan_filter.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace bluetooth_v2_shlib {
class LeScannerImpl;
}  // namespace bluetooth_v2_shlib

namespace bluetooth {
class BluetoothManagerPlatform;

class LeScanManager {
 public:
  class Observer {
   public:
    // Called when the scan has been enabled or disabled.
    virtual void OnScanEnableChanged(bool enabled) {}

    // Called when a new scan result is ready.
    virtual void OnNewScanResult(LeScanResult result) {}

    virtual ~Observer() = default;
  };

  virtual void AddObserver(Observer* o) = 0;
  virtual void RemoveObserver(Observer* o) = 0;

  class ScanHandle {
   public:
    ScanHandle(const ScanHandle&) = delete;
    ScanHandle& operator=(const ScanHandle&) = delete;

    virtual ~ScanHandle() = default;

   protected:
    ScanHandle() = default;
  };

  static std::unique_ptr<LeScanManager> Create(
      BluetoothManagerPlatform* bluetooth_manager,
      bluetooth_v2_shlib::LeScannerImpl* le_scanner);

  LeScanManager(const LeScanManager&) = delete;
  LeScanManager& operator=(const LeScanManager&) = delete;

  virtual ~LeScanManager() = default;

  virtual void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) = 0;
  virtual void Finalize() = 0;

  // Request a handle to enable BLE scanning. Can be called on any thread. |cb|
  // returns a handle. As long is there is at least one handle in existence, BLE
  // scanning will be enabled. Returns nullptr if failed to enable scanning.
  using RequestScanCallback =
      base::OnceCallback<void(std::unique_ptr<ScanHandle> handle)>;
  virtual void RequestScan(RequestScanCallback cb) = 0;

  // Asynchronously get the most recent scan results. Can be called on any
  // thread. |cb| is called on the calling thread with the results. If
  // |scan_filter| is passed, only scan results matching the given |scan_filter|
  // will be returned.
  using GetScanResultsCallback =
      base::OnceCallback<void(std::vector<LeScanResult>)>;
  virtual void GetScanResults(
      GetScanResultsCallback cb,
      std::optional<ScanFilter> scan_filter = std::nullopt) = 0;

  virtual void ClearScanResults() = 0;

  virtual void PauseScan() {}

  virtual void ResumeScan() {}

  virtual void SetScanParameters(int scan_interval_ms, int scan_window_ms) {}

 protected:
  LeScanManager() = default;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_H_
