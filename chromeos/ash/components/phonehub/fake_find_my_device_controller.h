// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FIND_MY_DEVICE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FIND_MY_DEVICE_CONTROLLER_H_

#include "chromeos/ash/components/phonehub/find_my_device_controller.h"

namespace ash {
namespace phonehub {

class FakeFindMyDeviceController : public FindMyDeviceController {
 public:
  FakeFindMyDeviceController();
  ~FakeFindMyDeviceController() override;

  void SetPhoneRingingState(Status status);

  // FindMyDeviceController:
  void SetPhoneRingingStatusInternal(Status status) override;
  void RequestNewPhoneRingingState(bool ringing) override;
  Status GetPhoneRingingStatus() override;

  void SetShouldRequestFail(bool is_request_fail);

 private:
  Status phone_ringing_status_ = Status::kRingingOff;

  // Indicates if the connection to the phone is working correctly. If it is
  // true, there is a problem and the phone cannot change its state.
  bool should_request_fail_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_FIND_MY_DEVICE_CONTROLLER_H_
