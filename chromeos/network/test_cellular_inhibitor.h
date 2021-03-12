// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_TEST_CELLULAR_INHIBITOR_H_
#define CHROMEOS_NETWORK_TEST_CELLULAR_INHIBITOR_H_

#include "chromeos/network/cellular_inhibitor.h"

namespace chromeos {

// A Test implementation of CellularInhibitor that considers and inhibit
// operation complete once the cellular device's Inhibit property is false.
class TestCellularInhibitor : public CellularInhibitor {
 public:
  TestCellularInhibitor();
  ~TestCellularInhibitor() override;

 private:
  // CellularESimProfileHandler:
  bool HasScanningStarted() override;
  bool HasScanningStopped() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_TEST_CELLULAR_INHIBITOR_H_
