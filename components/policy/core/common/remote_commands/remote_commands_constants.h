// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_

#include <string_view>

#include "base/feature_list.h"
#include "components/policy/policy_export.h"

namespace policy {

enum class PolicyInvalidationScope;

POLICY_EXPORT BASE_DECLARE_FEATURE(
    kDeviceRemoteCommandsInvalidationWithDirectMessagesEnabled);
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kUserRemoteCommandsInvalidationWithDirectMessagesEnabled);
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kCbcmRemoteCommandsInvalidationWithDirectMessagesEnabled);

// Returns GCP number for remote commands invalidations of given `scope`.
POLICY_EXPORT std::string_view GetRemoteCommandsInvalidationProjectNumber(
    PolicyInvalidationScope scope);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_
