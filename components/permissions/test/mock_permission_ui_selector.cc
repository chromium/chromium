// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_ui_selector.h"

void MockPermissionUiSelector::SelectUiToUse(
    content::WebContents* web_contents,
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  std::move(callback).Run(canned_decision_);
}

bool MockPermissionUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation;
}

std::optional<permissions::PermissionUiSelector::PredictionGrantLikelihood>
MockPermissionUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

std::optional<permissions::PermissionRequestRelevance>
MockPermissionUiSelector::PermissionRequestRelevanceForUKM() {
  return last_permission_request_relevance_;
}

std::optional<bool> MockPermissionUiSelector::WasSelectorDecisionHeldback() {
  return was_decision_held_back_;
}
