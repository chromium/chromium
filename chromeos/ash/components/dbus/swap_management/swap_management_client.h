// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_

#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
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

  // Send dbus message to swap_management for enabling zram writeback, targeted
  // on |size| MiB.
  virtual void SwapZramEnableWriteback(
      uint32_t size,
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void SwapZramSetWritebackLimit(
      uint32_t limit,
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void SwapZramMarkIdle(uint32_t age,
                                chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void InitiateSwapZramWriteback(
      swap_management::ZramWritebackMode mode,
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void MGLRUSetEnable(uint8_t value,
                              chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  SwapManagementClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_SWAP_MANAGEMENT_CLIENT_H_
