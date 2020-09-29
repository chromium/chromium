// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_

#include "chromeos/components/phonehub/do_not_disturb_controller.h"

namespace chromeos {
namespace phonehub {

// TODO(https://crbug.com/1106937): Add real implementation.
class DoNotDisturbControllerImpl : public DoNotDisturbController {
 public:
  DoNotDisturbControllerImpl();
  ~DoNotDisturbControllerImpl() override;

 private:
  // DoNotDisturbController:
  bool IsDndEnabled() const override;
  void SetDoNotDisturbStateInternal(bool is_dnd_enabled) override;
  void RequestNewDoNotDisturbState(bool enabled) override;

  bool is_dnd_enabled_ = false;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
