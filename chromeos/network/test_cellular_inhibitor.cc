// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/test_cellular_inhibitor.h"

#include "chromeos/network/device_state.h"

namespace chromeos {

TestCellularInhibitor::TestCellularInhibitor() = default;

TestCellularInhibitor::~TestCellularInhibitor() = default;

bool TestCellularInhibitor::HasScanningStarted() {
  // Stub response for testing.
  return true;
}

bool TestCellularInhibitor::HasScanningStopped() {
  // Stub response for testing.
  return true;
}

}  // namespace chromeos
