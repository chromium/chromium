// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_METRICS_H_

namespace proximity_auth {
namespace metrics {

extern const int kUnknownProximityValue;

// Possible states of the remote device's security settings. This enum is used
// to back a histogram, and hence should be treated as append-only.
enum class RemoteSecuritySettingsState {
  UNKNOWN,
  SCREEN_LOCK_DISABLED_TRUST_AGENT_UNSUPPORTED,
  SCREEN_LOCK_DISABLED_TRUST_AGENT_DISABLED,
  SCREEN_LOCK_DISABLED_TRUST_AGENT_ENABLED,
  SCREEN_LOCK_ENABLED_TRUST_AGENT_UNSUPPORTED,
  SCREEN_LOCK_ENABLED_TRUST_AGENT_DISABLED,
  SCREEN_LOCK_ENABLED_TRUST_AGENT_ENABLED,
  COUNT
};

// Records the current |rolling_rssi| reading, upon a successful auth attempt.
// |rolling_rssi| should be set to |kUnknownProximityValue| if no RSSI readings
// are available.
void RecordAuthProximityRollingRssi(int rolling_rssi);

// Records the screen lock and trust agent settings state of the remote device,
// as received in a status update from the remote device.
void RecordRemoteSecuritySettingsState(RemoteSecuritySettingsState state);

}  // namespace metrics
}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_METRICS_H_
