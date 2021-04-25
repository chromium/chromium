// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
#define CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// ResourcedClient is used to communicate with the org.chromium.ResourceManager
// service. The browser uses the ResourceManager service to get resource usage
// status.
class COMPONENT_EXPORT(RESOURCED) ResourcedClient {
 public:
  // Memory margins. When available is below critical, it's memory pressure
  // level critical. When available is below moderate, it's memory pressure
  // level moderate. See base/memory/memory_pressure_listener.h enum
  // MemoryPressureLevel for the details.
  struct MemoryMarginsKB {
    uint64_t critical;
    uint64_t moderate;
  };

  ResourcedClient(const ResourcedClient&) = delete;
  ResourcedClient& operator=(const ResourcedClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static ResourcedClient* Get();

  // Get available memory.
  virtual void GetAvailableMemoryKB(DBusMethodCallback<uint64_t> callback) = 0;

  // Get memory margins.
  virtual void GetMemoryMarginsKB(
      DBusMethodCallback<MemoryMarginsKB> callback) = 0;

  // Attempt to enter game mode is state is true, exit if state is false.
  virtual void SetGameMode(bool state, DBusMethodCallback<bool> callback) = 0;

 protected:
  ResourcedClient();
  virtual ~ResourcedClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
