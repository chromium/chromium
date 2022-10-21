// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

namespace multidevice_setup {

// PrivilegedHostDeviceSetter implementation which accepts receivers to bind to
// it.
class PrivilegedHostDeviceSetterBase
    : public mojom::PrivilegedHostDeviceSetter {
 public:
  PrivilegedHostDeviceSetterBase(const PrivilegedHostDeviceSetterBase&) =
      delete;
  PrivilegedHostDeviceSetterBase& operator=(
      const PrivilegedHostDeviceSetterBase&) = delete;

  ~PrivilegedHostDeviceSetterBase() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::PrivilegedHostDeviceSetter> receiver);

 protected:
  PrivilegedHostDeviceSetterBase();

 private:
  mojo::ReceiverSet<mojom::PrivilegedHostDeviceSetter> receivers_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_
