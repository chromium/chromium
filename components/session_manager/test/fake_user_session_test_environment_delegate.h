// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_TEST_FAKE_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_
#define COMPONENTS_SESSION_MANAGER_TEST_FAKE_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"

class PrefRegistrySimple;
class TestingPrefServiceSimple;

namespace ash::test {

// Implementation of UserSessionTestEnvironment::Delegate for tests that do
// not (or cannot) handle Profile directly, e.g. tests under //ash,
// //chromeos or //components.
//
// Instead of creating a Profile, this creates a PrefService for the user on
// CreateProfile(), and notifies UserManager via OnUserProfileCreated(), as
// ProfileUserManagerController does for a real Profile in //chrome.
// So, after UserSessionTestEnvironment::LogIn(), the User is marked as its
// Profile being created, and User::GetProfilePrefs() returns the PrefService.
//
// See ChromeUserSessionTestEnvironmentDelegate for tests in //chrome that
// need a TestingProfile.
//
// Note: UserSessionTestEnvironment::LogIn() always calls CreateProfile(), so
// a user's "Profile" is always created on log-in. In the future, tests that
// verify behavior during the session initialization, i.e. before the
// Profile is created, may need to be supported. Keep it in mind especially
// for the integration with AshTestHelper.
class FakeUserSessionTestEnvironmentDelegate
    : public UserSessionTestEnvironment::Delegate {
 public:
  // Registers the prefs for a user's PrefService.
  using RegisterProfilePrefsCallback =
      base::RepeatingCallback<void(PrefRegistrySimple*)>;

  // If set, `register_profile_prefs` is called for each user's PrefService to
  // register prefs. Otherwise, no prefs are registered.
  // Note that no prefs are registered implicitly, including
  // user_manager::UserManager::RegisterProfilePrefs(). If a test needs them,
  // `register_profile_prefs` should call it.
  // TODO(crbug.com/473653626): Revisit this once more use cases are added,
  // if there's common boilerplate in `register_profile_prefs` to be
  // consolidated.
  explicit FakeUserSessionTestEnvironmentDelegate(
      RegisterProfilePrefsCallback register_profile_prefs = {});
  FakeUserSessionTestEnvironmentDelegate(
      const FakeUserSessionTestEnvironmentDelegate&) = delete;
  FakeUserSessionTestEnvironmentDelegate& operator=(
      const FakeUserSessionTestEnvironmentDelegate&) = delete;
  ~FakeUserSessionTestEnvironmentDelegate() override;

  // UserSessionTestEnvironment::Delegate:
  void CreateProfileManager() override;
  void DestroyProfileManager() override;
  void CreateProfile(const AccountId& account_id) override;

 private:
  const RegisterProfilePrefsCallback register_profile_prefs_;

  // Whether CreateProfileManager() is called and DestroyProfileManager() is
  // not yet.
  bool profile_manager_created_ = false;

  // PrefService for each user whose "Profile" is created.
  std::map<AccountId, std::unique_ptr<TestingPrefServiceSimple>> prefs_;
};

}  // namespace ash::test

#endif  // COMPONENTS_SESSION_MANAGER_TEST_FAKE_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_
