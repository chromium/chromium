// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_

#include <ostream>

namespace ash::quick_start {

// Lists various types of QuickStartMessages possible.
enum class QuickStartMessageType {
  kSecondDeviceAuthPayload,  // secondDeviceAuthPayload in message
  kBootstrapOptions,         // bootstrapOptions in message
  kBootstrapConfigurations,  // bootstrapConfiguration in message
  kQuickStartPayload,        // quickStartPayload in message
  kBootstrapState,           // bootstrapState in message
};

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartMessageType& message_type);

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_
