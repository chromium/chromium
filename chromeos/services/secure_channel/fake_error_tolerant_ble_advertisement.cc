// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_error_tolerant_ble_advertisement.h"

#include "base/bind.h"

namespace chromeos {

namespace secure_channel {

FakeErrorTolerantBleAdvertisement::FakeErrorTolerantBleAdvertisement(
    const DeviceIdPair& device_id_pair,
    base::OnceCallback<void(const DeviceIdPair&)> destructor_callback)
    : ErrorTolerantBleAdvertisement(device_id_pair),
      id_(base::UnguessableToken::Create()),
      destructor_callback_(std::move(destructor_callback)) {}

FakeErrorTolerantBleAdvertisement::~FakeErrorTolerantBleAdvertisement() {
  std::move(destructor_callback_).Run(device_id_pair());
}

bool FakeErrorTolerantBleAdvertisement::HasBeenStopped() {
  return stopped_;
}

void FakeErrorTolerantBleAdvertisement::InvokeStopCallback() {
  DCHECK(HasBeenStopped());
  std::move(stop_callback_).Run();
}

void FakeErrorTolerantBleAdvertisement::Stop(base::OnceClosure callback) {
  DCHECK(!HasBeenStopped());
  stopped_ = true;
  stop_callback_ = std::move(callback);
}

}  // namespace secure_channel

}  // namespace chromeos
