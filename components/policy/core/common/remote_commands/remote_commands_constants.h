// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_

#include <string_view>

#include "components/policy/policy_export.h"

namespace policy {

// Returns GCP number for remote commands invalidations.
POLICY_EXPORT std::string_view GetRemoteCommandsInvalidationProjectNumber();

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_CONSTANTS_H_
