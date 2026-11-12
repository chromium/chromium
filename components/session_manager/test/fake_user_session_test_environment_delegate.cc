// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/test/fake_user_session_test_environment_delegate.h"

#include <utility>

#include "base/check.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::test {

FakeUserSessionTestEnvironmentDelegate::FakeUserSessionTestEnvironmentDelegate(
    RegisterProfilePrefsCallback register_profile_prefs)
    : register_profile_prefs_(std::move(register_profile_prefs)) {}

FakeUserSessionTestEnvironmentDelegate::
    ~FakeUserSessionTestEnvironmentDelegate() {
  CHECK(!profile_manager_created_);
  CHECK(prefs_.empty());
}

void FakeUserSessionTestEnvironmentDelegate::CreateProfileManager() {
  // There's no ProfileManager to be created, unlike
  // ChromeUserSessionTestEnvironmentDelegate.
  CHECK(!profile_manager_created_);
  profile_manager_created_ = true;
}

void FakeUserSessionTestEnvironmentDelegate::DestroyProfileManager() {
  CHECK(profile_manager_created_);
  // Similar to ProfileUserManagerController, notifies UserManager before
  // destroying the PrefService of each user's "Profile".
  auto* user_manager = user_manager::UserManager::Get();
  for (const auto& [account_id, prefs] : prefs_) {
    user_manager->OnUserProfileWillBeDestroyed(account_id);
  }
  prefs_.clear();
  profile_manager_created_ = false;
}

void FakeUserSessionTestEnvironmentDelegate::CreateProfile(
    const AccountId& account_id) {
  CHECK(profile_manager_created_);
  CHECK(!prefs_.contains(account_id)) << account_id;

  // Creates the PrefService for the user's "Profile".
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  if (register_profile_prefs_) {
    register_profile_prefs_.Run(prefs->registry());
  }

  auto* user_manager = user_manager::UserManager::Get();

  // TODO(crbug.com/473653626): Consider calling
  // UserManager::SetUserPolicyStatus() here, before OnUserProfileCreated().
  // In production, the policy status is set while the Profile is being
  // initialized: UserCloudPolicyManagerAsh sets it for managed users, and
  // ProfileUserManagerController sets it as unmanaged if there's no policy
  // manager. ChromeUserSessionTestEnvironmentDelegate will need the
  // equivalent capability. How tests specify the status (e.g. managed and/or
  // affiliated) per user is not yet decided, and needs further investigation.

  // Then, notifies UserManager, as ProfileUserManagerController does on
  // Profile creation.
  CHECK(user_manager->OnUserProfileCreated(account_id, prefs.get()));
  prefs_.emplace(account_id, std::move(prefs));
}

}  // namespace ash::test
