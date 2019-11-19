// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace multidevice_setup {

// MultiDeviceSetup implementation which accepts receivers to bind to it.
class MultiDeviceSetupBase : public mojom::MultiDeviceSetup {
 public:
  ~MultiDeviceSetupBase() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver);
  void CloseAllReceivers();

  // Sets the device with the given ID as the multi-device host for this
  // account.
  virtual void SetHostDeviceWithoutAuthToken(
      const std::string& host_device_id,
      mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback) = 0;

 protected:
  MultiDeviceSetupBase();

 private:
  mojo::ReceiverSet<mojom::MultiDeviceSetup> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupBase);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_
