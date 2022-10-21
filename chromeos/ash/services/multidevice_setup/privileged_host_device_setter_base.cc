// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/privileged_host_device_setter_base.h"

namespace ash {

namespace multidevice_setup {

PrivilegedHostDeviceSetterBase::PrivilegedHostDeviceSetterBase() = default;

PrivilegedHostDeviceSetterBase::~PrivilegedHostDeviceSetterBase() = default;

void PrivilegedHostDeviceSetterBase::BindReceiver(
    mojo::PendingReceiver<mojom::PrivilegedHostDeviceSetter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace multidevice_setup

}  // namespace ash
