// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/find_my_device_controller_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace phonehub {

FindMyDeviceControllerImpl::FindMyDeviceControllerImpl() = default;

FindMyDeviceControllerImpl::~FindMyDeviceControllerImpl() = default;

bool FindMyDeviceControllerImpl::IsPhoneRinging() const {
  return is_phone_ringing_;
}

void FindMyDeviceControllerImpl::SetIsPhoneRingingInternal(
    bool is_phone_ringing) {
  is_phone_ringing_ = is_phone_ringing;
}

void FindMyDeviceControllerImpl::RequestNewPhoneRingingState(bool ringing) {
  PA_LOG(INFO) << "Attempting to set Find My Device phone ring state; new "
               << "value: " << ringing;
}

}  // namespace phonehub
}  // namespace chromeos
