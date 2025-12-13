// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permission_ui_selector.h"

#include <optional>

namespace permissions {

// static
bool PermissionUiSelector::ShouldSuppressAnimation(
    std::optional<QuietUiReason> reason) {
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
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      return true;
  }
}

PermissionUiSelector::Decision::Decision(
    std::optional<QuietUiReason> quiet_ui_reason,
    std::optional<WarningReason> warning_reason,
    GeolocationAccuracy geolocation_accuracy)
    : quiet_ui_reason(quiet_ui_reason),
      warning_reason(warning_reason),
      geolocation_accuracy(geolocation_accuracy) {}
PermissionUiSelector::Decision::~Decision() = default;

PermissionUiSelector::Decision::Decision(const Decision&) = default;
PermissionUiSelector::Decision& PermissionUiSelector::Decision::operator=(
    const Decision&) = default;

bool PermissionUiSelector::Decision::operator==(const Decision&) const =
    default;

// static
PermissionUiSelector::Decision
PermissionUiSelector::Decision::UseNormalUiAndShowNoWarning() {
  return Decision::UseNormalUi(std::nullopt);
}

// static
PermissionUiSelector::Decision PermissionUiSelector::Decision::UseNormalUi(
    std::optional<WarningReason> warning_reason,
    GeolocationAccuracy geolocation_accuracy) {
  return Decision(std::nullopt, warning_reason, geolocation_accuracy);
}

// static
PermissionUiSelector::Decision PermissionUiSelector::Decision::UseQuietUi(
    QuietUiReason quiet_ui_reason,
    std::optional<WarningReason> warning_reason) {
  return Decision(quiet_ui_reason, warning_reason,
                  GeolocationAccuracy::kUnspecified);
}

std::optional<PermissionUiSelector::PredictionGrantLikelihood>
PermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return std::nullopt;
}

std::optional<PermissionRequestRelevance>
PermissionUiSelector::PermissionRequestRelevanceForUKM() {
  return std::nullopt;
}

std::optional<permissions::PermissionAiRelevanceModel>
PermissionUiSelector::PermissionAiRelevanceModelForUKM() {
  return std::nullopt;
}

std::optional<bool> PermissionUiSelector::WasSelectorDecisionHeldback() {
  return std::nullopt;
}

}  // namespace permissions
