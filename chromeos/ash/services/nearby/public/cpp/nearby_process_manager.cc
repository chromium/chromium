// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"

namespace ash {
namespace nearby {

std::ostream& operator<<(
    std::ostream& os,
    const NearbyProcessManager::NearbyProcessShutdownReason& reason) {
  switch (reason) {
    case NearbyProcessManager::NearbyProcessShutdownReason::kNormal:
      return os << "Normal";
    case NearbyProcessManager::NearbyProcessShutdownReason::kCrash:
      return os << "Crash";
    case NearbyProcessManager::NearbyProcessShutdownReason::
        kConnectionsMojoPipeDisconnection:
      return os << "Connections Mojo Pipe Disconnection";
    case NearbyProcessManager::NearbyProcessShutdownReason::
        kPresenceMojoPipeDisconnection:
      return os << "Presence Mojo Pipe Disconnection";
    case NearbyProcessManager::NearbyProcessShutdownReason::
        kDecoderMojoPipeDisconnection:
      return os << "Decoder Mojo Pipe Disconnection";
  }
}

}  // namespace nearby
}  // namespace ash
