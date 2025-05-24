// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/user_login_permission_tracker.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/user_type.h"

namespace ash {
namespace {
UserLoginPermissionTracker* g_user_login_permission_tracker = nullptr;
}

// static
UserLoginPermissionTracker* UserLoginPermissionTracker::Get() {
  return g_user_login_permission_tracker;
}

UserLoginPermissionTracker::UserLoginPermissionTracker(
    CrosSettings* cros_settings)
    : cros_settings_(cros_settings) {
  CHECK(cros_settings_);
  if (g_user_login_permission_tracker) {
    // Test may create a duplicate instance when in
    // `ScopedCrosSettingsTestHelper`.
    CHECK_IS_TEST();
  } else {
    g_user_login_permission_tracker = this;
  }
}

UserLoginPermissionTracker::~UserLoginPermissionTracker() {
  if (g_user_login_permission_tracker == this) {
    g_user_login_permission_tracker = nullptr;
  }
}

bool UserLoginPermissionTracker::IsUserAllowlisted(
    const std::string& username,
    bool* wildcard_match,
    const std::optional<user_manager::UserType>& user_type) {
  if (username == demo_user_) {
    return true;
  }

  return cros_settings_->IsUserAllowlisted(username, wildcard_match, user_type);
}

void UserLoginPermissionTracker::SetDemoUser(const std::string& demo_user) {
  demo_user_ = demo_user;
}

}  // namespace ash
