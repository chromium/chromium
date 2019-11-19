// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CUPS_PROXY_CUPS_PROXY_CLIENT_H_
#define CHROMEOS_DBUS_CUPS_PROXY_CUPS_PROXY_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// D-Bus client for the CupsProxyDaemon.
//
// Sole method bootstraps a Mojo connection between the daemon and Chrome,
// allowing Chrome to serve printing requests proxied by the daemon.
class COMPONENT_EXPORT(CUPS_PROXY) CupsProxyClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CupsProxyClient* Get();

  // Registers |callback| to run when the CupsProxyDaemon becomes available.
  // If the daemon is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Passes the file descriptor |fd| over D-Bus to the CupsProxyDaemon.
  // * The daemon expects a Mojo invitation in |fd| with an attached Mojo pipe.
  // * The daemon will bind the Mojo pipe to an
  //   chromeos::printing::mojom::CupsProxierPtr.
  // * Upon completion of the D-Bus call, |result_callback| will be invoked to
  //   indicate success or failure.
  // * This method will first wait for the CupsProxyService to become available.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CupsProxyClient();
  virtual ~CupsProxyClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CupsProxyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CUPS_PROXY_CUPS_PROXY_CLIENT_H_
