// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_STATUS_UPDATE_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_STATUS_UPDATE_H_

#include <memory>

#include "base/values.h"

namespace proximity_auth {

// Corresponds to the possible values for the 'user_presence' status update
// field.
enum UserPresence {
  USER_PRESENT,
  USER_ABSENT,
  USER_PRESENCE_UNKNOWN,
  USER_PRESENCE_SECONDARY,
  USER_PRESENCE_BACKGROUND,
};

// Corresponds to the possible values for the 'secure_screen_lock' status update
// field.
enum SecureScreenLockState {
  SECURE_SCREEN_LOCK_ENABLED,
  SECURE_SCREEN_LOCK_DISABLED,
  SECURE_SCREEN_LOCK_STATE_UNKNOWN,
};

// Corresponds to the possible values for the 'trust_agent' status update field.
enum TrustAgentState {
  TRUST_AGENT_ENABLED,
  TRUST_AGENT_DISABLED,
  TRUST_AGENT_UNSUPPORTED,
};

// Represents a 'status_update' message received from the remote device.
struct RemoteStatusUpdate {
  // Parses a dictionary value into a RemoteStatusUpdate. Returns a null pointer
  // if the serialized dictionary value is not valid.
  static std::unique_ptr<RemoteStatusUpdate> Deserialize(
      const base::Value::Dict& serialized_value);

  UserPresence user_presence;
  SecureScreenLockState secure_screen_lock_state;
  TrustAgentState trust_agent_state;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_STATUS_UPDATE_H_
