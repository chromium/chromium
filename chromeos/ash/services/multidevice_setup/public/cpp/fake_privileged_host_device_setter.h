// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_

#include <utility>
#include <vector>

#include "chromeos/ash/services/multidevice_setup/privileged_host_device_setter_base.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

// Test PrivilegedHostDeviceSetter implementation.
class FakePrivilegedHostDeviceSetter : public PrivilegedHostDeviceSetterBase {
 public:
  FakePrivilegedHostDeviceSetter();

  FakePrivilegedHostDeviceSetter(const FakePrivilegedHostDeviceSetter&) =
      delete;
  FakePrivilegedHostDeviceSetter& operator=(
      const FakePrivilegedHostDeviceSetter&) = delete;

  ~FakePrivilegedHostDeviceSetter() override;

  std::vector<std::pair<std::string, SetHostDeviceCallback>>& set_host_args() {
    return set_host_args_;
  }

 private:
  // mojom::PrivilegedHostDeviceSetter:
  void SetHostDevice(const std::string& host_instance_id_or_legacy_device_id,
                     SetHostDeviceCallback callback) override;

  std::vector<std::pair<std::string, SetHostDeviceCallback>> set_host_args_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_PRIVILEGED_HOST_DEVICE_SETTER_H_
