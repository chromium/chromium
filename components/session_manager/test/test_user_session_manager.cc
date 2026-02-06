// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/test/test_user_session_manager.h"

#include <memory>

#include "base/check.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"

namespace ash::test {

TestUserSessionManager::TestUserSessionManager(PrefService* local_state)
    : user_manager_(std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          local_state)),
      session_manager_(std::make_unique<session_manager::SessionManager>(
          std::make_unique<session_manager::FakeSessionManagerDelegate>())) {
  session_manager_->OnUserManagerCreated(user_manager_.Get());
}

TestUserSessionManager::~TestUserSessionManager() = default;

void TestUserSessionManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  user_manager::UserManager::RegisterPrefs(registry);
}

user_manager::User* TestUserSessionManager::AddRegularUser(
    const AccountId& account_id) {
  CHECK(session_manager_->sessions().empty());
  return user_manager::TestHelper(user_manager_.Get())
      .AddRegularUser(account_id);
}

user_manager::User* TestUserSessionManager::AddKioskChromeAppUser(
    std::string_view user_id) {
  CHECK(session_manager_->sessions().empty());
  return user_manager::TestHelper(user_manager_.Get())
      .AddKioskChromeAppUser(user_id);
}

void TestUserSessionManager::LogIn(const AccountId& account_id, bool new_user) {
  session_manager_->CreateSession(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
      new_user,
      /*has_active_session=*/false);
  session_manager_->SessionStarted();
}

}  // namespace ash::test
