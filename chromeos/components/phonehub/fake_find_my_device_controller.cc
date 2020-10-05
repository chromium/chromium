// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_find_my_device_controller.h"

namespace chromeos {
namespace phonehub {

FakeFindMyDeviceController::FakeFindMyDeviceController() = default;

FakeFindMyDeviceController::~FakeFindMyDeviceController() = default;

void FakeFindMyDeviceController::SetPhoneRingingState(
    Status phone_ringing_status) {
  if (phone_ringing_status_ == phone_ringing_status)
    return;
  phone_ringing_status_ = phone_ringing_status;
  NotifyPhoneRingingStateChanged();
}

void FakeFindMyDeviceController::SetIsPhoneRingingInternal(
    bool is_phone_ringing) {
  Status phone_ringing_status =
      is_phone_ringing ? Status::kRingingOn : Status::kRingingOff;

  if (phone_ringing_status_ == Status::kRingingNotAvailable)
    return;

  SetPhoneRingingState(phone_ringing_status);
}

void FakeFindMyDeviceController::RequestNewPhoneRingingState(bool ringing) {
  if (!should_request_fail_)
    SetIsPhoneRingingInternal(ringing);
}

FindMyDeviceController::Status
FakeFindMyDeviceController::GetPhoneRingingStatus() {
  return phone_ringing_status_;
}

void FakeFindMyDeviceController::SetShouldRequestFail(
    bool should_request_fail) {
  should_request_fail_ = should_request_fail;
}

}  // namespace phonehub
}  // namespace chromeos
