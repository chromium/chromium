// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_

#include "chromeos/ash/components/phonehub/tether_controller.h"

namespace ash {
namespace phonehub {

class FakeTetherController : public TetherController {
 public:
  FakeTetherController();
  ~FakeTetherController() override;

  using TetherController::NotifyAttemptConnectionScanFailed;

  void SetStatus(Status status);

  size_t num_scan_for_available_connection_calls() {
    return num_scan_for_available_connection_calls_;
  }

  // TetherController:
  Status GetStatus() const override;
  void ScanForAvailableConnection() override;

 private:
  // TetherController:
  void AttemptConnection() override;
  void Disconnect() override;

  Status status_;
  size_t num_scan_for_available_connection_calls_ = 0;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_
