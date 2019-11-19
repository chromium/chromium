// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_

#include "base/macros.h"
#include "components/arc/mojom/power.mojom.h"

namespace arc {

class FakePowerInstance : public mojom::PowerInstance {
 public:
  FakePowerInstance();
  ~FakePowerInstance() override;

  bool interactive() const { return interactive_; }
  int num_suspend() const { return num_suspend_; }
  int num_resume() const { return num_resume_; }
  double screen_brightness() const { return screen_brightness_; }
  int num_power_supply_info() const { return num_power_supply_info_; }

  // Returns |suspend_callback_| and resets the member.
  SuspendCallback GetSuspendCallback();

  // mojom::PowerInstance overrides:
  void InitDeprecated(mojom::PowerHostPtr host_ptr) override;
  void Init(mojom::PowerHostPtr host_ptr, InitCallback callback) override;
  void SetInteractive(bool enabled) override;
  void Suspend(SuspendCallback callback) override;
  void Resume() override;
  void UpdateScreenBrightnessSettings(double percent) override;
  void PowerSupplyInfoChanged() override;

 private:
  mojom::PowerHostPtr host_ptr_;

  // Last state passed to SetInteractive().
  bool interactive_ = true;

  // Number of calls to Suspend() and Resume().
  int num_suspend_ = 0;
  int num_resume_ = 0;

  // Last callback passed to Suspend().
  SuspendCallback suspend_callback_;

  // Last value passed to UpdateScreenBrightnessSettings().
  double screen_brightness_ = 0.0;

  // Number of calls to PowerSupplyInfoChanged().
  int num_power_supply_info_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakePowerInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_
