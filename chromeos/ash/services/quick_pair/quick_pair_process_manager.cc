// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"

#include <ostream>

namespace ash {
namespace quick_pair {

std::ostream& operator<<(
    std::ostream& os,
    const QuickPairProcessManager::ShutdownReason& reason) {
  switch (reason) {
    case QuickPairProcessManager::ShutdownReason::kNormal:
      return os << "[Normal]";
    case QuickPairProcessManager::ShutdownReason::kCrash:
      return os << "[Crash]";
    case QuickPairProcessManager::ShutdownReason::
        kFastPairDataParserMojoPipeDisconnection:
      return os << "[FastPairDataParser Mojo Pipe Disconnection]";
  }
}

}  // namespace quick_pair
}  // namespace ash
