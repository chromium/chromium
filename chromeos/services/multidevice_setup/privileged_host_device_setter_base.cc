// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/privileged_host_device_setter_base.h"

namespace chromeos {

namespace multidevice_setup {

PrivilegedHostDeviceSetterBase::PrivilegedHostDeviceSetterBase() = default;

PrivilegedHostDeviceSetterBase::~PrivilegedHostDeviceSetterBase() = default;

void PrivilegedHostDeviceSetterBase::BindRequest(
    mojom::PrivilegedHostDeviceSetterRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace multidevice_setup

}  // namespace chromeos
