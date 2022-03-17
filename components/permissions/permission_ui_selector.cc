// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_ui_selector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace permissions {

// static
bool PermissionUiSelector::ShouldSuppressAnimation(
    absl::optional<QuietUiReason> reason) {
  if (!reason)
    return true;

  switch (*reason) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      return false;
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
      return true;
  }
}

PermissionUiSelector::Decision::Decision(
    absl::optional<QuietUiReason> quiet_ui_reason,
    absl::optional<WarningReason> warning_reason,
    absl::optional<bool> decision_held_back)
    : quiet_ui_reason(quiet_ui_reason),
      warning_reason(warning_reason),
      decision_held_back(decision_held_back) {}
PermissionUiSelector::Decision::~Decision() = default;

PermissionUiSelector::Decision::Decision(const Decision&) = default;
PermissionUiSelector::Decision& PermissionUiSelector::Decision::operator=(
    const Decision&) = default;

// static
PermissionUiSelector::Decision
PermissionUiSelector::Decision::UseNormalUiAndShowNoWarning() {
  return Decision(UseNormalUi(), ShowNoWarning());
}

absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
PermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return absl::nullopt;
}

}  // namespace permissions
