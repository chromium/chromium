// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CHANGE_SUCCESS_TRACKER_H_

#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// Mocked PasswordChangeSuccessTracker used by unit tests.
class MockPasswordChangeSuccessTracker : public PasswordChangeSuccessTracker {
 public:
  MockPasswordChangeSuccessTracker();
  ~MockPasswordChangeSuccessTracker() override;

  MOCK_METHOD3(OnChangePasswordFlowStarted,
               void(const GURL& url,
                    const std::string& username,
                    StartEvent event_type));
  MOCK_METHOD3(OnChangePasswordFlowCompleted,
               void(const GURL& url,
                    const std::string& username,
                    EndEvent event_type));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
