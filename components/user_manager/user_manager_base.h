// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

class PrefRegistrySimple;

namespace base {
class ListValue;
class TaskRunner;
}

namespace user_manager {

// Hides all Supervised Users.
USER_MANAGER_EXPORT extern const base::Feature kHideSupervisedUsers;

class RemoveUserDelegate;

// Base implementation of the UserManager interface.
class USER_MANAGER_EXPORT UserManagerBase : public UserManager {
 public:
  // Creates UserManagerBase with |task_runner| for UI thread and
  // |blocking_task_runner| for SequencedWorkerPool.
  explicit UserManagerBase(scoped_refptr<base::TaskRunner> task_runner);
  ~UserManagerBase() override;

  // Registers UserManagerBase preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // UserManager implementation:
  void Shutdown() override;
  const UserList& GetUsers() const override;
  const UserList& GetLoggedInUsers() const override;
  const UserList& GetLRULoggedInUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart,
                    bool is_child) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SwitchToLastActiveUser() override;
  void OnSessionStarted() override;
  void RemoveUser(const AccountId& account_id,
                  RemoveUserDelegate* delegate) override;
  void RemoveUserFromList(const AccountId& account_id) override;
  bool IsKnownUser(const AccountId& account_id) const override;
  const User* FindUser(const AccountId& account_id) const override;
  User* FindUserAndModify(const AccountId& account_id) override;
  const User* GetActiveUser() const override;
  User* GetActiveUser() override;
  const User* GetPrimaryUser() const override;
  void SaveUserOAuthStatus(const AccountId& account_id,
                           User::OAuthTokenStatus oauth_token_status) override;
  void SaveForceOnlineSignin(const AccountId& account_id,
                             bool force_online_signin) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const base::string16& display_name) override;
  base::string16 GetUserDisplayName(const AccountId& account_id) const override;
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override;
  void SaveUserType(const User* user) override;
  void UpdateUserAccountData(const AccountId& account_id,
                             const UserAccountData& account_data) override;
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool IsCurrentUserCryptohomeDataEphemeral() const override;
  bool CanCurrentUserLock() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsChildUser() const override;
  bool IsLoggedInAsPublicAccount() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsSupervisedUser() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsArcKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsAnyKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsUserCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  void AddObserver(UserManager::Observer* obs) override;
  void RemoveObserver(UserManager::Observer* obs) override;
  void AddSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) override;
  void RemoveSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) override;
  void NotifyLocalStateChanged() override;
  void NotifyUserImageChanged(const User& user) override;
  void NotifyUserProfileImageUpdateFailed(const User& user) override;
  void NotifyUserProfileImageUpdated(
      const User& user,
      const gfx::ImageSkia& profile_image) override;
  void NotifyUsersSignInConstraintsChanged() override;
  void Initialize() override;

  // This method updates "User was added to the device in this session nad is
  // not full initialized yet" flag.
  virtual void SetIsCurrentUserNew(bool is_new);

  // Helper function that converts users from |users_list| to |users_vector| and
  // |users_set|. Duplicates and users already present in |existing_users| are
  // skipped.
  void ParseUserList(const base::ListValue& users_list,
                     const std::set<AccountId>& existing_users,
                     std::vector<AccountId>* users_vector,
                     std::set<AccountId>* users_set);

  // Returns true if trusted device policies have successfully been retrieved
  // and ephemeral users are enabled.
  virtual bool AreEphemeralUsersEnabled() const = 0;

  void AddUserRecordForTesting(User* user) {
    return AddUserRecord(user);
  }

  // Returns true if device is enterprise managed.
  virtual bool IsEnterpriseManaged() const = 0;

 protected:
  // Adds |user| to users list, and adds it to front of LRU list. It is assumed
  // that there is no user with same id.
  virtual void AddUserRecord(User* user);

  // Returns true if user may be removed.
  virtual bool CanUserBeRemoved(const User* user) const;

  // A wrapper around C++ delete operator. Deletes |user|, and when |user|
  // equals to active_user_, active_user_ is reset to NULL.
  virtual void DeleteUser(User* user);

  // Returns the locale used by the application.
  virtual const std::string& GetApplicationLocale() const = 0;

  // Loads |users_| from Local State if the list has not been loaded yet.
  // Subsequent calls have no effect. Must be called on the UI thread.
  virtual void EnsureUsersLoaded();

  // Handle OAuth token |status| change for |account_id|.
  virtual void HandleUserOAuthTokenStatusChange(
      const AccountId& account_id,
      User::OAuthTokenStatus status) const = 0;

  // Loads device local accounts from the Local state and fills in
  // |device_local_accounts_set|.
  virtual void LoadDeviceLocalAccounts(
      std::set<AccountId>* device_local_accounts_set) = 0;

  // Notifies observers that active user has changed.
  void NotifyActiveUserChanged(User* active_user);

  // Notifies that user has logged in.
  virtual void NotifyOnLogin();

  // Notifies observers that another user was added to the session.
  // If |user_switch_pending| is true this means that user has not been fully
  // initialized yet like waiting for profile to be loaded.
  virtual void NotifyUserAddedToSession(const User* added_user,
                                        bool user_switch_pending);

  // Performs any additional actions before user list is loaded.
  virtual void PerformPreUserListLoadingActions() = 0;

  // Performs any additional actions after user list is loaded.
  virtual void PerformPostUserListLoadingActions() = 0;

  // Performs any additional actions after UserLoggedIn() execution has been
  // completed.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  virtual void PerformPostUserLoggedInActions(bool browser_restart) = 0;

  // Implementation for RemoveUser method. It is synchronous. It is called from
  // RemoveUserInternal after owner check.
  virtual void RemoveNonOwnerUserInternal(const AccountId& account_id,
                                          RemoveUserDelegate* delegate);

  // Removes a regular or supervised user from the user list.
  // Returns the user if found or NULL otherwise.
  // Also removes the user from the persistent user list.
  // |notify| is true when OnUserRemoved() should be triggered,
  // meaning that the user won't be added after the removal.
  User* RemoveRegularOrSupervisedUserFromList(const AccountId& account_id,
                                              bool notify);

  // Implementation for RemoveUser method. This is an asynchronous part of the
  // method, that verifies that owner will not get deleted, and calls
  // |RemoveNonOwnerUserInternal|.
  virtual void RemoveUserInternal(const AccountId& account_id,
                                  RemoveUserDelegate* delegate);

  // Removes data stored or cached outside the user's cryptohome (wallpaper,
  // avatar, OAuth token status, display name, display email).
  virtual void RemoveNonCryptohomeData(const AccountId& account_id);

  // Check for a particular user type.

  // Returns true if |account_id| represents demo app.
  virtual bool IsDemoApp(const AccountId& account_id) const = 0;

  // These methods are called when corresponding user type has signed in.

  // Indicates that the demo account has just logged in.
  virtual void DemoAccountLoggedIn() = 0;

  // Indicates that a user just logged in as guest.
  virtual void GuestUserLoggedIn();

  // Indicates that a kiosk app robot just logged in.
  virtual void KioskAppLoggedIn(User* user) = 0;

  // Indicates that an ARC kiosk app robot just logged in.
  virtual void ArcKioskAppLoggedIn(User* user) = 0;

  // Indicates that an web kiosk app robot just logged in.
  virtual void WebKioskAppLoggedIn(User* user) = 0;

  // Indicates that a user just logged into a public session.
  virtual void PublicAccountUserLoggedIn(User* user) = 0;

  // Indicates that a regular user just logged in.
  virtual void RegularUserLoggedIn(const AccountId& account_id,
                                   const UserType user_type);

  // Indicates that a regular user just logged in as ephemeral.
  virtual void RegularUserLoggedInAsEphemeral(const AccountId& account_id,
                                              const UserType user_type);

  // Indicates that a supervised user just logged in.
  virtual void SupervisedUserLoggedIn(const AccountId& account_id) = 0;

  // Should be called when regular user was removed.
  virtual void OnUserRemoved(const AccountId& account_id) = 0;

  // Update the global LoginState.
  virtual void UpdateLoginState(const User* active_user,
                                const User* primary_user,
                                bool is_current_user_owner) const = 0;

  // Getters/setters for private members.

  virtual bool GetEphemeralUsersEnabled() const;
  virtual void SetEphemeralUsersEnabled(bool enabled);

  virtual void SetOwnerId(const AccountId& owner_account_id);

  virtual const AccountId& GetPendingUserSwitchID() const;
  virtual void SetPendingUserSwitchId(const AccountId& account_id);

  // The logged-in user that is currently active in current session.
  // NULL until a user has logged in, then points to one
  // of the User instances in |users_|, the |guest_user_| instance or an
  // ephemeral user instance.
  User* active_user_ = nullptr;

  // The primary user of the current session. It is recorded for the first
  // signed-in user and does not change thereafter.
  User* primary_user_ = nullptr;

  // List of all known users. User instances are owned by |this|. Regular users
  // are removed by |RemoveUserFromList|, device local accounts by
  // |UpdateAndCleanUpDeviceLocalAccounts|.
  UserList users_;

  // List of all users that are logged in current session. These point to User
  // instances in |users_|. Only one of them could be marked as active.
  UserList logged_in_users_;

  // A list of all users that are logged in the current session. In contrast to
  // |logged_in_users|, the order of this list is least recently used so that
  // the active user should always be the first one in the list.
  UserList lru_logged_in_users_;

 private:
  // Stages of loading user list from preferences. Some methods can have
  // different behavior depending on stage.
  enum UserLoadStage { STAGE_NOT_LOADED = 0, STAGE_LOADING, STAGE_LOADED };

  // Returns a list of users who have logged into this device previously.
  // Same as GetUsers but used if you need to modify User from that list.
  UserList& GetUsersAndModify();

  // Returns the user with the given email address if found in the persistent
  // list. Returns |NULL| otherwise.
  const User* FindUserInList(const AccountId& account_id) const;

  // Returns |true| if user with the given id is found in the persistent list.
  // Returns |false| otherwise. Does not trigger user loading.
  bool UserExistsInList(const AccountId& account_id) const;

  // Same as FindUserInList but returns non-const pointer to User object.
  User* FindUserInListAndModify(const AccountId& account_id);

  // Reads user's oauth token status from local state preferences.
  User::OAuthTokenStatus LoadUserOAuthStatus(const AccountId& account_id) const;

  // Read a flag indicating whether online authentication against GAIA should
  // be enforced during the user's next sign-in from local state preferences.
  bool LoadForceOnlineSignin(const AccountId& account_id) const;

  // Read a flag indicating whether session initialization has completed at
  // least once.
  bool LoadSessionInitialized(const AccountId& account_id) const;

  // Notifies observers that merge session state had changed.
  void NotifyMergeSessionStateChanged();

  // Notifies observers that active account_id hash has changed.
  void NotifyActiveUserHashChanged(const std::string& hash);

  // Call UpdateLoginState.
  void CallUpdateLoginState();

  // Insert |user| at the front of the LRU user list.
  void SetLRUUser(User* user);

  // Sends metrics in response to a user with gaia account (regular) logging in.
  void SendGaiaUserLoginMetrics(const AccountId& account_id);

  // Sets account locale for user with id |account_id|.
  virtual void UpdateUserAccountLocale(const AccountId& account_id,
                                       const std::string& locale);

  // Updates user account after locale was resolved.
  void DoUpdateAccountLocale(const AccountId& account_id,
                             std::unique_ptr<std::string> resolved_locale);

  // Indicates stage of loading user from prefs.
  UserLoadStage user_loading_stage_ = STAGE_NOT_LOADED;

  // Cached flag of whether the currently logged-in user existed before this
  // login.
  bool is_current_user_new_ = false;

  // Cached flag of whether the currently logged-in user is a regular user who
  // logged in as ephemeral. Storage of persistent information is avoided for
  // such users by not adding them to the persistent user list, not downloading
  // their custom avatars and mounting their cryptohomes using tmpfs. Defaults
  // to |false|.
  bool is_current_user_ephemeral_regular_user_ = false;

  // Cached flag indicating whether the ephemeral user policy is enabled.
  // Defaults to |false| if the value has not been read from trusted device
  // policy yet.
  bool ephemeral_users_enabled_ = false;

  // Cached name of device owner. Defaults to empty if the value has not
  // been read from trusted device policy yet.
  AccountId owner_account_id_ = EmptyAccountId();

  base::ObserverList<UserManager::Observer>::Unchecked observer_list_;

  // TODO(nkostylev): Merge with session state refactoring CL.
  base::ObserverList<UserManager::UserSessionStateObserver>::Unchecked
      session_state_observer_list_;

  // Time at which this object was created.
  base::TimeTicks manager_creation_time_ = base::TimeTicks::Now();

  // ID of the user just added to the session that needs to be activated
  // as soon as user's profile is loaded.
  AccountId pending_user_switch_ = EmptyAccountId();

  // ID of the user that was active in the previous session.
  // Preference value is stored here before first user signs in
  // because pref will be overidden once session restore starts.
  AccountId last_session_active_account_id_ = EmptyAccountId();
  bool last_session_active_account_id_initialized_ = false;

  // TaskRunner for UI thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<UserManagerBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserManagerBase);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
