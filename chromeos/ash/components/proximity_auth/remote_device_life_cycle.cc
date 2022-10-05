// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle.h"

namespace proximity_auth {

using State = RemoteDeviceLifeCycle::State;

std::ostream& operator<<(std::ostream& stream, const State& state) {
  switch (state) {
    case State::STOPPED:
      stream << "[stopped]";
      break;
    case State::FINDING_CONNECTION:
      stream << "[finding connection]";
      break;
    case State::AUTHENTICATING:
      stream << "[authenticating]";
      break;
    case State::SECURE_CHANNEL_ESTABLISHED:
      stream << "[secure channel established]";
      break;
    case State::AUTHENTICATION_FAILED:
      stream << "[authentication failed]";
      break;
  }

  return stream;
}

}  // namespace proximity_auth
