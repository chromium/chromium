// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/screenlock_state.h"

namespace proximity_auth {

std::ostream& operator<<(std::ostream& stream, const ScreenlockState& state) {
  switch (state) {
    case ScreenlockState::INACTIVE:
      stream << "[inactive]";
      break;
    case ScreenlockState::NO_BLUETOOTH:
      stream << "[no bluetooth]";
      break;
    case ScreenlockState::BLUETOOTH_CONNECTING:
      stream << "[bluetooth connecting]";
      break;
    case ScreenlockState::NO_PHONE:
      stream << "[no phone]";
      break;
    case ScreenlockState::PHONE_NOT_AUTHENTICATED:
      stream << "[phone not authenticated]";
      break;
    case ScreenlockState::PHONE_LOCKED:
      stream << "[phone locked]";
      break;
    case ScreenlockState::PHONE_NOT_LOCKABLE:
      stream << "[phone not lockable]";
      break;
    case ScreenlockState::PHONE_UNSUPPORTED:
      stream << "[phone unsupported]";
      break;
    case ScreenlockState::RSSI_TOO_LOW:
      stream << "[rssi too low]";
      break;
    case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
      stream << "[phone locked and rssi too low]";
      break;
    case ScreenlockState::AUTHENTICATED:
      stream << "[authenticated]";
      break;
    case ScreenlockState::PASSWORD_REAUTH:
      stream << "[password reauth]";
      break;
    case ScreenlockState::PRIMARY_USER_ABSENT:
      stream << "[primary user absent]";
      break;
  }

  return stream;
}

}  // namespace proximity_auth
