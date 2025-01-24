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

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(content::WebContents* web_contents,
                     permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

 private:
  Decision canned_decision_;
};

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_UI_SELECTOR_H_
