// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_TEST_USER_SESSION_TEST_ENVIRONMENT_H_
#define COMPONENTS_SESSION_MANAGER_TEST_USER_SESSION_TEST_ENVIRONMENT_H_

#include <map>
#include <memory>
#include <string_view>

#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"

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
class UserSessionTestEnvironment {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Creates and initializes the ProfileManager. Called during
    // UserSessionTestEnvironment construction after UserManager and
    // SessionManager are initialized.
    virtual void CreateProfileManager() = 0;

    // Destroys the ProfileManager. Called during UserSessionTestEnvironment
    // destruction before SessionManager and UserManager are destroyed.
    virtual void DestroyProfileManager() = 0;

    // Creates and initializes a Profile for the user specified by `account_id`.
    // Called during LogIn() after CreateSession() and before SessionStarted(),
    // as UserSessionManager creates the profile before the session becomes
    // active.
    // `initial_profile_prefs` is the dictionary of the initial values of the
    // Profile's prefs, keyed by pref names, which is given on adding the user.
    // Implementations are expected to create the Profile's PrefService with
    // the values, so the values are visible already on the Profile creation.
    virtual void CreateProfile(
        const AccountId& account_id,
        const base::DictValue& initial_profile_prefs) = 0;
  };

  // `local_state` must not be nullptr and must outlive this instance.
  // `delegate` may be nullptr for migration purposes.
  explicit UserSessionTestEnvironment(
      PrefService* local_state,
      std::unique_ptr<Delegate> delegate = nullptr);
  UserSessionTestEnvironment(const UserSessionTestEnvironment&) = delete;
  UserSessionTestEnvironment& operator=(const UserSessionTestEnvironment&) =
      delete;
  ~UserSessionTestEnvironment();

  // Registers LocalState's prefs that this test utility uses.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Following methods add a user. They must be called *ANY* of LogIn call.
  // Returns nullptr on error, so callers should check it always.
  // `initial_profile_prefs` is the dictionary of the initial values of the
  // user's Profile prefs, keyed by pref names. It is passed to
  // Delegate::CreateProfile() on LogIn(). It must be empty if Delegate is not
  // set. Guest user does not take it, because guest user's Profile is always
  // created freshly.
  [[nodiscard]] user_manager::User* AddRegularUser(
      const AccountId& account_id,
      base::DictValue initial_profile_prefs = {});
  [[nodiscard]] user_manager::User* AddChildUser(
      const AccountId& account_id,
      base::DictValue initial_profile_prefs = {});
  [[nodiscard]] user_manager::User* AddGuestUser();
  [[nodiscard]] user_manager::User* AddPublicAccountUser(
      std::string_view user_id,
      base::DictValue initial_profile_prefs = {});
  [[nodiscard]] user_manager::User* AddKioskChromeAppUser(
      std::string_view user_id,
      base::DictValue initial_profile_prefs = {});
  [[nodiscard]] user_manager::User* AddKioskWebAppUser(
      std::string_view user_id,
      base::DictValue initial_profile_prefs = {});
  [[nodiscard]] user_manager::User* AddKioskIwaUser(
      std::string_view user_id,
      base::DictValue initial_profile_prefs = {});

  // Logs in to a new user session with the user specified by `account_id`.
  void LogIn(const AccountId& account_id, bool new_user = false);

 private:
  // Remembers `initial_profile_prefs` for the `user` to be passed to
  // Delegate::CreateProfile() on LogIn(), if it is not empty.
  void MaybeSetInitialProfilePrefs(const user_manager::User& user,
                                   base::DictValue initial_profile_prefs);

  std::unique_ptr<Delegate> delegate_;
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  // Initial values of the Profile prefs for each user.
  std::map<AccountId, base::DictValue> initial_profile_prefs_;
};

}  // namespace ash::test

#endif  // COMPONENTS_SESSION_MANAGER_TEST_USER_SESSION_TEST_ENVIRONMENT_H_
