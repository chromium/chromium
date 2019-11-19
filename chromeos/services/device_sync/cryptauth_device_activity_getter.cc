// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_activity_getter.h"

#include <utility>

namespace chromeos {

namespace device_sync {

CryptAuthDeviceActivityGetter::CryptAuthDeviceActivityGetter() = default;

CryptAuthDeviceActivityGetter::~CryptAuthDeviceActivityGetter() = default;

void CryptAuthDeviceActivityGetter::GetDevicesActivityStatus(
    GetDeviceActivityStatusAttemptFinishedCallback success_callback,
    GetDeviceActivityStatusAttemptErrorCallback error_callback) {
  // Enforce that GetDevicesActivityStatus() can only be called once.
  DCHECK(!was_get_device_activity_getter_called_);
  was_get_device_activity_getter_called_ = true;

  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  OnAttemptStarted();
}

void CryptAuthDeviceActivityGetter::FinishAttemptSuccessfully(
    DeviceActivityStatusResult device_activity_status) {
  DCHECK(success_callback_);
  std::move(success_callback_).Run(std::move(device_activity_status));
}

void CryptAuthDeviceActivityGetter::FinishAttemptWithError(
    NetworkRequestError network_request_error) {
  DCHECK(error_callback_);
  std::move(error_callback_).Run(network_request_error);
}

}  // namespace device_sync

}  // namespace chromeos
