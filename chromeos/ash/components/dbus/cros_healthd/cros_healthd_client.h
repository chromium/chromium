// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace dbus {
class Bus;
}

namespace ash::cros_healthd {

// D-Bus client for the cros_healthd service. Its only purpose is to bootstrap a
// Mojo connection to the cros_healthd daemon.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD)
    CrosHealthdClient {
 public:
  using BootstrapMojoConnectionCallback =
      base::OnceCallback<void(bool success)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CrosHealthdClient* Get();

  CrosHealthdClient(const CrosHealthdClient&) = delete;
  CrosHealthdClient& operator=(const CrosHealthdClient&) = delete;

  // Uses D-Bus to bootstrap the Mojo connection between the cros_healthd daemon
  // and the browser. Returns a bound remote.
  virtual mojo::Remote<mojom::CrosHealthdServiceFactory>
      BootstrapMojoConnection(BootstrapMojoConnectionCallback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CrosHealthdClient();
  virtual ~CrosHealthdClient();
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_
