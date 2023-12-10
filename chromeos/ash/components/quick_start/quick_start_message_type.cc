// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_message_type.h"

namespace ash::quick_start {

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartMessageType& message_type) {
  switch (message_type) {
    case QuickStartMessageType::kSecondDeviceAuthPayload:
      stream << "SecondDeviceAuthPayload";
      break;
    case QuickStartMessageType::kBootstrapOptions:
      stream << "BootstrapOptions";
      break;
    case QuickStartMessageType::kBootstrapConfigurations:
      stream << "BootstrapConfigurations";
      break;
    case QuickStartMessageType::kQuickStartPayload:
      stream << "QuickStartPayload";
      break;
    case QuickStartMessageType::kBootstrapState:
      stream << "BootstrapState";
      break;
  }
  return stream;
}

}  // namespace ash::quick_start
