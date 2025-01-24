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
