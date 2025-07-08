// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/fake_user_manager.h"

#include <memory>

#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace user_manager {

FakeUserManager::FakeUserManager(PrefService* local_state)
    : UserManagerImpl(std::make_unique<FakeUserManagerDelegate>(),
                      local_state) {}

FakeUserManager::~FakeUserManager() = default;

std::string FakeUserManager::GetFakeUsernameHash(const AccountId& account_id) {
  return TestHelper::GetFakeUsernameHash(account_id);
}

void FakeUserManager::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash) {
  // Please keep the implementation in sync with
  // FakeChromeUserManager::UserLoggedIn. We're in process to merge.
  for (auto& user : user_storage_) {
    if (user->GetAccountId() == account_id) {
      user->set_is_logged_in(true);
      user->set_username_hash(username_hash);
      logged_in_users_.push_back(user.get());
      if (!primary_user_) {
        primary_user_ = user.get();
      }
      if (active_user_) {
        NotifyUserAddedToSession(user.get());
      } else {
        active_user_ = user.get();
      }
      break;
    }
  }

  NotifyOnLogin();
}

bool FakeUserManager::EnsureUser(const AccountId& account_id,
                                 UserType user_type,
                                 bool is_ephemeral) {
  NOTREACHED();
}

void FakeUserManager::SwitchActiveUser(const AccountId& account_id) {
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      active_user_ = (*it).get();
      break;
    }
  }

  if (active_user_ != nullptr) {
    NotifyActiveUserChanged(active_user_);
  }
}

}  // namespace user_manager
