// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionPriority& connection_priority) {
  switch (connection_priority) {
    case ConnectionPriority::kLow:
      stream << "[low priority]";
      break;
    case ConnectionPriority::kMedium:
      stream << "[medium priority]";
      break;
    case ConnectionPriority::kHigh:
      stream << "[high priority]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
