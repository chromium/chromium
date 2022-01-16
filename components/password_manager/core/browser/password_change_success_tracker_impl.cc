// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "url/gurl.h"

namespace password_manager {

PasswordChangeSuccessTrackerImpl::PasswordChangeSuccessTrackerImpl() = default;

PasswordChangeSuccessTrackerImpl::~PasswordChangeSuccessTrackerImpl() = default;

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    StartEvent event_type) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowCompleted(
    const GURL& url,
    const std::string& username,
    EndEvent event_type) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
}

}  // namespace password_manager
