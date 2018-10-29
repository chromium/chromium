// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_

#include "chromeos/services/multidevice_setup/privileged_host_device_setter_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupBase;

// Concrete PrivilegedHostDeviceSetterBase implementation, which delegates
// SetHostDevice() calls to MultiDeviceSetupBase.
class PrivilegedHostDeviceSetterImpl : public PrivilegedHostDeviceSetterBase {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<PrivilegedHostDeviceSetterBase> BuildInstance(
        MultiDeviceSetupBase* multidevice_setup);

   private:
    static Factory* test_factory_;
  };

  ~PrivilegedHostDeviceSetterImpl() override;

 private:
  explicit PrivilegedHostDeviceSetterImpl(
      MultiDeviceSetupBase* multidevice_setup);

  // mojom::PrivilegedHostDeviceSetter:
  void SetHostDevice(const std::string& host_device_id,
                     SetHostDeviceCallback callback) override;

  MultiDeviceSetupBase* multidevice_setup_;

  DISALLOW_COPY_AND_ASSIGN(PrivilegedHostDeviceSetterImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_
