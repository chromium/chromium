// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/test/user_session_test_environment.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"

namespace ash::test {

UserSessionTestEnvironment::UserSessionTestEnvironment(
    PrefService* local_state,
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      user_manager_(std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          local_state)),
      session_manager_(std::make_unique<session_manager::SessionManager>(
          std::make_unique<session_manager::FakeSessionManagerDelegate>())) {
  session_manager_->OnUserManagerCreated(user_manager_.Get());
  if (delegate_) {
    delegate_->CreateProfileManager();
  }
}

UserSessionTestEnvironment::~UserSessionTestEnvironment() {
  if (delegate_) {
    delegate_->DestroyProfileManager();
  }
}

void UserSessionTestEnvironment::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  user_manager::UserManager::RegisterPrefs(registry);
  user_manager::MultiUserSignInPolicyController::RegisterPrefs(registry);
}

user_manager::User* UserSessionTestEnvironment::AddRegularUser(
    const AccountId& account_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user =
      user_manager::TestHelper(user_manager_.Get()).AddRegularUser(account_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

user_manager::User* UserSessionTestEnvironment::AddChildUser(
    const AccountId& account_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user =
      user_manager::TestHelper(user_manager_.Get()).AddChildUser(account_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

user_manager::User* UserSessionTestEnvironment::AddGuestUser() {
  CHECK(session_manager_->sessions().empty());
  return user_manager::TestHelper(user_manager_.Get()).AddGuestUser();
}

user_manager::User* UserSessionTestEnvironment::AddPublicAccountUser(
    std::string_view user_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user = user_manager::TestHelper(user_manager_.Get())
                                 .AddPublicAccountUser(user_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

user_manager::User* UserSessionTestEnvironment::AddKioskChromeAppUser(
    std::string_view user_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user = user_manager::TestHelper(user_manager_.Get())
                                 .AddKioskChromeAppUser(user_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

user_manager::User* UserSessionTestEnvironment::AddKioskWebAppUser(
    std::string_view user_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user =
      user_manager::TestHelper(user_manager_.Get()).AddKioskWebAppUser(user_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

user_manager::User* UserSessionTestEnvironment::AddKioskIwaUser(
    std::string_view user_id,
    base::DictValue initial_profile_prefs) {
  CHECK(session_manager_->sessions().empty());
  user_manager::User* user =
      user_manager::TestHelper(user_manager_.Get()).AddKioskIwaUser(user_id);
  if (!user) {
    return nullptr;
  }
  MaybeSetInitialProfilePrefs(*user, std::move(initial_profile_prefs));
  return user;
}

void UserSessionTestEnvironment::LogIn(const AccountId& account_id,
                                       bool new_user) {
  session_manager_->CreateSession(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
      new_user,
      /*has_active_session=*/false);
  if (delegate_) {
    // Cast the empty dict to an lvalue so that the conditional expression
    // yields an lvalue without copying `*initial_profile_prefs`. The empty
    // dict is created only if no initial values are given, and lives until the
    // end of the full-expression, i.e. during CreateProfile().
    const base::DictValue* initial_profile_prefs =
        base::FindOrNull(initial_profile_prefs_, account_id);
    delegate_->CreateProfile(
        account_id, initial_profile_prefs ? *initial_profile_prefs
                                          : static_cast<const base::DictValue&>(
                                                base::DictValue()));
  }
  session_manager_->SessionStarted();
}

void UserSessionTestEnvironment::MaybeSetInitialProfilePrefs(
    const user_manager::User& user,
    base::DictValue initial_profile_prefs) {
  if (initial_profile_prefs.empty()) {
    return;
  }
  // The initial values are passed to the Delegate on the Profile creation.
  CHECK(delegate_) << "Delegate is required to set initial Profile prefs";
  initial_profile_prefs_.emplace(user.GetAccountId(),
                                 std::move(initial_profile_prefs));
}

}  // namespace ash::test
