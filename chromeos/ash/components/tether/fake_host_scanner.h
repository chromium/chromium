// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_

#include "chromeos/ash/components/tether/host_scanner.h"

namespace ash {

namespace tether {

// Test double for HostScanner.
class FakeHostScanner : public HostScanner {
 public:
  FakeHostScanner();

  FakeHostScanner(const FakeHostScanner&) = delete;
  FakeHostScanner& operator=(const FakeHostScanner&) = delete;

  ~FakeHostScanner() override;

  size_t num_scans_started() { return num_scans_started_; }

  void NotifyScanFinished();

  // HostScanner:
  bool IsScanActive() override;
  void StartScan() override;
  void StopScan() override;

 private:
  size_t num_scans_started_ = 0u;
  bool is_active_ = false;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCANNER_H_
