// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_USER_LOGIN_PERMISSION_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_USER_LOGIN_PERMISSION_TRACKER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace user_manager {
enum class UserType;
}

namespace ash {
class CrosSettings;

// Check whether the user is allowed to login on device.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
    UserLoginPermissionTracker {
 public:
  static UserLoginPermissionTracker* Get();

  explicit UserLoginPermissionTracker(CrosSettings* cros_settings);
  UserLoginPermissionTracker(const UserLoginPermissionTracker&) = delete;
  UserLoginPermissionTracker& operator=(const UserLoginPermissionTracker&) =
      delete;
  ~UserLoginPermissionTracker();

  bool IsUserAllowlisted(
      const std::string& username,
      bool* wildcard_match,
      const std::optional<user_manager::UserType>& user_type);

  // Set current active `demo_user_` for demo mode. Should only used by demo
  // mode.
  void SetDemoUser(const std::string& demo_user);

 private:
  // Active demo user created in demo session. Should be allowlisted.
  std::string demo_user_;

  // Interface to the CrOS settings store.
  raw_ptr<CrosSettings> cros_settings_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_USER_LOGIN_PERMISSION_TRACKER_H_
