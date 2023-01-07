// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

#include "base/callback_forward.h"
#include "chromeos/ash/components/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd {

// Fake implementation of CrosHealthdClient.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD)
    FakeCrosHealthdClient : public CrosHealthdClient {
 public:
  // Callback type for bootstrapping the mojo connection.
  using BootstrapCallback =
      base::RepeatingCallback<mojo::Remote<mojom::CrosHealthdServiceFactory>()>;

  FakeCrosHealthdClient();
  FakeCrosHealthdClient(const FakeCrosHealthdClient&) = delete;
  FakeCrosHealthdClient& operator=(const FakeCrosHealthdClient&) = delete;
  ~FakeCrosHealthdClient() override;

  // Returns the global FakeCrosHealthdClient instance. Returns `nullptr` if
  // it is not initialized.
  static FakeCrosHealthdClient* Get();

  // Set the bootstrap callback.
  void SetBootstrapCallback(BootstrapCallback callback) {
    bootstrap_callback_ = callback;
  }

 private:
  // CrosHealthdClient overrides:
  mojo::Remote<mojom::CrosHealthdServiceFactory> BootstrapMojoConnection(
      BootstrapMojoConnectionCallback result_callback) override;

  // Callback for bootstrapping the mojo connection. This will be set after the
  // fake healthd mojo service initialized.
  BootstrapCallback bootstrap_callback_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
