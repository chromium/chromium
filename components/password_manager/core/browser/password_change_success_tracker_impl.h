// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_

#include "components/password_manager/core/browser/password_change_success_tracker.h"

class GURL;

namespace password_manager {

class PasswordChangeSuccessTrackerImpl
    : public password_manager::PasswordChangeSuccessTracker {
 public:
  PasswordChangeSuccessTrackerImpl();

  ~PasswordChangeSuccessTrackerImpl() override;

  void OnChangePasswordFlowStarted(const GURL& url,
                                   const std::string& username,
                                   StartEvent event_type) override;

  void OnChangePasswordFlowCompleted(const GURL& url,
                                     const std::string& username,
                                     EndEvent event_type) override;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
