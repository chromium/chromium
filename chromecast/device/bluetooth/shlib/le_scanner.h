// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_SHLIB_LE_SCANNER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_SHLIB_LE_SCANNER_H_

#include "chromecast/public/bluetooth/le_scanner.h"

namespace chromecast {
namespace bluetooth_v2_shlib {

class LeScannerImpl {
 public:
  virtual ~LeScannerImpl() = default;
  virtual bool IsSupported() = 0;
  virtual void SetDelegate(LeScanner::Delegate* delegate) = 0;
  virtual bool StartScan() = 0;
  virtual bool StopScan() = 0;
  virtual bool SetScanParameters(int scan_interval_ms, int scan_window_ms) = 0;
};

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_SHLIB_LE_SCANNER_H_
