// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_

#include "chromeos/components/phonehub/tether_controller.h"

namespace chromeos {
namespace phonehub {

class FakeTetherController : public TetherController {
 public:
  FakeTetherController();
  ~FakeTetherController() override;

  void SetStatus(Status status);

  // TetherController:
  Status GetStatus() const override;

 private:
  void ScanForAvailableConnection() override;
  void AttemptConnection() override;
  void Disconnect() override;

  Status status_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_TETHER_CONTROLLER_H_
