// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_

#include <utility>
#include <vector>

#include "chromeos/services/multidevice_setup/privileged_host_device_setter_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace multidevice_setup {

// Test PrivilegedHostDeviceSetter implementation.
class FakePrivilegedHostDeviceSetter : public PrivilegedHostDeviceSetterBase {
 public:
  FakePrivilegedHostDeviceSetter();
  ~FakePrivilegedHostDeviceSetter() override;

  std::vector<std::pair<std::string, SetHostDeviceCallback>>& set_host_args() {
    return set_host_args_;
  }

 private:
  // mojom::PrivilegedHostDeviceSetter:
  void SetHostDevice(const std::string& host_device_id,
                     SetHostDeviceCallback callback) override;

  std::vector<std::pair<std::string, SetHostDeviceCallback>> set_host_args_;

  mojo::BindingSet<mojom::PrivilegedHostDeviceSetter> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakePrivilegedHostDeviceSetter);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_
