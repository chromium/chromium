// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/fake_privileged_host_device_setter.h"

namespace chromeos {

namespace multidevice_setup {

FakePrivilegedHostDeviceSetter::FakePrivilegedHostDeviceSetter() = default;

FakePrivilegedHostDeviceSetter::~FakePrivilegedHostDeviceSetter() = default;

void FakePrivilegedHostDeviceSetter::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    SetHostDeviceCallback callback) {
  set_host_args_.emplace_back(host_instance_id_or_legacy_device_id,
                              std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace chromeos
