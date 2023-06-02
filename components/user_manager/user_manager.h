// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/scoped_observation_traits.h"
#include "components/user_manager/include_exclude_account_id_filter.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

class AccountId;
class PrefService;

namespace user_manager {

class ScopedUserManager;

// A list pref of the the regular users known on this device, arranged in LRU
// order, stored in local state.
USER_MANAGER_EXPORT extern const char kRegularUsersPref[];

enum class UserRemovalReason : int32_t {
  UNKNOWN = 0,
  LOCAL_USER_INITIATED = 1,
  REMOTE_ADMIN_INITIATED = 2,
  LOCAL_USER_INITIATED_ON_REQUIRED_UPDATE = 3,
  DEVICE_EPHEMERAL_USERS_ENABLED = 4,
  GAIA_REMOVED = 5,
  MISCONFIGURED_USER = 6,
};

// Interface for UserManagerBase - that provides base implementation for
// Chrome OS user management. Typical features:
// * Get list of all know users (who have logged into this Chrome OS device)
// * Keep track for logged in/LRU users, active user in multi-user session.
// * Find/modify users, store user meta-data such as display name/email.
class USER_MANAGER_EXPORT UserManager {
 public:
  using EphemeralModeConfig = IncludeExcludeAccountIdFilter;

  // Interface that observers of UserManager must implement in order
  // to receive notification when local state preferences is changed
  class Observer {
   public:
    // Called when the local state preferences is changed.
    virtual void LocalStateChanged(UserManager* user_manager);

    // Called when the user list is loaded.
    virtual void OnUserListLoaded();

    // Called when the device local user list is updated.
    virtual void OnDeviceLocalUserListUpdated();

    // Called when the user is logged in.
    virtual void OnUserLoggedIn(const User& user);

    // Called when the image of the given user is changed.
    virtual void OnUserImageChanged(const User& user);

    // Called when the user image enterprise state of the given user is changed.
    virtual void OnUserImageIsEnterpriseManagedChanged(
        const User& user,
        bool is_enterprise_managed);

    // Called when the Profile instance for the user is created.
    virtual void OnUserProfileCreated(const User& user);

    // Called when the profile image download for the given user fails or
    // user has the default profile image or no porfile image at all.
    virtual void OnUserProfileImageUpdateFailed(const User& user);

    // Called when the profile image for the given user is downloaded.
    // |profile_image| contains the downloaded profile image.
    virtual void OnUserProfileImageUpdated(const User& user,
                                           const gfx::ImageSkia& profile_image);

    // Called when any of the device cros settings which are responsible for
    // user sign in are changed.
    virtual void OnUsersSignInConstraintsChanged();

    // Called when the user affiliation is updated.
    virtual void OnUserAffiliationUpdated(const User& user);

    // Called just before a user of the device will be removed.
    virtual void OnUserToBeRemoved(const AccountId& account_id);

    // Called just after a user of the device has been removed.
    virtual void OnUserRemoved(const AccountId& account_id,
                               UserRemovalReason reason);

   protected:
    virtual ~Observer();
  };

  // TODO(xiyuan): Refactor and move this observer out of UserManager.
  // Observer interface that defines methods used to notify on user session /
  // active user state changes. Default implementation is empty.
  class UserSessionStateObserver {
   public:
    // Called when active user has changed.
    virtual void ActiveUserChanged(User* active_user);

    // Called when another user got added to the existing session.
    virtual void UserAddedToSession(const User* added_user);

   protected:
    virtual ~UserSessionStateObserver();
  };

  // Data retrieved from user account.
  class UserAccountData {
   public:
    UserAccountData(const std::u16string& display_name,
                    const std::u16string& given_name,
                    const std::string& locale);

    UserAccountData(const UserAccountData&) = delete;
    UserAccountData& operator=(const UserAccountData&) = delete;

    ~UserAccountData();
    const std::u16string& display_name() const { return display_name_; }
    const std::u16string& given_name() const { return given_name_; }
    const std::string& locale() const { return locale_; }

   private:
    const std::u16string display_name_;
    const std::u16string given_name_;
    const std::string locale_;
  };

  // Initializes UserManager instance to this. Normally should be called right
  // after creation so that user_manager::UserManager::Get() doesn't fail.
  // Tests could call this method if they are replacing existing UserManager
  // instance with their own test instance.
  virtual void Initialize();

  // Checks whether the UserManager instance has been created already.
  // This method is not thread-safe and must be called from the main UI thread.
  static bool IsInitialized();

  // Shuts down the UserManager. After this method has been called, the
  // singleton has unregistered itself as an observer but remains available so
  // that other classes can access it during their shutdown. This method is not
  // thread-safe and must be called from the main UI thread.
  virtual void Shutdown() = 0;

  // Sets UserManager instance to NULL. Always call Shutdown() first.
  // This method is not thread-safe and must be called from the main UI thread.
  void Destroy();

  // Returns UserManager instance or will crash if it is |NULL| (has either not
  // been created yet or is already destroyed). This method is not thread-safe
  // and must be called from the main UI thread.
  static UserManager* Get();

  virtual ~UserManager();

  // Returns a list of users who have logged into this device previously. This
  // is sorted by last login date with the most recent user at the beginning.
  virtual const UserList& GetUsers() const = 0;

  // Returns list of users allowed for logging in into multi-profile session.
  // Users that have a policy that prevents them from being added to the
  // multi-profile session will still be part of this list as long as they
  // are regular users (i.e. not a public session/supervised etc.).
  // Returns an empty list in case when primary user is not a regular one or
  // has a policy that prohibits it to be part of multi-profile session.
  virtual UserList GetUsersAllowedForMultiProfile() const = 0;

  // Returns a list of users who are currently logged in.
  virtual const UserList& GetLoggedInUsers() const = 0;

  // Returns a list of users who are currently logged in in the LRU order -
  // so the active user is the first one in the list. If there is no user logged
  // in, the current user will be returned.
  virtual const UserList& GetLRULoggedInUsers() const = 0;

  // Returns a list of users who can unlock the device.
  // This list is based on policy and whether user is able to do unlock.
  // Policy:
  // * If user has primary-only policy then it is the only user in unlock users.
  // * Otherwise all users with unrestricted policy are added to this list.
  // All users that are unable to perform unlock are excluded from this list.
  virtual UserList GetUnlockUsers() const = 0;

  // Returns account Id of the owner user. Returns an empty Id if there is
  // no owner for the device.
  virtual const AccountId& GetOwnerAccountId() const = 0;

  // Provides the caller with account Id of the Owner user once it is loaded.
  // Would provide empty account id if there is no owner on the device (e.g.
  // if device is enterprise-owned).
  virtual void GetOwnerAccountIdAsync(
      base::OnceCallback<void(const AccountId&)> callback) const = 0;

  // Returns account Id of the user that was active in the previous session.
  virtual const AccountId& GetLastSessionActiveAccountId() const = 0;

  // Indicates that a user with the given |account_id| has just logged in. The
  // persistent list is updated accordingly if the user is not ephemeral.
  // |browser_restart| is true when reloading Chrome after crash to distinguish
  // from normal sign in flow.
  // |username_hash| is used to identify homedir mount point.
  virtual void UserLoggedIn(const AccountId& account_id,
                            const std::string& username_hash,
                            bool browser_restart,
                            bool is_child) = 0;

  // Switches to active user identified by |account_id|. User has to be logged
  // in.
  virtual void SwitchActiveUser(const AccountId& account_id) = 0;

  // Switches to the last active user (called after crash happens and session
  // restore has completed).
  virtual void SwitchToLastActiveUser() = 0;

  // Invoked by session manager to inform session start.
  virtual void OnSessionStarted() = 0;

  // Removes the user from the device while providing a reason for enterprise
  // reporting. Note, it will verify that the given user isn't the owner, so
  // calling this method for the owner will take no effect.
  // This removes the user from the list synchronously, so the following
  // function calls should have updated users. However, actual deletion of
  // a user from a device has more tasks to complete, such as deletion of
  // cryptohome data, which are asynchronous operations. Currently, there's
  // no support to observe the completion of such tasks.
  virtual void RemoveUser(const AccountId& account_id,
                          UserRemovalReason reason) = 0;

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const AccountId& account_id) = 0;

  // Removes the user from persistent list, without triggering user removal
  // notification.
  // Used to re-create user in Password changed flow when user can not
  // remember old password and decides to delete existing user directory and
  // re-create it.
  // TODO(b/270040728): Remove this method once internal architecture allows
  // better solution.
  virtual void RemoveUserFromListForRecreation(const AccountId& account_id) = 0;

  // Returns true if a user with the given account id is found in the persistent
  // list or currently logged in as ephemeral.
  virtual bool IsKnownUser(const AccountId& account_id) const = 0;

  // Returns the user with the given account id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  virtual const User* FindUser(const AccountId& account_id) const = 0;

  // Returns the user with the given account id if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  // Same as FindUser but returns non-const pointer to User object.
  virtual User* FindUserAndModify(const AccountId& account_id) = 0;

  // Returns the logged-in user that is currently active within this session.
  // There could be multiple users logged in at the the same but for now
  // we support only one of them being active.
  virtual const User* GetActiveUser() const = 0;
  virtual User* GetActiveUser() = 0;

  // Returns the primary user of the current session. It is recorded for the
  // first signed-in user and does not change thereafter.
  virtual const User* GetPrimaryUser() const = 0;

  // Saves user's oauth token status in local state preferences.
  virtual void SaveUserOAuthStatus(
      const AccountId& account_id,
      User::OAuthTokenStatus oauth_token_status) = 0;

  // Saves a flag indicating whether online authentication against GAIA should
  // be enforced during the user's next sign-in.
  virtual void SaveForceOnlineSignin(const AccountId& account_id,
                                     bool force_online_signin) = 0;

  // Saves user's displayed name in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayName(const AccountId& account_id,
                                   const std::u16string& display_name) = 0;

  // Updates data upon User Account download.
  virtual void UpdateUserAccountData(const AccountId& account_id,
                                     const UserAccountData& account_data) = 0;

  // Returns the display name for user |account_id| if it is known (was
  // previously set by a |SaveUserDisplayName| call).
  // Otherwise, returns an empty string.
  virtual std::u16string GetUserDisplayName(
      const AccountId& account_id) const = 0;

  // Saves user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayEmail(const AccountId& account_id,
                                    const std::string& display_email) = 0;

  // Returns stored user type or USER_TYPE_REGULAR by default.
  virtual UserType GetUserType(const AccountId& account_id) = 0;

  // Saves user's type for |user| into local state preferences.
  virtual void SaveUserType(const User* user) = 0;

  // Returns the email of the owner user stored in local state. Can return
  // nullopt if no user attempted to take ownership so far (e.g. there were
  // only guest sessions or it's a managed device). This is a secondary / backup
  // mechanism to determine the owner user, prefer relying on device policies or
  // possession of the private key when possible.
  virtual absl::optional<std::string> GetOwnerEmail() = 0;

  // Records the identity of the owner user. In the current implementation
  // always stores the email.
  virtual void RecordOwner(const AccountId& owner) = 0;

  // Returns true if the given |user| is the device owner.
  virtual bool IsOwnerUser(const User* user) const = 0;

  // Returns true if the given |user| is the primary user.
  virtual bool IsPrimaryUser(const User* user) const = 0;

  // Returns true if the given |user| is an ephemeral user.
  virtual bool IsEphemeralUser(const User* user) const = 0;

  // Returns true if current user is an owner.
  virtual bool IsCurrentUserOwner() const = 0;

  // Returns true if current user is not existing one (hasn't signed in before).
  virtual bool IsCurrentUserNew() const = 0;

  // Returns true if data stored or cached for the current user outside that
  // user's cryptohome (wallpaper, avatar, OAuth token status, display name,
  // display email) is ephemeral.
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const = 0;

  // Returns true if data stored or cached for the current user inside that
  // user's cryptohome is ephemeral.
  virtual bool IsCurrentUserCryptohomeDataEphemeral() const = 0;

  // Returns true if the current user's session can be locked (i.e. the user has
  // a password with which to unlock the session).
  virtual bool CanCurrentUserLock() const = 0;

  // Returns true if at least one user has signed in.
  virtual bool IsUserLoggedIn() const = 0;

  // Returns true if we're logged in as a user with gaia account.
  virtual bool IsLoggedInAsUserWithGaiaAccount() const = 0;

  // Returns true if we're logged in as a child user.
  virtual bool IsLoggedInAsChildUser() const = 0;

  // Returns true if we're logged in as a public account.
  virtual bool IsLoggedInAsPublicAccount() const = 0;

  // Returns true if we're logged in as a Guest.
  virtual bool IsLoggedInAsGuest() const = 0;

  // Returns true if we're logged in as a kiosk app.
  virtual bool IsLoggedInAsKioskApp() const = 0;

  // Returns true if we're logged in as an ARC kiosk app.
  virtual bool IsLoggedInAsArcKioskApp() const = 0;

  // Returns true if we're logged in as a Web kiosk app.
  virtual bool IsLoggedInAsWebKioskApp() const = 0;

  // Returns true if we're logged in as chrome, ARC or Web kiosk app.
  virtual bool IsLoggedInAsAnyKioskApp() const = 0;

  // Returns true if we're logged in as the stub user used for testing on Linux.
  virtual bool IsLoggedInAsStub() const = 0;

  // Returns true if data stored or cached for the user with the given
  // |account_id|
  // address outside that user's cryptohome (wallpaper, avatar, OAuth token
  // status, display name, display email) is to be treated as ephemeral.
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const = 0;

  virtual bool IsUserCryptohomeDataEphemeral(
      const AccountId& account_id) const = 0;

  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void AddSessionStateObserver(UserSessionStateObserver* obs) = 0;
  virtual void RemoveSessionStateObserver(UserSessionStateObserver* obs) = 0;

  virtual void NotifyLocalStateChanged() = 0;
  virtual void NotifyUserImageChanged(const User& user) = 0;
  virtual void NotifyUserImageIsEnterpriseManagedChanged(
      const User& user,
      bool is_enterprise_managed) = 0;
  virtual void NotifyUserProfileImageUpdateFailed(const User& user) = 0;
  virtual void NotifyUserProfileImageUpdated(
      const User& user,
      const gfx::ImageSkia& profile_image) = 0;
  virtual void NotifyUsersSignInConstraintsChanged() = 0;
  virtual void NotifyUserAffiliationUpdated(const User& user) = 0;
  virtual void NotifyUserToBeRemoved(const AccountId& account_id) = 0;
  virtual void NotifyUserRemoved(const AccountId& account_id,
                                 UserRemovalReason reason) = 0;

  // Returns true if guest user is allowed.
  virtual bool IsGuestSessionAllowed() const = 0;

  // Returns true if the |user|, which has a GAIA account is allowed according
  // to device settings and policies.
  // Accept only users who has gaia account.
  virtual bool IsGaiaUserAllowed(const User& user) const = 0;

  // Returns true if |user| is allowed depending on device policies.
  // Accepted user types: USER_TYPE_REGULAR, USER_TYPE_GUEST, USER_TYPE_CHILD.
  virtual bool IsUserAllowed(const User& user) const = 0;

  // Explicitly non-ephemeral accounts are Owner account (on consumer-owned
  // devices) and Stub accounts (used in tests).
  //
  // Explicitly ephemeral accounts are Guest and Managed Guest sessions.
  //
  // In all other cases the ephemeral status of account depends on set of
  // policies.
  virtual bool IsEphemeralAccountId(const AccountId& account_id) const = 0;

  // Returns "Local State" PrefService instance.
  virtual PrefService* GetLocalState() const = 0;

  // Checks for platform-specific known users matching given |user_email|. If
  // data matches a known account, fills |out_account_id| with account id and
  // returns true.
  virtual bool GetPlatformKnownUserId(const std::string& user_email,
                                      AccountId* out_account_id) const = 0;

  // Returns account id of the Guest user.
  virtual const AccountId& GetGuestAccountId() const = 0;

  // Returns true if this is first exec after boot.
  virtual bool IsFirstExecAfterBoot() const = 0;

  // Actually removes cryptohome.
  virtual void AsyncRemoveCryptohome(const AccountId& account_id) const = 0;

  // Returns true if |account_id| is Guest user.
  virtual bool IsGuestAccountId(const AccountId& account_id) const = 0;

  // Returns true if |account_id| is Stub user.
  virtual bool IsStubAccountId(const AccountId& account_id) const = 0;

  // Returns true if |account_id| is deprecated supervised.
  // TODO(crbug/1155729): Check it is not used anymore and remove it.
  virtual bool IsDeprecatedSupervisedAccountId(
      const AccountId& account_id) const = 0;

  virtual bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const = 0;

  // Returns true when the browser has crashed and restarted during the current
  // user's session.
  virtual bool HasBrowserRestarted() const = 0;

  // Returns image from resources bundle.
  virtual const gfx::ImageSkia& GetResourceImagekiaNamed(int id) const = 0;

  // Returns string from resources bundle.
  virtual std::u16string GetResourceStringUTF16(int string_id) const = 0;

  // Schedules CheckAndResolveLocale using given task runner and
  // |on_resolved_callback| as reply callback.
  virtual void ScheduleResolveLocale(
      const std::string& locale,
      base::OnceClosure on_resolved_callback,
      std::string* out_resolved_locale) const = 0;

  // Returns true if |image_index| is a valid default user image index.
  virtual bool IsValidDefaultUserImageId(int image_index) const = 0;

  UserType CalculateUserType(const AccountId& account_id,
                             const User* user,
                             bool browser_restart,
                             bool is_child) const;

 protected:
  // Sets UserManager instance.
  static void SetInstance(UserManager* user_manager);

  // Pointer to the existing UserManager instance (if any).
  // Usually is set by calling Initialize(), reset by calling Destroy().
  // Not owned since specific implementation of UserManager should decide on its
  // own appropriate owner. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static UserManager* instance;

 private:
  friend class ScopedUserManager;

  // Same as Get() but doesn't won't crash is current instance is NULL.
  static UserManager* GetForTesting();

  // Sets UserManager instance to the given |user_manager|.
  // Returns the previous value of the instance.
  static UserManager* SetForTesting(UserManager* user_manager);
};

}  // namespace user_manager

namespace base {

template <>
struct ScopedObservationTraits<
    user_manager::UserManager,
    user_manager::UserManager::UserSessionStateObserver> {
  static void AddObserver(
      user_manager::UserManager* source,
      user_manager::UserManager::UserSessionStateObserver* observer) {
    source->AddSessionStateObserver(observer);
  }
  static void RemoveObserver(
      user_manager::UserManager* source,
      user_manager::UserManager::UserSessionStateObserver* observer) {
    source->RemoveSessionStateObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_H_
