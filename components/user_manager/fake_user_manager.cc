// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/fake_user_manager.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/task_runner.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace {

class FakeTaskRunner : public base::TaskRunner {
 public:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    std::move(task).Run();
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}
};

}  // namespace

namespace user_manager {

FakeUserManager::FakeUserManager()
    : UserManagerBase(new FakeTaskRunner()), primary_user_(nullptr) {}

FakeUserManager::~FakeUserManager() {
}

const User* FakeUserManager::AddUser(const AccountId& account_id) {
  return AddUserWithAffiliation(account_id, false);
}

const User* FakeUserManager::AddChildUser(const AccountId& account_id) {
  User* user = User::CreateRegularUser(account_id, USER_TYPE_CHILD);
  users_.push_back(user);
  return user;
}

const User* FakeUserManager::AddGuestUser(const AccountId& account_id) {
  User* user = User::CreateGuestUser(account_id);
  users_.push_back(user);
  return user;
}

const User* FakeUserManager::AddUserWithAffiliation(const AccountId& account_id,
                                                    bool is_affiliated) {
  User* user = User::CreateRegularUser(account_id, USER_TYPE_REGULAR);
  user->SetAffiliation(is_affiliated);
  users_.push_back(user);
  return user;
}

const user_manager::User* FakeUserManager::AddPublicAccountUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreatePublicAccountUserForTesting(account_id);
  users_.push_back(user);
  return user;
}

void FakeUserManager::RemoveUserFromList(const AccountId& account_id) {
  const UserList::iterator it = std::find_if(
      users_.begin(), users_.end(), [&account_id](const User* user) {
        return user->GetAccountId() == account_id;
      });
  if (it != users_.end()) {
    if (primary_user_ == *it)
      primary_user_ = nullptr;
    if (active_user_ != *it)
      delete *it;
    users_.erase(it);
  }
}

const UserList& FakeUserManager::GetUsers() const {
  return users_;
}

UserList FakeUserManager::GetUsersAllowedForMultiProfile() const {
  UserList result;
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetType() == USER_TYPE_REGULAR && !(*it)->is_logged_in())
      result.push_back(*it);
  }
  return result;
}

void FakeUserManager::UpdateUserAccountData(
    const AccountId& account_id,
    const UserAccountData& account_data) {
  for (User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_display_name(account_data.display_name());
      user->set_given_name(account_data.given_name());
      return;
    }
  }
}

void FakeUserManager::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash,
                                   bool browser_restart,
                                   bool is_child) {
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->username_hash() == username_hash) {
      (*it)->set_is_logged_in(true);
      (*it)->SetProfileIsCreated();
      logged_in_users_.push_back(*it);

      if (!primary_user_)
        primary_user_ = *it;
      if (!active_user_)
        active_user_ = *it;
      break;
    }
  }

  if (!active_user_ && AreEphemeralUsersEnabled())
    RegularUserLoggedInAsEphemeral(account_id, USER_TYPE_REGULAR);
}

User* FakeUserManager::GetActiveUserInternal() const {
  if (active_user_ != nullptr)
    return active_user_;

  if (!users_.empty()) {
    if (active_account_id_.is_valid()) {
      for (UserList::const_iterator it = users_.begin(); it != users_.end();
           ++it) {
        if ((*it)->GetAccountId() == active_account_id_)
          return *it;
      }
    }
    return users_[0];
  }
  return nullptr;
}

const User* FakeUserManager::GetActiveUser() const {
  return GetActiveUserInternal();
}

User* FakeUserManager::GetActiveUser() {
  return GetActiveUserInternal();
}

void FakeUserManager::SwitchActiveUser(const AccountId& account_id) {}

void FakeUserManager::SaveUserDisplayName(const AccountId& account_id,
                                          const base::string16& display_name) {
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      (*it)->set_display_name(display_name);
      return;
    }
  }
}

const UserList& FakeUserManager::GetLRULoggedInUsers() const {
  return users_;
}

UserList FakeUserManager::GetUnlockUsers() const {
  return users_;
}

const AccountId& FakeUserManager::GetOwnerAccountId() const {
  return owner_account_id_;
}

bool FakeUserManager::IsKnownUser(const AccountId& account_id) const {
  return true;
}

const User* FakeUserManager::FindUser(const AccountId& account_id) const {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id)
    return active_user_;

  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      return *it;
  }

  return nullptr;
}

User* FakeUserManager::FindUserAndModify(const AccountId& account_id) {
  return nullptr;
}

const User* FakeUserManager::GetPrimaryUser() const {
  return primary_user_;
}

base::string16 FakeUserManager::GetUserDisplayName(
    const AccountId& account_id) const {
  return base::string16();
}

bool FakeUserManager::IsCurrentUserOwner() const {
  return false;
}

bool FakeUserManager::IsCurrentUserNew() const {
  return false;
}

bool FakeUserManager::IsCurrentUserNonCryptohomeDataEphemeral() const {
  return false;
}

bool FakeUserManager::CanCurrentUserLock() const {
  return false;
}

bool FakeUserManager::IsUserLoggedIn() const {
  return logged_in_users_.size() > 0;
}

bool FakeUserManager::IsLoggedInAsUserWithGaiaAccount() const {
  return true;
}

bool FakeUserManager::IsLoggedInAsPublicAccount() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsGuest() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsSupervisedUser() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == USER_TYPE_KIOSK_APP : false;
}

bool FakeUserManager::IsLoggedInAsArcKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == USER_TYPE_ARC_KIOSK_APP
                     : false;
}

bool FakeUserManager::IsLoggedInAsWebKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == USER_TYPE_WEB_KIOSK_APP
                     : false;
}

bool FakeUserManager::IsLoggedInAsAnyKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user && active_user->IsKioskType();
}

bool FakeUserManager::IsLoggedInAsStub() const {
  return false;
}

bool FakeUserManager::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  return false;
}

bool FakeUserManager::AreSupervisedUsersAllowed() const {
  return true;
}

bool FakeUserManager::IsGuestSessionAllowed() const {
  return true;
}

bool FakeUserManager::IsGaiaUserAllowed(const User& user) const {
  return true;
}

bool FakeUserManager::IsUserAllowed(const User& user) const {
  return true;
}

bool FakeUserManager::AreEphemeralUsersEnabled() const {
  return GetEphemeralUsersEnabled();
}

void FakeUserManager::SetEphemeralUsersEnabled(bool enabled) {
  UserManagerBase::SetEphemeralUsersEnabled(enabled);
}

const std::string& FakeUserManager::GetApplicationLocale() const {
  static const std::string default_locale("en-US");
  return default_locale;
}

PrefService* FakeUserManager::GetLocalState() const {
  return local_state_;
}

bool FakeUserManager::IsEnterpriseManaged() const {
  return false;
}

bool FakeUserManager::IsDemoApp(const AccountId& account_id) const {
  return account_id == DemoAccountId();
}

bool FakeUserManager::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return false;
}

void FakeUserManager::UpdateLoginState(const User* active_user,
                                       const User* primary_user,
                                       bool is_current_user_owner) const {}

bool FakeUserManager::GetPlatformKnownUserId(const std::string& user_email,
                                             const std::string& gaia_id,
                                             AccountId* out_account_id) const {
  if (user_email == kStubUserEmail) {
    *out_account_id = StubAccountId();
    return true;
  }

  if (user_email == kGuestUserName) {
    *out_account_id = GuestAccountId();
    return true;
  }
  return false;
}

const AccountId& FakeUserManager::GetGuestAccountId() const {
  return GuestAccountId();
}

bool FakeUserManager::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void FakeUserManager::AsyncRemoveCryptohome(const AccountId& account_id) const {
  NOTIMPLEMENTED();
}

bool FakeUserManager::IsGuestAccountId(const AccountId& account_id) const {
  return account_id == GuestAccountId();
}

bool FakeUserManager::IsStubAccountId(const AccountId& account_id) const {
  return account_id == StubAccountId();
}

bool FakeUserManager::IsSupervisedAccountId(const AccountId& account_id) const {
  return false;
}

bool FakeUserManager::HasBrowserRestarted() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return base::SysInfo::IsRunningOnChromeOS() &&
         command_line->HasSwitch(chromeos::switches::kLoginUser);
}

const gfx::ImageSkia& FakeUserManager::GetResourceImagekiaNamed(int id) const {
  NOTIMPLEMENTED();
  return empty_image_;
}

base::string16 FakeUserManager::GetResourceStringUTF16(int string_id) const {
  return base::string16();
}

void FakeUserManager::ScheduleResolveLocale(
    const std::string& locale,
    base::OnceClosure on_resolved_callback,
    std::string* out_resolved_locale) const {
  NOTIMPLEMENTED();
  return;
}

bool FakeUserManager::IsValidDefaultUserImageId(int image_index) const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace user_manager
