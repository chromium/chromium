// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_

#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// SwapManagementClient is used to communicate with the swap_management.
class COMPONENT_EXPORT(SWAP_MANAGEMENT) SwapManagementClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static SwapManagementClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  SwapManagementClient(const SwapManagementClient&) = delete;
  SwapManagementClient& operator=(const SwapManagementClient&) = delete;

  ~SwapManagementClient() override;

  virtual void MGLRUSetEnable(uint8_t value,
                              chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  SwapManagementClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_
