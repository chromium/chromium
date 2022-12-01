// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_DO_NOT_DISTURB_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_DO_NOT_DISTURB_CONTROLLER_H_

#include "chromeos/ash/components/phonehub/do_not_disturb_controller.h"

namespace ash {
namespace phonehub {

class FakeDoNotDisturbController : public DoNotDisturbController {
 public:
  FakeDoNotDisturbController();
  ~FakeDoNotDisturbController() override;

  // DoNotDisturbController:
  bool IsDndEnabled() const override;
  void SetDoNotDisturbStateInternal(bool is_dnd_enabled,
                                    bool can_request_new_dnd_state) override;
  void RequestNewDoNotDisturbState(bool enabled) override;
  bool CanRequestNewDndState() const override;

  void SetShouldRequestFail(bool should_request_fail);

 private:
  bool is_dnd_enabled_ = false;
  bool can_request_new_dnd_state_ = false;

  // Indicates if the connection to the phone is working correctly. If it is
  // true, there is a problem and the phone cannot change its state.
  bool should_request_fail_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_DO_NOT_DISTURB_CONTROLLER_H_
