// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// CecServiceClient is used to communicate with org.chromium.CecService.
//
// CecService offers a small subset of HDMI CEC capabilities focused on power
// management of connected displays.
//
// All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS) CecServiceClient : public DBusClient {
 public:
  ~CecServiceClient() override;

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

  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<CecServiceClient> Create();

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
  // Let test code call protected DBusClient::Init().
  friend class CecServiceClientTest;

  CecServiceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CecServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_
