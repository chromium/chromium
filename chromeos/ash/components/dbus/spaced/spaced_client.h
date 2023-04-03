// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace ash {

// A class to make DBus calls for the org.chromium.Spaced service.
class COMPONENT_EXPORT(SPACED_CLIENT) SpacedClient {
 public:
  using GetSizeCallback = chromeos::DBusMethodCallback<int64_t>;

  SpacedClient(const SpacedClient&) = delete;
  SpacedClient& operator=(const SpacedClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static SpacedClient* Get();

  // Gets free disk space available in bytes for the given file path.
  virtual void GetFreeDiskSpace(const std::string& path,
                                GetSizeCallback callback) = 0;

  // Gets total disk space available in bytes on the current partition for the
  // given file path.
  virtual void GetTotalDiskSpace(const std::string& path,
                                 GetSizeCallback callback) = 0;

  // Gets the total disk space available in bytes for usage on the device.
  virtual void GetRootDeviceSize(GetSizeCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SpacedClient();
  virtual ~SpacedClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_
