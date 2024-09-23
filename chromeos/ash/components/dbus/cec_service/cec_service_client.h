// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_CEC_SERVICE_CLIENT_H_

#include <memory>
#include <vector>

#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// CecServiceClient is used to communicate with org.chromium.CecService.
//
// CecService offers a small subset of HDMI CEC capabilities focused on power
// management of connected displays.
//
// All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(ASH_DBUS_CEC_SERVICE) CecServiceClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static CecServiceClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  CecServiceClient(const CecServiceClient&) = delete;
  CecServiceClient& operator=(const CecServiceClient&) = delete;

  enum class PowerState {
    // There was an error when querying the display.
    kError,
    // The kernel adapter is not configured (no EDID set).
    kAdapterNotConfigured,
    // No response on the CEC bus (the connection was not ACKed).
    kNoDevice,
    // The display is on.
    kOn,
    // The display is in standby.
    kStandBy,
    // The display is transitioning from standby to a powered on state. It's not
    // guaranteed that any output is visible on the display at this stage.
    kTransitioningToOn,
    // The display is transitioning into standby mode.
    kTransitioningToStandBy,
    // A power status was read from the display but its value is unknown.
    kUnknown,
  };

  using PowerStateCallback =
      base::OnceCallback<void(const std::vector<PowerState>&)>;

  // Puts all connected HDMI CEC capable displays into stand-by mode. The effect
  // of calling this method is on a best effort basis, no guarantees of displays
  // going into stand-by is made.
  virtual void SendStandBy() = 0;

  // Wakes up all connected HDMI CEC capable displays from stand-by mode. The
  // effect of calling this method is on a best effort basis, no guarantees of
  // displays going into stand-by is made.
  virtual void SendWakeUp() = 0;

  // Queries all HDMI CEC capable displays for their current power state. The
  // effects of calling the methods above should be observable through this
  // inspection method.
  virtual void QueryDisplayCecPowerState(PowerStateCallback callback) = 0;

 protected:
  // Let test code call protected chromeos::DBusClient::Init().
  friend class CecServiceClientTest;

  CecServiceClient();
  ~CecServiceClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_CEC_SERVICE_CLIENT_H_
