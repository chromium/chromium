// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FEDERATED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FEDERATED_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"

namespace dbus {
class Bus;
}

namespace ash {

// D-Bus client for federated service. Its only purpose is to bootstrap a Mojo
// connection to the federated service daemon.
class COMPONENT_EXPORT(FEDERATED) FederatedClient {
 public:
  // Creates and initializes the global instance. `bus` must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static FederatedClient* Get();

  // Passes the file descriptor `fd` over D-Bus to the federated service daemon.
  // * The daemon expects a Mojo invitation in `fd` with an attached Mojo pipe.
  // * The daemon will bind the Mojo pipe to an implementation of
  //   chromeos::federated::mojom::FederatedService.
  // * Upon completion of the D-Bus call, `result_callback` will be invoked to
  //   indicate success or failure.
  // * This method will first wait for the federated service to become
  //   available.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  FederatedClient();
  virtual ~FederatedClient();
  FederatedClient(const FederatedClient&) = delete;
  FederatedClient& operator=(const FederatedClient&) = delete;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FEDERATED_CLIENT_H_
