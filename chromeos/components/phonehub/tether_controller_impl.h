// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_

#include "chromeos/components/phonehub/tether_controller.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace chromeos {
namespace phonehub {

// TetherController implementation which utilizes MultiDeviceSetupClient and
// CrosNetworkConfig in order to interact with Instant Tethering.
// TODO(khorimoto): Set the status depending on the current state; currently,
// this class is a stub.
class TetherControllerImpl
    : public TetherController,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  TetherControllerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~TetherControllerImpl() override;

 private:
  // TetherController:
  Status GetStatus() const override;
  void ScanForAvailableConnection() override;
  void AttemptConnection() override;
  void Disconnect() override;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  Status status_ = Status::kIneligibleForFeature;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_
