// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

namespace multidevice_setup {

// MultiDeviceSetup implementation which accepts receivers to bind to it.
class MultiDeviceSetupBase : public mojom::MultiDeviceSetup {
 public:
  MultiDeviceSetupBase(const MultiDeviceSetupBase&) = delete;
  MultiDeviceSetupBase& operator=(const MultiDeviceSetupBase&) = delete;

  ~MultiDeviceSetupBase() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver);
  void CloseAllReceivers();

  // Sets the device with the given ID as the multi-device host for this
  // account.
  // TODO(crbug.com/40105247): When v1 DeviceSync is turned off, only
  // use Instance ID since all devices are guaranteed to have one.
  virtual void SetHostDeviceWithoutAuthToken(
      const std::string& host_instance_id_or_legacy_device_id,
      mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback) = 0;

 protected:
  MultiDeviceSetupBase();

 private:
  mojo::ReceiverSet<mojom::MultiDeviceSetup> receivers_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_BASE_H_
