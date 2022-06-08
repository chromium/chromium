// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_bootstrap_controller.h"

namespace device {
class BluetoothAdapter;
}

namespace ash::quick_start {

class TargetDeviceBootstrapControllerImpl
    : public TargetDeviceBootstrapController {
 public:
  class Factory {
   public:
    static std::unique_ptr<TargetDeviceBootstrapController> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<TargetDeviceBootstrapControllerImpl>
    CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  using FeatureSupportStatus =
      TargetDeviceBootstrapController::FeatureSupportStatus;

  TargetDeviceBootstrapControllerImpl();
  TargetDeviceBootstrapControllerImpl(TargetDeviceBootstrapControllerImpl&) =
      delete;
  TargetDeviceBootstrapControllerImpl& operator=(
      TargetDeviceBootstrapControllerImpl&) = delete;
  ~TargetDeviceBootstrapControllerImpl() override;

  // TargetDeviceBootstrapController:
  FeatureSupportStatus GetFeatureSupportStatus() const override;

 private:
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  base::WeakPtrFactory<TargetDeviceBootstrapControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_IMPL_H_
