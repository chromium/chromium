// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_

namespace ash::quick_start {

// Lists various types of QuickStartMessages possible.
enum class QuickStartMessageType {
  kSecondDeviceAuthPayload,  // secondDeviceAuthPayload in message
  kBootstrapConfigurations,  // bootstrapConfiguration in message
  kFidoMessage,              // fidoMessage in message
  kQuickStartPayload,        // quickStartPayload in message
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_TYPE_H_
