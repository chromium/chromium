// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/privileged_host_device_setter_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_base.h"

namespace ash {

namespace multidevice_setup {

// static
PrivilegedHostDeviceSetterImpl::Factory*
    PrivilegedHostDeviceSetterImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PrivilegedHostDeviceSetterBase>
PrivilegedHostDeviceSetterImpl::Factory::Create(
    MultiDeviceSetupBase* multidevice_setup) {
  if (test_factory_)
    return test_factory_->CreateInstance(multidevice_setup);

  return base::WrapUnique(
      new PrivilegedHostDeviceSetterImpl(multidevice_setup));
}

// static
void PrivilegedHostDeviceSetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PrivilegedHostDeviceSetterImpl::Factory::~Factory() = default;

PrivilegedHostDeviceSetterImpl::PrivilegedHostDeviceSetterImpl(
    MultiDeviceSetupBase* multidevice_setup)
    : multidevice_setup_(multidevice_setup) {}

PrivilegedHostDeviceSetterImpl::~PrivilegedHostDeviceSetterImpl() = default;

void PrivilegedHostDeviceSetterImpl::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    SetHostDeviceCallback callback) {
  multidevice_setup_->SetHostDeviceWithoutAuthToken(
      host_instance_id_or_legacy_device_id, std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace ash
