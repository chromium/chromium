// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_ZR_VENDOR_OS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_ZR_VENDOR_OS_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace ash {

// ZrVendorOsClient is used to communicate with the zrvendorOs_service.
class COMPONENT_EXPORT(ZRVENDOROS) ZrVendorOsClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ZrVendorOsClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ZrVendorOsClient(const ZrVendorOsClient&) = delete;
  ZrVendorOsClient& operator=(const ZrVendorOsClient&) = delete;

  // Waits for the zrvendorOs_service to become available on D-Bus.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ZrVendorOsClient();
  ~ZrVendorOsClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_ZR_VENDOR_OS_CLIENT_H_
