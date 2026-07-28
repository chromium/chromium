// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TEST_CONSTANTS_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TEST_CONSTANTS_H_

#include "components/browser_actuator/public/common.h"

namespace browser_actuator {

// Standard test-only constants for PayloadType.
inline constexpr PayloadType kTestPayloadTypeA = static_cast<PayloadType>(1);
inline constexpr PayloadType kTestPayloadTypeB = static_cast<PayloadType>(2);
inline constexpr PayloadType kTestPayloadTypeC = static_cast<PayloadType>(3);

// Standard test-only constants for FactoryId.
inline constexpr FactoryId kTestFactoryId1 = static_cast<FactoryId>(1);
inline constexpr FactoryId kTestFactoryId2 = static_cast<FactoryId>(2);
inline constexpr FactoryId kTestFactoryId3 = static_cast<FactoryId>(3);

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TEST_CONSTANTS_H_
