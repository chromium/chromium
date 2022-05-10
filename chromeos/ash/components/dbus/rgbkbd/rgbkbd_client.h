// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_RGBKBD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_RGBKBD_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace ash {

// A class to make DBus calls for the org.chromium.Rgbkbd service.
class COMPONENT_EXPORT(RGBKBD_CLIENT) RgbkbdClient {
 public:
  using GetRgbKeyboardCapabilitiesCallback =
      DBusMethodCallback<rgbkbd::RgbKeyboardCapabilities>;

  RgbkbdClient(const RgbkbdClient&) = delete;
  RgbkbdClient& operator=(const RgbkbdClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static RgbkbdClient* Get();

  virtual void GetRgbKeyboardCapabilities(
      GetRgbKeyboardCapabilitiesCallback callback) = 0;

  virtual void SetCapsLockState(bool enabled) = 0;

  virtual void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b) = 0;

  virtual void SetRainbowMode() = 0;

  virtual void SetAnimationMode(rgbkbd::RgbAnimationMode mode) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  RgbkbdClient();
  virtual ~RgbkbdClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_RGBKBD_CLIENT_H_
