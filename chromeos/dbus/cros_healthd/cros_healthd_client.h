// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// D-Bus client for the cros_healthd service. Its only purpose is to bootstrap a
// Mojo connection to the cros_healthd daemon.
class COMPONENT_EXPORT(CROS_HEALTHD) CrosHealthdClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CrosHealthdClient* Get();

  // Uses D-Bus to bootstrap the Mojo connection between the cros_healthd daemon
  // and the browser. Returns a bound remote
  virtual mojo::Remote<cros_healthd::mojom::CrosHealthdService>
  BootstrapMojoConnection(
      base::OnceCallback<void(bool success)> result_callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CrosHealthdClient();
  virtual ~CrosHealthdClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosHealthdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_CROS_HEALTHD_CLIENT_H_
