// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notification_permission_ui_selector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace permissions {

// static
bool NotificationPermissionUiSelector::ShouldSuppressAnimation(
    absl::optional<QuietUiReason> reason) {
  if (!reason)
    return true;

  switch (*reason) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kPredictedVeryUnlikelyGrant:
      return false;
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
      return true;
  }
}

NotificationPermissionUiSelector::Decision::Decision(
    absl::optional<QuietUiReason> quiet_ui_reason,
    absl::optional<WarningReason> warning_reason)
    : quiet_ui_reason(quiet_ui_reason), warning_reason(warning_reason) {}
NotificationPermissionUiSelector::Decision::~Decision() = default;

NotificationPermissionUiSelector::Decision::Decision(const Decision&) = default;
NotificationPermissionUiSelector::Decision&
NotificationPermissionUiSelector::Decision::operator=(const Decision&) =
    default;

// static
NotificationPermissionUiSelector::Decision
NotificationPermissionUiSelector::Decision::UseNormalUiAndShowNoWarning() {
  return Decision(UseNormalUi(), ShowNoWarning());
}

absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
NotificationPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return absl::nullopt;
}

}  // namespace permissions
