// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_TEST_TEST_USER_SESSION_MANAGER_H_
#define COMPONENTS_SESSION_MANAGER_TEST_TEST_USER_SESSION_MANAGER_H_

#include <memory>
#include <string_view>

#include "components/user_manager/scoped_user_manager.h"

class AccountId;
class PrefRegistrySimple;
class PrefService;

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash::test {

// Maintains UserManager and SessionManager for testing.
// It also provides several methods for the common testing operations.
class TestUserSessionManager {
 public:
  // `local_state` must not be nullptr and must outlive this instance.
  explicit TestUserSessionManager(PrefService* local_state);
  TestUserSessionManager(const TestUserSessionManager&) = delete;
  TestUserSessionManager& operator=(const TestUserSessionManager&) = delete;
  ~TestUserSessionManager();

  // Registers LocalState's prefs that this test utility uses.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Following methods add a user. They must be called *ANY* of LogIn call.
  // Returns nullptr on error, so callers should check it always.
  [[nodiscard]] user_manager::User* AddRegularUser(const AccountId& account_id);
  [[nodiscard]] user_manager::User* AddKioskChromeAppUser(
      std::string_view user_id);

  // Logs in to a new user session with the user specified by `account_id`.
  void LogIn(const AccountId& account_id, bool new_user = false);

 private:
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
};

}  // namespace ash::test

#endif  // COMPONENTS_SESSION_MANAGER_TEST_TEST_USER_SESSION_MANAGER_H_
