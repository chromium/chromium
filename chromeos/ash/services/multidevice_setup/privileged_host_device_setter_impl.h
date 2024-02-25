// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/multidevice_setup/privileged_host_device_setter_base.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

class MultiDeviceSetupBase;

// Concrete PrivilegedHostDeviceSetterBase implementation, which delegates
// SetHostDevice() calls to MultiDeviceSetupBase.
class PrivilegedHostDeviceSetterImpl : public PrivilegedHostDeviceSetterBase {
 public:
  class Factory {
   public:
    static std::unique_ptr<PrivilegedHostDeviceSetterBase> Create(
        MultiDeviceSetupBase* multidevice_setup);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PrivilegedHostDeviceSetterBase> CreateInstance(
        MultiDeviceSetupBase* multidevice_setup) = 0;

   private:
    static Factory* test_factory_;
  };

  PrivilegedHostDeviceSetterImpl(const PrivilegedHostDeviceSetterImpl&) =
      delete;
  PrivilegedHostDeviceSetterImpl& operator=(
      const PrivilegedHostDeviceSetterImpl&) = delete;

  ~PrivilegedHostDeviceSetterImpl() override;

 private:
  explicit PrivilegedHostDeviceSetterImpl(
      MultiDeviceSetupBase* multidevice_setup);

  // mojom::PrivilegedHostDeviceSetter:
  void SetHostDevice(const std::string& host_instance_id_or_legacy_device_id,
                     SetHostDeviceCallback callback) override;

  raw_ptr<MultiDeviceSetupBase> multidevice_setup_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_IMPL_H_
