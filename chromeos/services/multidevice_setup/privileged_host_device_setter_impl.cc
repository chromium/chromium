// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/privileged_host_device_setter_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"

namespace chromeos {

namespace multidevice_setup {

// static
PrivilegedHostDeviceSetterImpl::Factory*
    PrivilegedHostDeviceSetterImpl::Factory::test_factory_ = nullptr;

// static
PrivilegedHostDeviceSetterImpl::Factory*
PrivilegedHostDeviceSetterImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void PrivilegedHostDeviceSetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PrivilegedHostDeviceSetterImpl::Factory::~Factory() = default;

std::unique_ptr<PrivilegedHostDeviceSetterBase>
PrivilegedHostDeviceSetterImpl::Factory::BuildInstance(
    MultiDeviceSetupBase* multidevice_setup) {
  return base::WrapUnique(
      new PrivilegedHostDeviceSetterImpl(multidevice_setup));
}

PrivilegedHostDeviceSetterImpl::PrivilegedHostDeviceSetterImpl(
    MultiDeviceSetupBase* multidevice_setup)
    : multidevice_setup_(multidevice_setup) {}

PrivilegedHostDeviceSetterImpl::~PrivilegedHostDeviceSetterImpl() = default;

void PrivilegedHostDeviceSetterImpl::SetHostDevice(
    const std::string& host_device_id,
    SetHostDeviceCallback callback) {
  multidevice_setup_->SetHostDeviceWithoutAuthToken(host_device_id,
                                                    std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace chromeos
