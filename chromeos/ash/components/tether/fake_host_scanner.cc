// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_host_scanner.h"

namespace ash {

namespace tether {

FakeHostScanner::FakeHostScanner() = default;

FakeHostScanner::~FakeHostScanner() = default;

void FakeHostScanner::StopScan() {
  bool was_active = is_active_;
  is_active_ = false;

  if (was_active)
    NotifyScanFinished();
}

void FakeHostScanner::NotifyScanFinished() {
  HostScanner::NotifyScanFinished();
}

bool FakeHostScanner::IsScanActive() {
  return is_active_;
}

void FakeHostScanner::StartScan() {
  ++num_scans_started_;
  is_active_ = true;
}

}  // namespace tether

}  // namespace ash
