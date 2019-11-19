// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_SHLIB_MOCK_LE_SCANNER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_SHLIB_MOCK_LE_SCANNER_H_

#include "chromecast/device/bluetooth/shlib/le_scanner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth_v2_shlib {

class MockLeScanner : public LeScannerImpl {
 public:
  MockLeScanner();
  ~MockLeScanner() override;
  MOCK_METHOD0(IsSupported, bool());
  MOCK_METHOD1(SetDelegate, void(LeScanner::Delegate* delegate));
  MOCK_METHOD0(StartScan, bool());
  MOCK_METHOD0(StopScan, bool());
  MOCK_METHOD2(SetScanParameters, bool(int, int));
};

inline MockLeScanner::MockLeScanner() = default;
inline MockLeScanner::~MockLeScanner() = default;

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_SHLIB_MOCK_LE_SCANNER_H_
