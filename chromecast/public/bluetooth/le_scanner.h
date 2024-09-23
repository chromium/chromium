// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_BLUETOOTH_LE_SCANNER_H_
#define CHROMECAST_PUBLIC_BLUETOOTH_LE_SCANNER_H_

#include <cstdint>
#include <vector>

#include "bluetooth_types.h"
#include "chromecast_export.h"

namespace chromecast {
namespace bluetooth_v2_shlib {

// Interface for BLE Scanner.
class CHROMECAST_EXPORT LeScanner {
 public:
  struct ScanResult {
    ScanResult(Addr addr, const std::vector<uint8_t>& adv_data, int rssi);
    ScanResult(const ScanResult& other);
    ~ScanResult();

    const Addr addr;
    const std::vector<uint8_t> adv_data;
    const int rssi;
  };

  class Delegate {
   public:
    virtual void OnScanResult(const ScanResult& scan_result) = 0;

    virtual ~Delegate() = default;
  };

  // Returns true if this interface is implemented.
  static bool IsSupported();
  static void SetDelegate(Delegate* delegate);

  static bool StartScan();
  static bool StopScan();

  static bool SetScanParameters(int scan_interval_ms, int scan_window_ms)
      __attribute__((__weak__));
};

inline LeScanner::ScanResult::ScanResult(Addr addr, const std::vector<uint8_t>& adv_data, int rssi) : addr(std::move(addr)), adv_data(adv_data), rssi(rssi) {} 
inline LeScanner::ScanResult::ScanResult(const LeScanner::ScanResult& other) =
    default;
inline LeScanner::ScanResult::~ScanResult() = default;

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  //  CHROMECAST_PUBLIC_BLUETOOTH_LE_SCANNER_H_
