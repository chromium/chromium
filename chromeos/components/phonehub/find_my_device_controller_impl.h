// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_

#include "chromeos/components/phonehub/find_my_device_controller.h"

namespace chromeos {
namespace phonehub {

// TODO(https://crbug.com/1106937): Add real implementation.
class FindMyDeviceControllerImpl : public FindMyDeviceController {
 public:
  FindMyDeviceControllerImpl();
  ~FindMyDeviceControllerImpl() override;

 private:
  // FindMyDeviceController:
  bool IsPhoneRinging() const override;
  void SetIsPhoneRingingInternal(bool is_phone_ringing) override;
  void RequestNewPhoneRingingState(bool ringing) override;

  bool is_phone_ringing_ = false;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_
