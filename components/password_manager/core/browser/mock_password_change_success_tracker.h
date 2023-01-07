// Copyright 2022 The Chromium Authors
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

  MOCK_METHOD(void,
              OnChangePasswordFlowStarted,
              (const GURL& url,
               const std::string& username,
               StartEvent event_type,
               EntryPoint entry_point),
              (override));

  MOCK_METHOD(void,
              OnManualChangePasswordFlowStarted,
              (const GURL& url,
               const std::string& username,
               EntryPoint entry_point),
              (override));

  MOCK_METHOD(void,
              OnChangePasswordFlowModified,
              (const GURL& url, StartEvent new_event_type),
              (override));

  MOCK_METHOD(void,
              OnChangePasswordFlowModified,
              (const GURL& url,
               const std::string& username,
               StartEvent new_event_type),
              (override));

  MOCK_METHOD(void,
              OnChangePasswordFlowCompleted,
              (const GURL& url,
               const std::string& username,
               EndEvent event_type,
               bool phished),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
