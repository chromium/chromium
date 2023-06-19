// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;

namespace base {
class SingleThreadTaskRunner;
}

namespace user_manager {

// Feature that removes legacy supervised users.
BASE_DECLARE_FEATURE(kRemoveLegacySupervisedUsersOnStartup);

// Base implementation of the UserManager interface.
class USER_MANAGER_EXPORT UserManagerBase : public UserManager {
 public:
  // These enum values represent a legacy supervised user's (LSU) status on the
  // sign in screen.
  // TODO(crbug/1155729): Remove once all LSUs deleted in the wild. LSUs were
  // first hidden on the login screen in M74. Assuming a five year AUE, we
  // should stop supporting devices with LSUs by 2024.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "LegacySupervisedUserStatus" in src/tools/metrics/histograms/enums.xml.
  enum class LegacySupervisedUserStatus {
    // Non-LSU Gaia user displayed on login screen.
    kGaiaUserDisplayed = 0,
    // LSU hidden on login screen. Expect this count to decline to zero over
    // time as we delete LSUs.
    kLSUHidden = 1,
    // LSU attempted to delete cryptohome. Expect this count to decline to zero
    // over time as we delete LSUs.
    kLSUDeleted = 2,
    // Add future entires above this comment, in sync with
    // "LegacySupervisedUserStatus" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kLSUDeleted
  };

  // Creates UserManagerBase with |task_runner| for UI thread, and given
  // |local_state|. |local_state| must outlive this UserManager.
  UserManagerBase(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  PrefService* local_state);

  UserManagerBase(const UserManagerBase&) = delete;
  UserManagerBase& operator=(const UserManagerBase&) = delete;

  ~UserManagerBase() override;

  // Histogram for tracking the number of deprecated legacy supervised user
  // cryptohomes remaining in the wild.
  static const char kLegacySupervisedUsersHistogramName[];

  // Registers UserManagerBase preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // UserManager implementation:
  void Shutdown() override;
  const UserList& GetUsers() const override;
  const UserList& GetLoggedInUsers() const override;
  const UserList& GetLRULoggedInUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void GetOwnerAccountIdAsync(
      base::OnceCallback<void(const AccountId&)> callback) const override;

  const AccountId& GetLastSessionActiveAccountId() const override;
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart,
                    bool is_child) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SwitchToLastActiveUser() override;
  void OnSessionStarted() override;
  void RemoveUser(const AccountId& account_id,
                  UserRemovalReason reason) override;
  void RemoveUserFromList(const AccountId& account_id) override;
  void RemoveUserFromListForRecreation(const AccountId& account_id) override;
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
                           const std::u16string& display_name) override;
  std::u16string GetUserDisplayName(const AccountId& account_id) const override;
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override;
  UserType GetUserType(const AccountId& account_id) override;
  void SaveUserType(const User* user) override;
  absl::optional<std::string> GetOwnerEmail() override;
  void RecordOwner(const AccountId& owner) override;
  void UpdateUserAccountData(const AccountId& account_id,
                             const UserAccountData& account_data) override;
  bool IsOwnerUser(const User* user) const override;
  bool IsPrimaryUser(const User* user) const override;
  bool IsEphemeralUser(const User* user) const override;
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const final;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool IsCurrentUserCryptohomeDataEphemeral() const override;
  bool CanCurrentUserLock() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsChildUser() const override;
  bool IsLoggedInAsPublicAccount() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsArcKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsAnyKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsUserCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsEphemeralAccountId(const AccountId& account_id) const final;
  void AddObserver(UserManager::Observer* obs) override;
  void RemoveObserver(UserManager::Observer* obs) override;
  void AddSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) override;
  void RemoveSessionStateObserver(
      UserManager::UserSessionStateObserver* obs) override;
  void NotifyLocalStateChanged() override;
  void NotifyUserImageChanged(const User& user) override;
  void NotifyUserImageIsEnterpriseManagedChanged(
      const User& user,
      bool is_enterprise_managed) override;
  void NotifyUserProfileImageUpdateFailed(const User& user) override;
  void NotifyUserProfileImageUpdated(
      const User& user,
      const gfx::ImageSkia& profile_image) override;
  void NotifyUsersSignInConstraintsChanged() override;
  void NotifyUserAffiliationUpdated(const User& user) override;
  void NotifyUserToBeRemoved(const AccountId& account_id) override;
  void NotifyUserRemoved(const AccountId& account_id,
                         UserRemovalReason reason) override;
  PrefService* GetLocalState() const final;
  bool IsFirstExecAfterBoot() const final;
  bool HasBrowserRestarted() const final;

  void Initialize() override;

  // This method updates "User was added to the device in this session nad is
  // not full initialized yet" flag.
  void SetIsCurrentUserNew(bool is_new);

  // Helper function that converts users from |users_list| to |users_vector| and
  // |users_set|. Duplicates and users already present in |existing_users| are
  // skipped.
  void ParseUserList(const base::Value::List& users_list,
                     const std::set<AccountId>& existing_users,
                     std::vector<AccountId>* users_vector,
                     std::set<AccountId>* users_set);

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

  // Performs any additional actions after UserLoggedIn() execution has been
  // completed.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  virtual void PerformPostUserLoggedInActions(bool browser_restart) = 0;

  // Implementation for RemoveUser method. It is synchronous. It is called from
  // RemoveUserInternal after owner check.
  // Pass |account_id| by value here to avoid use-after-free. Original
  // |account_id| could be destroyed during the user removal.
  virtual void RemoveNonOwnerUserInternal(AccountId account_id,
                                          UserRemovalReason reason);

  // Removes a regular or supervised user from the user list.
  // Returns the user if found or NULL otherwise.
  // Also removes the user from the persistent user list.
  // |notify| is true when OnUserRemoved() should be triggered,
  // meaning that the user won't be added after the removal.
  User* RemoveRegularOrSupervisedUserFromList(const AccountId& account_id,
                                              bool notify);

  // Implementation for RemoveUser. If |reason| is set, it notifies observers
  // via OnUserToBeRemoved, OnUserRemoved and LocalStateChanged.
  // If |trigger_cryptohome_removal| is set to true, this triggeres an
  // asynchronous operation to remove the user data in Cryptohome.
  void RemoveUserFromListImpl(const AccountId& account_id,
                              absl::optional<UserRemovalReason> reason,
                              bool trigger_cryptohome_removal);

  // Implementation for RemoveUser method. This is an asynchronous part of the
  // method, that verifies that owner will not get deleted, and calls
  // |RemoveNonOwnerUserInternal|.
  virtual void RemoveUserInternal(const AccountId& account_id,
                                  UserRemovalReason reason);

  // Removes data stored or cached outside the user's cryptohome (wallpaper,
  // avatar, OAuth token status, display name, display email).
  virtual void RemoveNonCryptohomeData(const AccountId& account_id);

  // Check for a particular user type.

  // These methods are called when corresponding user type has signed in.

  // Indicates that a user just logged in as guest.
  virtual void GuestUserLoggedIn();

  // Indicates that a kiosk app robot just logged in.
  virtual void KioskAppLoggedIn(User* user) = 0;

  // Indicates that a user just logged into a public session.
  virtual void PublicAccountUserLoggedIn(User* user) = 0;

  // Indicates that a regular user just logged in.
  virtual void RegularUserLoggedIn(const AccountId& account_id,
                                   const UserType user_type);

  // Indicates that a regular user just logged in as ephemeral.
  virtual void RegularUserLoggedInAsEphemeral(const AccountId& account_id,
                                              const UserType user_type);

  // Update the global LoginState.
  virtual void UpdateLoginState(const User* active_user,
                                const User* primary_user,
                                bool is_current_user_owner) const = 0;

  virtual bool IsEphemeralAccountIdByPolicy(
      const AccountId& account_id) const = 0;

  // Getters/setters for private members.

  const EphemeralModeConfig& GetEphemeralModeConfig() const;
  virtual void SetEphemeralModeConfig(
      EphemeralModeConfig ephemeral_mode_config);

  virtual void ResetOwnerId();
  virtual void SetOwnerId(const AccountId& owner_account_id);

  virtual const AccountId& GetPendingUserSwitchID() const;
  virtual void SetPendingUserSwitchId(const AccountId& account_id);

  base::ObserverList<UserManager::Observer>::Unchecked observer_list_;

  // The logged-in user that is currently active in current session.
  // NULL until a user has logged in, then points to one
  // of the User instances in |users_|, the |guest_user_| instance or an
  // ephemeral user instance.
  raw_ptr<User, DanglingUntriaged> active_user_ = nullptr;

  // The primary user of the current session. It is recorded for the first
  // signed-in user and does not change thereafter.
  raw_ptr<User, DanglingUntriaged> primary_user_ = nullptr;

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

  void RemoveLegacySupervisedUser(const AccountId& account_id);

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

  // Cached `EphemeralModeConfig` created from trusted device policies.
  //
  // If the value has not been read from trusted device policy yet, then all
  // users considered as non-ephemeral.
  EphemeralModeConfig ephemeral_mode_config_;

  // Cached name of device owner. Defaults to empty if the value has not
  // been read from trusted device policy yet.
  absl::optional<AccountId> owner_account_id_ = absl::nullopt;

  mutable base::OnceCallbackList<void(const AccountId&)>
      pending_owner_callbacks_;

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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const raw_ptr<PrefService, DanglingUntriaged> local_state_;

  base::WeakPtrFactory<UserManagerBase> weak_factory_{this};
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_BASE_H_
