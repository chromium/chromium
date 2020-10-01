// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_find_my_device_controller.h"

namespace chromeos {
namespace phonehub {

FakeFindMyDeviceController::FakeFindMyDeviceController() = default;

FakeFindMyDeviceController::~FakeFindMyDeviceController() = default;

bool FakeFindMyDeviceController::IsPhoneRinging() const {
  return is_phone_ringing_;
}

void FakeFindMyDeviceController::SetIsPhoneRingingInternal(
    bool is_phone_ringing) {
  if (is_phone_ringing_ == is_phone_ringing)
    return;

  is_phone_ringing_ = is_phone_ringing;
  NotifyPhoneRingingStateChanged();
}

void FakeFindMyDeviceController::RequestNewPhoneRingingState(bool ringing) {
  SetIsPhoneRingingInternal(ringing);
}

}  // namespace phonehub
}  // namespace chromeos
