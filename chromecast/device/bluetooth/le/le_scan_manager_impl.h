// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_IMPL_H_

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "chromecast/device/bluetooth/le/scan_filter.h"
#include "chromecast/device/bluetooth/shlib/le_scanner.h"

namespace chromecast {
namespace bluetooth {

class LeScanManagerImpl : public LeScanManager,
                          public bluetooth_v2_shlib::LeScanner::Delegate {
 public:
  explicit LeScanManagerImpl(bluetooth_v2_shlib::LeScannerImpl* le_scanner);
  ~LeScanManagerImpl() override;

  static constexpr int kMaxScanResultEntries = 1024;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  void Finalize();

  // LeScanManager implementation:
  void AddObserver(Observer* o) override;
  void RemoveObserver(Observer* o) override;
  void RequestScan(RequestScanCallback cb) override;
  void GetScanResults(
      GetScanResultsCallback cb,
      base::Optional<ScanFilter> service_uuid = base::nullopt) override;
  void ClearScanResults() override;
  void PauseScan() override;
  void RestartScan() override;
  void SetScanParameters(int scan_interval_ms, int scan_window_ms) override;

 private:
  class ScanHandleImpl;

  // bluetooth_v2_shlib::LeScanner::Delegate implementation:
  void OnScanResult(const bluetooth_v2_shlib::LeScanner::ScanResult&
                        scan_result_shlib) override;

  // Returns a list of all BLE scan results. The results are sorted by RSSI.
  // Must be called on |io_task_runner|.
  std::vector<LeScanResult> GetScanResultsInternal(
      base::Optional<ScanFilter> service_uuid);

  void NotifyScanHandleDestroyed(int32_t id);

  bluetooth_v2_shlib::LeScannerImpl* const le_scanner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;
  std::map<bluetooth_v2_shlib::Addr, std::list<LeScanResult>>
      addr_to_scan_results_;

  // List of addresses in scan results. Addresses are sorted from most recently
  // used to least recently used.
  std::deque<bluetooth_v2_shlib::Addr> scan_result_addr_list_;

  int32_t next_scan_handle_id_ = 0;
  std::set<int32_t> scan_handle_ids_;

  base::WeakPtrFactory<LeScanManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LeScanManagerImpl);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_MANAGER_IMPL_H_
