// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_UI_SELECTOR_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_UI_SELECTOR_H_

#include "components/permissions/permission_ui_selector.h"

// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class MockPermissionUiSelector : public permissions::PermissionUiSelector {
 public:
  explicit MockPermissionUiSelector(const Decision& canned_decision)
      : canned_decision_(canned_decision) {}

  MockPermissionUiSelector(const MockPermissionUiSelector&) = delete;
  MockPermissionUiSelector& operator=(const MockPermissionUiSelector&) = delete;

  ~MockPermissionUiSelector() override = default;

  std::optional<permissions::PermissionUmaUtil::PredictionGrantLikelihood>
      last_request_grant_likelihood_;
  std::optional<permissions::PermissionRequestRelevance>
      last_permission_request_relevance_;
  std::optional<bool> was_decision_held_back_;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(content::WebContents* web_contents,
                     permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

  std::optional<permissions::PermissionUmaUtil::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM() override;

  std::optional<permissions::PermissionRequestRelevance>
  PermissionRequestRelevanceForUKM() override;

  std::optional<bool> WasSelectorDecisionHeldback() override;

 private:
  Decision canned_decision_;
};

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_UI_SELECTOR_H_
