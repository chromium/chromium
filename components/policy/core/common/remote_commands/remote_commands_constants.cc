// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_constants.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/invalidation/invalidation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace policy {

BASE_FEATURE(kDeviceRemoteCommandsInvalidationWithDirectMessagesEnabled,
             "DeviceRemoteCommandsInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUserRemoteCommandsInvalidationWithDirectMessagesEnabled,
             "UserRemoteCommandsInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCbcmRemoteCommandsInvalidationWithDirectMessagesEnabled,
             "CbcmRemoteCommandsInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// GCP number to be used for remote commands invalidations. Remote commands are
// considered critical to receive invalidation.
constexpr std::string_view kRemoteCommandsInvalidationsProjectNumber =
    invalidation::kCriticalInvalidationsProjectNumber;

bool IsDirectInvalidationEnabledForScope(PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return base::FeatureList::IsEnabled(
          kUserRemoteCommandsInvalidationWithDirectMessagesEnabled);
    case PolicyInvalidationScope::kDevice:
      return base::FeatureList::IsEnabled(
          kDeviceRemoteCommandsInvalidationWithDirectMessagesEnabled);
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "Device local account commands are not supported.";
    case PolicyInvalidationScope::kCBCM:
      return base::FeatureList::IsEnabled(
          kCbcmRemoteCommandsInvalidationWithDirectMessagesEnabled);
  }
}

}  // namespace

std::string_view GetRemoteCommandsInvalidationProjectNumber(
    PolicyInvalidationScope scope) {
  if (IsDirectInvalidationEnabledForScope(scope)) {
    return kRemoteCommandsInvalidationsProjectNumber;
  }
  return kPolicyFCMInvalidationSenderID;
}

}  // namespace policy
