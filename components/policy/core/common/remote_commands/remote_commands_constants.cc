// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_constants.h"

#include <string_view>

#include "components/invalidation/invalidation_constants.h"
#include "components/invalidation/invalidation_features.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

namespace {
// GCP number to be used for remote commands invalidations. Remote commands are
// considered critical to receive invalidation.
constexpr std::string_view kRemoteCommandsInvalidationsProjectNumber =
    invalidation::kCriticalInvalidationsProjectNumber;
}  // namespace

std::string_view GetRemoteCommandsInvalidationProjectNumber() {
  if (invalidation::IsInvalidationsWithDirectMessagesEnabled()) {
    return kRemoteCommandsInvalidationsProjectNumber;
  }
  return kPolicyFCMInvalidationSenderID;
}

}  // namespace policy
