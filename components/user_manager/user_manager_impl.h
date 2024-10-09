// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_IMPL_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

class PrefRegistrySimple;

namespace ash {
class CrosSettings;
class UserManagerTest;
}  // namespace ash

namespace user_manager {

// Feature that removes legacy supervised users.
BASE_DECLARE_FEATURE(kRemoveLegacySupervisedUsersOnStartup);

// Feature that removes deprecated ARC kiosk users.
USER_MANAGER_EXPORT
BASE_DECLARE_FEATURE(kRemoveDeprecatedArcKioskUsersOnStartup);

// Base implementation of the UserManager interface.
class USER_MANAGER_EXPORT UserManagerImpl : public UserManager {
 public:
  // These enum values represent a legacy supervised user's (LSU) status on the
  // sign in screen.
  // TODO(crbug.com/40735554): Remove once all LSUs deleted in the wild. LSUs
  // were first hidden on the login screen in M74. Assuming a five year AUE, we
  // should stop supporting devices with LSUs by 2024.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "LegacySupervisedUserStatus" in
  // src/tools/metrics/histograms/metadata/families/enums.xml.
  enum class LegacySupervisedUserStatus {
    // Non-LSU Gaia user displayed on login screen.
    kGaiaUserDisplayed = 0,
    // LSU hidden on login screen. Expect this count to decline to zero over
    // time as we delete LSUs.
    kLSUHidden = 1,
    // LSU attempted to delete cryptohome. Expect this count to decline to zero
    // over time as we delete LSUs.
    kLSUDeleted = 2,
    // Add future entries above this comment, in sync with
    // "LegacySupervisedUserStatus" in
    // src/tools/metrics/histograms/metadata/families/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kLSUDeleted
  };

  // These enum values represent a deprecated ARC kiosk user's status on the
  // sign in screen.
  // TODO(b/355590943): Remove once all ARC kiosk users are deleted in the wild.
  // ARC Kiosk has been deprecated and removed in m126. However, the accounts
  // still exist on the devices if configured prior to m126, but hidden. These
  // values are logged to UMA. Entries should not be renumbered and numeric
  // values should never be reused. Please keep in sync with
  // "DeprecatedArcKioskUserStatus" in src/tools/metrics/histograms/enums.xml.
  enum class DeprecatedArcKioskUserStatus {
    // ARC kiosk hidden on login screen. Expect this count to decline to zero
    // over
    // time.
    kHidden = 0,
    // Attempted to delete cryptohome. Expect this count to decline to zero
    // over time.
    kDeleted = 1,
    kMaxValue = kDeleted
  };

  // Delegate interface to inject //chrome/* dependency.
  // In case you need to extend this, please consider to minimize the
  // responsibility, because it means to depend more things on //chrome/*
  // browser from ash-system, which we prefer minimizing.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the application locale.
    virtual const std::string& GetApplicationLocale() = 0;

    // Overrides the home directory path for the `primary_user`.
    virtual void OverrideDirHome(const User& primary_user) = 0;

    // Returns whether user session restore is in progress.
    virtual bool IsUserSessionRestoreInProgress() = 0;

    // Returns UserType for the DeviceLocalAccount of the given `email`.
    virtual std::optional<UserType> GetDeviceLocalAccountUserType(
        std::string_view email) = 0;

    // Verifies the Profile's state for the given `user` on login.
    virtual void CheckProfileOnLogin(const User& user) = 0;

    // Removes the Profile tied to the `account_id`.
    virtual void RemoveProfileByAccountId(const AccountId& account_id) = 0;

    // Triggers to remove cryptohome for the user identified by `account_id`
    virtual void RemoveCryptohomeAsync(const AccountId& account_id) = 0;
  };

  // Creates UserManagerImpl on UI thread with given `local_state`.
  // `local_state` must outlive this UserManager.
  UserManagerImpl(std::unique_ptr<Delegate> delegate,
                  PrefService* local_state,
                  ash::CrosSettings* cros_settings);

  UserManagerImpl(const UserManagerImpl&) = delete;
  UserManagerImpl& operator=(const UserManagerImpl&) = delete;

  ~UserManagerImpl() override;

  // Histogram for tracking the number of deprecated legacy supervised user
  // cryptohomes remaining in the wild.
  static const char kLegacySupervisedUsersHistogramName[];

  // Histogram for tracking the number of deprecated ARC kiosk user
  // cryptohomes remaining in the wild.
  // TODO(b/355590943): clean up once there is no ARC kiosk records.
  static const char kDeprecatedArcKioskUsersHistogramName[];

  // Registers UserManagerImpl preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // UserManager implementation:
  void Shutdown() override;
  const UserList& GetUsers() const override;
  UserList GetUsersAllowedForMultiProfile() const override;
  UserList FindLoginAllowedUsersFrom(const UserList& users) const final;
  const UserList& GetLoggedInUsers() const override;
  const UserList& GetLRULoggedInUsers() const override;
  UserList GetUnlockUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void GetOwnerAccountIdAsync(
      base::OnceCallback<void(const AccountId&)> callback) const override;

  const AccountId& GetLastSessionActiveAccountId() const override;
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart,
                    bool is_child) override;
  bool OnUserProfileCreated(const AccountId& account_id,
                            PrefService* prefs) override;
  void OnUserProfileWillBeDestroyed(const AccountId& account_id) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SwitchToLastActiveUser() override;
  void OnSessionStarted() override;
  bool UpdateDeviceLocalAccountUser(
      const base::span<DeviceLocalAccountInfo>& device_local_accounts) override;
  void RemoveUser(const AccountId& account_id,
                  UserRemovalReason reason) override;
  void RemoveUserFromList(const AccountId& account_id) override;
  void RemoveUserFromListForRecreation(const AccountId& account_id) override;
  bool RemoveStaleEphemeralUsers() override;
  void CleanStaleUserInformationFor(const AccountId& account_id) override;
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
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override;
  UserType GetUserType(const AccountId& account_id) override;
  void SaveUserType(const User* user) override;
  void SetUserUsingSaml(const AccountId& account_id,
                        bool using_saml,
                        bool using_saml_principals_api) override;
  std::optional<std::string> GetOwnerEmail() override;
  void RecordOwner(const AccountId& owner) override;
  void UpdateUserAccountData(const AccountId& account_id,
                             const UserAccountData& account_data) override;
  bool IsOwnerUser(const User* user) const override;
  bool IsPrimaryUser(const User* user) const override;
  bool IsEphemeralUser(const User* user) const override;
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const final;
  void SetIsCurrentUserNew(bool is_new) override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool IsCurrentUserCryptohomeDataEphemeral() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsChildUser() const override;
  bool IsLoggedInAsManagedGuestSession() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsKioskIWA() const override;
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
  void NotifyUserNotAllowed(const std::string& user_email) final;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const User& user) const override;
  bool IsUserAllowed(const User& user) const override;
  PrefService* GetLocalState() const final;
  bool IsFirstExecAfterBoot() const final;
  bool IsDeprecatedSupervisedAccountId(
      const AccountId& account_id) const override;
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void SetUserAffiliated(const AccountId& account_id,
                         bool is_affiliated) override;
  bool HasBrowserRestarted() const final;
  MultiUserSignInPolicyController* GetMultiUserSignInPolicyController()
      override;

  void Initialize() override;

  // Creates and adds a kiosk user for testing with a given `account_id`
  // and `username_hash` to identify homedir mount point.
  // Returns a pointer to the user.
  // Note: call `UserLoggedIn` if the user needs to be logged-in.
  const User* AddKioskAppUserForTesting(const AccountId& account_id,
                                        const std::string& username_hash);

  // Helper function that converts users from |users_list| to |users_vector| and
  // |users_set|. Duplicates and users already present in |existing_users| are
  // skipped.
  void ParseUserList(const base::Value::List& users_list,
                     const std::set<AccountId>& existing_users,
                     std::vector<AccountId>* users_vector,
                     std::set<AccountId>* users_set);

 protected:
  friend class ash::UserManagerTest;

  ash::CrosSettings* cros_settings() { return cros_settings_; }
  const ash::CrosSettings* cros_settings() const { return cros_settings_; }

  // Adds |user| to users list, and adds it to front of LRU list. It is assumed
  // that there is no user with same id.
  virtual void AddUserRecord(User* user);

  // Returns true if user may be removed.
  virtual bool CanUserBeRemoved(const User* user) const;

  // A wrapper around C++ delete operator. Deletes |user|, and when |user|
  // equals to active_user_, active_user_ is reset to NULL.
  virtual void DeleteUser(User* user);

  // Loads device local accounts from the Local state and fills in
  // |device_local_accounts_set|.
  void LoadDeviceLocalAccounts(std::set<AccountId>* device_local_accounts_set);

  // If data for a device local account is marked as pending removal and the
  // user is no longer logged into that account, removes the data.
  void RemovePendingDeviceLocalAccount();

  // Notifies observers that active user has changed.
  void NotifyActiveUserChanged(User* active_user);

  // Notifies observers that login state is changed.
  void NotifyLoginStateUpdated();

  // Notifies that user has logged in.
  virtual void NotifyOnLogin();

  // Notifies observers that another user was added to the session.
  void NotifyUserAddedToSession(const User* added_user);

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
  void RemoveUserFromListImpl(AccountId account_id,
                              std::optional<UserRemovalReason> reason,
                              bool trigger_cryptohome_removal);

  // Implementation for RemoveUser method. This is an asynchronous part of the
  // method, that verifies that owner will not get deleted, and calls
  // |RemoveNonOwnerUserInternal|.
  void RemoveUserInternal(const AccountId& account_id,
                          UserRemovalReason reason);

  // Removes data stored or cached outside the user's cryptohome (wallpaper,
  // avatar, OAuth token status, display name, display email).
  virtual void RemoveNonCryptohomeData(const AccountId& account_id);

  // Getters/setters for private members.

  const EphemeralModeConfig& GetEphemeralModeConfig() const;
  void SetEphemeralModeConfig(
      EphemeralModeConfig ephemeral_mode_config) override;

  virtual void ResetOwnerId();
  void SetOwnerId(const AccountId& owner_account_id) override;

  // If there's pending user switch, processes it.
  void ProcessPendingUserSwitchId();

  // TODO(b/278643115): Move to private, once we migrate fake implementation
  // closer enough to the production behavior.
  void RegularUserLoggedInAsEphemeral(const AccountId& account_id,
                                      const UserType user_type);

  base::ObserverList<UserManager::Observer>::Unchecked observer_list_;

  // A list of User instances taking their ownership.
  // Following members can refer User instances in this vector.
  // Thus, they must be listed below to deal with raw_ptr rule.
  std::vector<std::unique_ptr<User>> user_storage_;

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
  // Loads |users_| from Local State if the list has not been loaded yet.
  // Subsequent calls have no effect. Must be called on the UI thread.
  void EnsureUsersLoaded();

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

  // Processes log-in for each type of users.
  void RegularUserLoggedIn(const AccountId& account_id,
                           const UserType user_type);
  void GuestUserLoggedIn();
  void PublicAccountUserLoggedIn(User* user);
  void KioskAppLoggedIn(User* user);

  // Insert |user| at the front of the LRU user list.
  void SetLRUUser(User* user);

  // Updates num-users crash key.
  void UpdateCrashKey(int num_users, std::optional<UserType> active_user_type);

  // Sends metrics in response to a user with gaia account (regular) logging in.
  void SendGaiaUserLoginMetrics(const AccountId& account_id);

  // Sends metrics for multi user sign-in.
  void SendMultiUserSignInMetrics();

  // Sets account locale for user with id |account_id|.
  virtual void UpdateUserAccountLocale(const AccountId& account_id,
                                       const std::string& locale);

  // Updates user account after locale was resolved.
  void DoUpdateAccountLocale(const AccountId& account_id,
                             const std::string& resolved_locale);

  void RemoveLegacySupervisedUser(const AccountId& account_id);

  // Returns true if |account_id| is a deprecated ARC kiosk account.
  // TODO(b/355590943): Check if it is not used anymore and remove it.
  bool IsDeprecatedArcKioskAccountId(const AccountId& account_id) const;
  void RemoveDeprecatedArcKioskUser(const AccountId& account_id);

  std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<PrefService, DanglingUntriaged> local_state_;

  // Interface to the signed settings store.
  const raw_ptr<ash::CrosSettings> cros_settings_;

  // Handles multi-user sign-in policy.
  MultiUserSignInPolicyController multi_user_sign_in_policy_controller_;

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
  std::optional<AccountId> owner_account_id_ = std::nullopt;

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

  base::WeakPtrFactory<UserManagerImpl> weak_factory_{this};
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_IMPL_H_
