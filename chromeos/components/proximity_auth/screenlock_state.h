// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_STATE_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_STATE_H_

#include <ostream>

namespace proximity_auth {

// Possible user states of the proximity auth feature on the lock or sign-in
// screen.
enum class ScreenlockState {
  // Proximity-based authentication is not enabled, or the screen is not
  // locked.
  INACTIVE,
  // Bluetooth is not on.
  NO_BLUETOOTH,
  // The local device is in process of connecting to the remote device.
  BLUETOOTH_CONNECTING,
  // No phones eligible to unlock the local device can be found.
  NO_PHONE,
  // A phone eligible to unlock the local device is found, but cannot be
  // authenticated.
  PHONE_NOT_AUTHENTICATED,
  // A phone eligible to unlock the local device is found, but it's locked.
  PHONE_LOCKED,
  // A phone eligible to unlock the local device is found, but it does not have
  // a lock screen enabled.
  PHONE_NOT_LOCKABLE,
  // An enabled phone is found, but it is not allowed to unlock the local device
  // because it does not support reporting its lock screen state.
  PHONE_UNSUPPORTED,
  // A phone eligible to unlock the local device is found, but its received
  // signal strength is too low, i.e. the phone is roughly more than 30 feet
  // away, and therefore is not allowed to unlock the device.
  RSSI_TOO_LOW,
  // A phone eligible to unlock the local device is found; but (a) the phone is
  // locked, and (b) the local device's transmission power is too high,
  // indicating that the phone is (probably) more than 1 foot away, and
  // therefore is not allowed to unlock the device.
  PHONE_LOCKED_AND_RSSI_TOO_LOW,
  // The local device can be unlocked using proximity-based authentication.
  AUTHENTICATED,
  // The user must reauthenticate using their password because a sufficient time
  // has elapsed since their last password entry.
  PASSWORD_REAUTH,
  // The primary user profile is either in the background or this user is a
  // secondary user profile.
  PRIMARY_USER_ABSENT,
};

std::ostream& operator<<(std::ostream& stream, const ScreenlockState& state);

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_STATE_H_
