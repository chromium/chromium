// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/user_login_permission_tracker.h"
#include "components/crash/core/common/crash_key.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_manager_policy_util.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {
namespace {

// Upper bound for a histogram metric reporting the amount of time between
// one regular user logging out and a different regular user logging in.
const int kLogoutToLoginDelayMaxSec = 1800;

// Used for serializing information about the owner user. The existing entries
// should never be deleted / renumbered.
enum class OwnerAccountType { kGoogleEmail = 1 };

// This reads integer value from kUserType Local State preference and
// interprets it as UserType. It is used in initial users load.
UserType GetStoredUserType(const base::Value::Dict& prefs_user_types,
                           const AccountId& account_id) {
  const base::Value* stored_user_type = prefs_user_types.Find(
      account_id.HasAccountIdKey() ? account_id.GetAccountIdKey()
                                   : account_id.GetUserEmail());
  if (!stored_user_type || !stored_user_type->is_int()) {
    return UserType::kRegular;
  }

  int int_user_type = stored_user_type->GetInt();
  if (int_user_type < 0 ||
      int_user_type > static_cast<int>(UserType::kMaxValue) ||
      int_user_type == 2) {
    LOG(ERROR) << "Bad user type " << int_user_type;
    return UserType::kRegular;
  }
  return static_cast<UserType>(int_user_type);
}

bool IsDeviceLocalAccountChanged(
    const UserList& users,
    const base::span<UserManager::DeviceLocalAccountInfo>&
        device_local_accounts) {
  size_t i = 0;
  for (const user_manager::User* user : users) {
    if (!user->IsDeviceLocalAccount()) {
      continue;
    }
    if (i >= device_local_accounts.size()) {
      // `users` has device local users more than the updated one.
      return true;
    }
    if (user->GetAccountId().GetUserEmail() !=
            device_local_accounts[i].user_id ||
        user->GetType() != device_local_accounts[i].type) {
      // The device local user at the position is different from the new
      // corresponding entry.
      return true;
    }
    ++i;
  }

  // If there're still new entries, the `users` needs to be updated.
  return i != device_local_accounts.size();
}

}  // namespace

// static
const char UserManagerImpl::kLegacySupervisedUsersHistogramName[] =
    "ChromeOS.LegacySupervisedUsers.HiddenFromLoginScreen";

// static
const char UserManagerImpl::kDeprecatedArcKioskUsersHistogramName[] =
    "Kiosk.DeprecatedArcKioskUsers";
// static
BASE_FEATURE(kRemoveDeprecatedArcKioskUsersOnStartup,
             "RemoveDeprecatedArcKioskUsersOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

UserManagerImpl::UserManagerImpl(std::unique_ptr<Delegate> delegate,
                                 PrefService* local_state)
    : delegate_(std::move(delegate)), local_state_(local_state) {
  // |local_state| can be nullptr only for testing.
  if (!local_state) {
    CHECK_IS_TEST();
  }
  UpdateNumLoggedInUsersCrashKey(0);
}

UserManagerImpl::UserManagerImpl(std::unique_ptr<Delegate> delegate,
                                 PrefService* local_state,
                                 ash::CrosSettings* /*unused*/)
    : UserManagerImpl(std::move(delegate), local_state) {}

UserManagerImpl::~UserManagerImpl() = default;

void UserManagerImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const UserList& UserManagerImpl::GetPersistedUsers() const {
  return persisted_users_;
}

UserList UserManagerImpl::GetUsersAllowedForMultiUserSignIn() const {
  // Supervised users are not allowed to use multi-user sign-in.
  if (logged_in_users_.size() == 1 &&
      primary_user_->GetType() != UserType::kRegular) {
    return {};
  }

  // No user is allowed if the primary user policy forbids it.
  if (GetMultiUserSignInPolicy(primary_user_) ==
      MultiUserSignInPolicy::kNotAllowed) {
    return {};
  }

  user_manager::UserList result;
  for (user_manager::User* user : persisted_users_) {
    if (user->GetType() == UserType::kRegular && !user->is_logged_in()) {
      // Users with a policy that prevents them being added to a session will be
      // shown in login UI but will be grayed out.
      // Same applies to owner account (see http://crbug.com/385034).
      result.push_back(user);
    }
  }

  // Extract out users that are allowed on login screen.
  return FindLoginAllowedUsersFrom(result);
}

UserList UserManagerImpl::FindLoginAllowedUsersFrom(
    const UserList& users) const {
  UserList found_users;
  for (User* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskChromeAppManager and KioskWebAppManager.
    if (user->IsKioskType()) {
      continue;
    }
    const bool meets_allowlist_requirements =
        !user->HasGaiaAccount() || IsGaiaUserAllowed(*user);
    // Public session accounts are always shown on login screen.
    const bool meets_show_users_requirements =
        show_users_on_sign_in_ || user->GetType() == UserType::kPublicAccount;
    if (meets_allowlist_requirements && meets_show_users_requirements) {
      found_users.push_back(user);
    }
  }
  return found_users;
}

void UserManagerImpl::SetShowUsersOnSignIn(bool value) {
  show_users_on_sign_in_ = value;
}

const UserList& UserManagerImpl::GetLoggedInUsers() const {
  return logged_in_users_;
}

const UserList& UserManagerImpl::GetLRULoggedInUsers() const {
  return lru_logged_in_users_;
}

UserList UserManagerImpl::GetUnlockUsers() const {
  std::optional<MultiUserSignInPolicy> primary_policy =
      GetMultiUserSignInPolicy(primary_user_);
  if (!primary_policy.has_value()) {
    // Locking is not allowed until the primary user profile is created.
    return {};
  }

  // Specific case: only one logged in user or
  // primary user has primary-only multi-user policy.
  if (logged_in_users_.size() == 1 ||
      primary_policy == MultiUserSignInPolicy::kPrimaryOnly) {
    return primary_user_->CanLock() ? UserList{{primary_user_.get()}}
                                    : UserList{};
  }

  // Fill list of potential unlock users based on multi-user policy state.
  UserList unlock_users;
  for (User* user : logged_in_users_) {
    std::optional<MultiUserSignInPolicy> policy =
        GetMultiUserSignInPolicy(user);
    if (!policy.has_value()) {
      continue;
    }
    if (policy == MultiUserSignInPolicy::kUnrestricted && user->CanLock()) {
      unlock_users.push_back(user);
    } else if (policy == MultiUserSignInPolicy::kPrimaryOnly) {
      NOTREACHED()
          << "Spotted primary-only multi-user policy for non-primary user";
    }
  }

  return unlock_users;
}

const AccountId& UserManagerImpl::GetOwnerAccountId() const {
  if (!owner_account_id_.has_value()) {
    return EmptyAccountId();
  }
  return *owner_account_id_;
}

void UserManagerImpl::GetOwnerAccountIdAsync(
    base::OnceCallback<void(const AccountId&)> callback) const {
  if (owner_account_id_.has_value()) {
    std::move(callback).Run(*owner_account_id_);
    return;
  }
  pending_owner_callbacks_.AddUnsafe(std::move(callback));
}

const AccountId& UserManagerImpl::GetLastSessionActiveAccountId() const {
  return last_session_active_account_id_;
}

void UserManagerImpl::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!last_session_active_account_id_initialized_) {
    last_session_active_account_id_ = AccountId::FromUserEmail(
        local_state_->GetString(prefs::kLastActiveUser));
    last_session_active_account_id_initialized_ = true;
  }

  bool is_primary_login = logged_in_users_.empty();
  // For primary login case, i.e., there was no previous login, so there should
  // be no active user. Otherwise, there was some previous login, so there
  // should be an active user.
  CHECK_EQ(!active_user_, is_primary_login);

  User* user = FindUserAndModify(account_id);
  CHECK(user);
  OnUserLoggedIn(*user, username_hash);

  if (is_primary_login) {
    OnPrimaryUserLoggedIn(*user);
    OnActiveUserSwitched(*user);

    local_state_->CommitPendingWrite();
    NotifyOnLogin();
  } else {
    SendMultiUserSignInMetrics();
    NotifyUserAddedToSession(user);
  }
}

bool UserManagerImpl::EnsureUser(const AccountId& account_id,
                                 UserType user_type,
                                 bool is_ephemeral) {
  User* user = FindUserInListAndModify(account_id);
  switch (user_type) {
    case UserType::kRegular:
    case UserType::kChild:
      if (user) {
        KnownUser known_user(local_state_.get());
        // There already is a registered User, update the type as needed.
        if (user->GetType() != user_type) {
          user->SetType(user_type);
          SaveUserType(user);
          // Clear information about profile policy requirements to enforce
          // setting it again for the new account type.
          known_user.ClearProfileRequiresPolicy(account_id);
        }
        break;
      }

      // Ensure User is created.
      if (is_ephemeral) {
        user = AddEphemeralUser(account_id, user_type);
      } else {
        user = AddGaiaUser(account_id, user_type);
      }
      return true;

    case UserType::kGuest:
      CHECK(!user);
      user = AddGuestUser();
      return true;

    case UserType::kPublicAccount:
      if (!user) {
        user = AddPublicAccountUser(account_id);
        return true;
      }
      break;

    case UserType::kKioskChromeApp:
    case UserType::kKioskWebApp:
    case UserType::kKioskIWA:
    case UserType::kKioskArcvmApp:
      // Do nothing. User should be already there.
      break;

    default:
      NOTREACHED() << "Unhandled usert type " << user_type;
  }
  CHECK(user);
  return false;
}

void UserManagerImpl::OnUserLoggedIn(User& user,
                                     std::string_view username_hash) {
  user.set_is_logged_in(true);
  user.set_username_hash(username_hash);
  logged_in_users_.push_back(&user);
  UpdateNumLoggedInUsersCrashKey(logged_in_users_.size());
  lru_logged_in_users_.push_back(&user);
}

void UserManagerImpl::OnPrimaryUserLoggedIn(User& user) {
  CHECK(!primary_user_);
  primary_user_ = &user;

  const auto& account_id = user.GetAccountId();
  delegate_->OverrideDirHome(user);
  if (user.HasGaiaAccount()) {
    // Move the user to the front of the list.
    auto it = std::ranges::find(persisted_users_, account_id,
                                [](auto& ptr) { return ptr->GetAccountId(); });
    if (it != persisted_users_.end()) {
      std::rotate(persisted_users_.begin(), it, it + 1);
      ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                              prefs::kRegularUsersPref);
      prefs_users_update->clear();
      for (const User* u : persisted_users_) {
        if (u->HasGaiaAccount()) {
          prefs_users_update->Append(u->GetAccountId().GetUserEmail());
        }
      }
    }
    SendGaiaUserLoginMetrics(account_id);
  }

  base::UmaHistogramEnumeration("UserManager.LoginUserType", user.GetType());
  UpdateSessionTypeCrashKey(user.GetType());

  local_state_->SetString(
      prefs::kLastLoggedInGaiaUser,
      user.HasGaiaAccount() ? account_id.GetUserEmail() : "");

  delegate_->CheckProfileOnLogin(user);
}

void UserManagerImpl::SwitchActiveUser(const AccountId& account_id) {
  User* user = FindUserAndModify(account_id);
  if (!user) {
    DUMP_WILL_BE_NOTREACHED() << "Switching to a non-existing user";
    return;
  }
  if (user == active_user_) {
    DUMP_WILL_BE_NOTREACHED() << "Switching to a user who is already active";
    return;
  }
  if (!user->is_logged_in()) {
    DUMP_WILL_BE_NOTREACHED() << "Switching to a user that is not logged in";
    return;
  }
  if (!user->HasGaiaAccount()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Switching to a user without gaia account (non-regular one)";
    return;
  }
  if (user->username_hash().empty()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Switching to a user that doesn't have username_hash set";
    return;
  }

  DCHECK(active_user_);
  OnActiveUserSwitched(*user);

  NotifyActiveUserChanged(active_user_);
  NotifyLoginStateUpdated();
}

void UserManagerImpl::SwitchToLastActiveUser() {
  if (!last_session_active_account_id_.is_valid()) {
    return;
  }

  if (AccountId::FromUserEmail(
          GetActiveUser()->GetAccountId().GetUserEmail()) !=
      last_session_active_account_id_) {
    SwitchActiveUser(last_session_active_account_id_);
  }

  // Make sure that this function gets run only once.
  last_session_active_account_id_.clear();
}

void UserManagerImpl::OnSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyLoginStateUpdated();
  local_state_->CommitPendingWrite();
}

bool UserManagerImpl::UpdateDeviceLocalAccountUser(
    const base::span<DeviceLocalAccountInfo>& device_local_accounts) {
  // Try to remove any device local account data marked as pending removal.
  RemovePendingDeviceLocalAccount();

  // Persist the new list of device local accounts in a pref. These accounts
  // will be loaded in LoadDeviceLocalAccounts() on the next reboot regardless
  // of whether they still exist in kAccountsPrefDeviceLocalAccounts, allowing
  // us to clean up associated data if they disappear from policy.
  ScopedListPrefUpdate prefs_device_local_accounts_update(
      GetLocalState(), prefs::kDeviceLocalAccountsWithSavedData);
  prefs_device_local_accounts_update->clear();
  for (const auto& account : device_local_accounts) {
    prefs_device_local_accounts_update->Append(account.user_id);
  }

  // If the list of device local accounts has not changed, return.
  if (!IsDeviceLocalAccountChanged(persisted_users_, device_local_accounts)) {
    return false;
  }

  // Remove the old device local accounts from the user list.
  // Take snapshot because RemoveUserFromListImpl will update |user_|.
  std::vector<User*> users(persisted_users_.begin(), persisted_users_.end());
  for (User* user : users) {
    if (!user->IsDeviceLocalAccount()) {
      // Non device local account is not a target to be removed.
      continue;
    }
    if (std::ranges::any_of(
            device_local_accounts, [user](const DeviceLocalAccountInfo& info) {
              return info.user_id == user->GetAccountId().GetUserEmail() &&
                     info.type == user->GetType();
            })) {
      // The account exists in new device local accounts. Do not remove.
      continue;
    }
    if (user == GetActiveUser()) {
      // This user is active, so keep the instance. Instead, mark it as
      // pending removal, so it will be removed in the next turn.
      GetLocalState()->SetString(prefs::kDeviceLocalAccountPendingDataRemoval,
                                 user->GetAccountId().GetUserEmail());
      std::erase(persisted_users_, user);
      continue;
    }

    // Remove the instance.
    RemoveUserFromListImpl(user->GetAccountId(),
                           UserRemovalReason::DEVICE_LOCAL_ACCOUNT_UPDATED,
                           /*trigger_cryptohome_removal=*/false);
  }

  // Add the new device local accounts to the front of the user list.
  for (size_t i = 0; i < device_local_accounts.size(); ++i) {
    const DeviceLocalAccountInfo& account = device_local_accounts[i];
    auto iter = std::find_if(
        persisted_users_.begin() + i, persisted_users_.end(),
        [&account](const User* user) {
          return user->GetAccountId().GetUserEmail() == account.user_id &&
                 user->GetType() == account.type;
        });
    if (iter != persisted_users_.end()) {
      // Found the instance. Rotate the `persisted_users_` to place the found
      // user at the i-th position.
      std::rotate(persisted_users_.begin() + i, iter, iter + 1);
    } else {
      // Not found so create an instance.
      // Using `new` to access a non-public constructor.
      user_storage_.push_back(base::WrapUnique(
          new User(AccountId::FromUserEmail(account.user_id), account.type)));
      persisted_users_.insert(persisted_users_.begin() + i,
                              user_storage_.back().get());
    }
    if (account.display_name) {
      SaveUserDisplayName(AccountId::FromUserEmail(account.user_id),
                          *account.display_name);
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnDeviceLocalUserListUpdated();
  }

  return true;
}

void UserManagerImpl::RemoveUser(const AccountId& account_id,
                                 UserRemovalReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const User* const user = FindUser(account_id);

  // Misconfigured user would not be included in GetPersistedUsers(),
  // account for them separately.
  // TODO(crbug.com/404898436): Find a better way for the special handling.
  if (reason == UserRemovalReason::MISCONFIGURED_USER) {
    if (user && user->IsDeviceLocalAccount()) {
      // Device local account users are created from policy and should only be
      // remove from the user list on policy change. So just remove crypothome
      // for them instead of a full removal.
      delegate_->RemoveCryptohomeAsync(account_id);
    } else {
      RemoveUserInternal(account_id, reason);
    }

    return;
  }

  if (!CanUserBeRemoved(user)) {
    return;
  }

  RemoveUserInternal(account_id, reason);
}

void UserManagerImpl::RemoveUserInternal(const AccountId& account_id,
                                         UserRemovalReason reason) {
  // If owner is not yet set, this waits for its readiness.
  GetOwnerAccountIdAsync(
      base::BindOnce(&UserManagerImpl::RemoveUserInternalWithOwnerAccountId,
                     weak_factory_.GetWeakPtr(), account_id, reason));
}

void UserManagerImpl::RemoveUserInternalWithOwnerAccountId(
    const AccountId& account_id,
    UserRemovalReason reason,
    const AccountId& owner_account_id) {
  if (account_id == owner_account_id) {
    // Owner is not allowed to be removed from the device.
    return;
  }
  delegate_->RemoveProfileByAccountId(account_id);

  RemoveUserFromListImpl(account_id, reason,
                         /*trigger_cryptohome_removal=*/true);
}

void UserManagerImpl::RemoveUserFromList(const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, UserRemovalReason::UNKNOWN,
                         /*trigger_cryptohome_removal=*/false);
}

void UserManagerImpl::RemoveUserFromListForRecreation(
    const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, /*reason=*/std::nullopt,
                         /*trigger_cryptohome_removal=*/false);
}

bool UserManagerImpl::RemoveStaleEphemeralUsers() {
  CHECK(!IsUserLoggedIn());
  bool changed = false;
  const auto owner_id = GetOwnerAccountId();

  // Take snapshot because DeleteUser called in the loop will update it.
  std::vector<User*> users(persisted_users_.begin(), persisted_users_.end());
  for (user_manager::User* user : users) {
    const AccountId account_id = user->GetAccountId();
    if (user->HasGaiaAccount() && account_id != owner_id &&
        IsEphemeralAccountId(account_id)) {
      RemoveUserFromListImpl(
          account_id,
          /*reason=*/UserRemovalReason::DEVICE_EPHEMERAL_USERS_ENABLED,
          /*trigger_cryptohome_removal=*/false);
      changed = true;
    }
  }
  return changed;
}

void UserManagerImpl::CleanStaleUserInformationFor(
    const AccountId& account_id) {
  KnownUser known_user(local_state_);
  if (known_user.UserExists(account_id)) {
    known_user.RemovePrefs(account_id);
    known_user.SaveKnownUser(account_id);
  }
  // For users with actual online identity we need
  // to remove them from the user list as well, otherwise
  // they would be incorrectly detected as "existing" later.
  // For Kiosk/MGS users it is actually expected for User object
  // to exist even if no cryptohome is present for the user.
  const User* user = FindUserInList(account_id);
  if (!user) {
    return;
  }
  if (!User::TypeHasGaiaAccount(user->GetType())) {
    return;
  }
  RemoveUserFromList(account_id);
}

// Use AccountId instead of const AccountId& here, since the account_id maybe
// originated from the one stored in the User being removed, and the removed ID
// will be kept using after the actual deletion for observer call.
void UserManagerImpl::RemoveUserFromListImpl(
    AccountId account_id,
    std::optional<UserRemovalReason> reason,
    bool trigger_cryptohome_removal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reason.has_value()) {
    NotifyUserToBeRemoved(account_id);
  }
  if (trigger_cryptohome_removal) {
    delegate_->RemoveCryptohomeAsync(account_id);
  }

  RemoveNonCryptohomeData(account_id);
  KnownUser(local_state_.get()).RemovePrefs(account_id);

  // After the User object is deleted from memory in DeleteUser() here,
  // the account_id reference will be invalid if the reference points
  // to the account_id in the User object.
  DeleteUser(
      RemoveRegularOrSupervisedUserFromList(account_id, reason.has_value()));

  if (reason.has_value()) {
    NotifyUserRemoved(account_id, reason.value());
  }

  // Make sure that new data is persisted to Local State.
  local_state_->CommitPendingWrite();
}

bool UserManagerImpl::IsKnownUser(const AccountId& account_id) const {
  // We check for the presence of a misconfigured user as well. This is because
  // `WallpaperControllerClientImpl::RemoveUserWallpaper` would not remove
  // the wallpaper prefs if we return false here, thus leaving behind
  // orphan prefs for the misconfigured users.
  UserDirectoryIntegrityManager integrity_manager(local_state_.get());
  return FindUser(account_id) != nullptr ||
         integrity_manager.IsUserMisconfigured(account_id);
}

const User* UserManagerImpl::FindUser(const AccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& user : user_storage_) {
    if (user->GetAccountId() == account_id) {
      return user.get();
    }
  }
  return nullptr;
}

User* UserManagerImpl::FindUserAndModify(const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& user : user_storage_) {
    if (user->GetAccountId() == account_id) {
      return user.get();
    }
  }
  return nullptr;
}

const User* UserManagerImpl::GetActiveUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_user_;
}

User* UserManagerImpl::GetActiveUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_user_;
}

const User* UserManagerImpl::GetPrimaryUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return primary_user_;
}

void UserManagerImpl::SaveUserOAuthStatus(
    const AccountId& account_id,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = FindUserAndModify(account_id);
  if (user) {
    user->set_oauth_token_status(oauth_token_status);
  }

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  {
    ScopedDictPrefUpdate oauth_status_update(local_state_.get(),
                                             prefs::kUserOAuthTokenStatus);
    oauth_status_update->Set(account_id.GetUserEmail(),
                             static_cast<int>(oauth_token_status));
  }
  local_state_->CommitPendingWrite();
}

void UserManagerImpl::SaveForceOnlineSignin(const AccountId& account_id,
                                            bool force_online_signin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  User* const user = FindUserAndModify(account_id);
  if (user) {
    user->set_force_online_signin(force_online_signin);
  }

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  {
    ScopedDictPrefUpdate force_online_update(local_state_.get(),
                                             prefs::kUserForceOnlineSignin);
    force_online_update->Set(account_id.GetUserEmail(), force_online_signin);
  }
  local_state_->CommitPendingWrite();
}

void UserManagerImpl::SaveUserDisplayName(const AccountId& account_id,
                                          const std::u16string& display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (User* user = FindUserAndModify(account_id)) {
    user->set_display_name(display_name);

    // Do not update local state if data stored or cached outside the user's
    // cryptohome is to be treated as ephemeral.
    if (!IsUserNonCryptohomeDataEphemeral(account_id)) {
      ScopedDictPrefUpdate display_name_update(local_state_.get(),
                                               prefs::kUserDisplayName);
      display_name_update->Set(account_id.GetUserEmail(), display_name);
    }
  }
}

void UserManagerImpl::SaveUserDisplayEmail(const AccountId& account_id,
                                           const std::string& display_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  User* user = FindUserAndModify(account_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
    return;  // Ignore if there is no such user.
  }

  user->set_display_email(display_email);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  ScopedDictPrefUpdate display_email_update(local_state_.get(),
                                            prefs::kUserDisplayEmail);
  display_email_update->Set(account_id.GetUserEmail(), display_email);
}

UserType UserManagerImpl::GetUserType(const AccountId& account_id) {
  const base::Value::Dict& prefs_user_types =
      local_state_->GetDict(prefs::kUserType);
  return GetStoredUserType(prefs_user_types, account_id);
}

void UserManagerImpl::SaveUserType(const User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(user);
  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user->GetAccountId())) {
    return;
  }

  ScopedDictPrefUpdate user_type_update(local_state_.get(), prefs::kUserType);
  user_type_update->Set(user->GetAccountId().GetAccountIdKey(),
                        static_cast<int>(user->GetType()));
  local_state_->CommitPendingWrite();
}

void UserManagerImpl::SetUserUsingSaml(const AccountId& account_id,
                                       bool using_saml,
                                       bool using_saml_principals_api) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& user = CHECK_DEREF(FindUserAndModify(account_id));
  user.set_using_saml(using_saml);

  user_manager::KnownUser known_user(local_state_);
  known_user.UpdateUsingSAML(account_id, using_saml);
  known_user.UpdateIsUsingSAMLPrincipalsAPI(
      account_id, using_saml && using_saml_principals_api);
  if (!using_saml) {
    known_user.ClearPasswordSyncToken(account_id);
  }
}

std::optional<std::string> UserManagerImpl::GetOwnerEmail() {
  const base::Value::Dict& owner = local_state_->GetDict(prefs::kOwnerAccount);
  std::optional<int> type = owner.FindInt(prefs::kOwnerAccountType);
  if (!type.has_value() || (static_cast<OwnerAccountType>(type.value())) !=
                               OwnerAccountType::kGoogleEmail) {
    return std::nullopt;
  }

  const std::string* email = owner.FindString(prefs::kOwnerAccountIdentity);
  // A valid email should not be empty, so return a nullopt if Chrome
  // accidentally saved an empty string.
  if (!email || email->empty()) {
    return std::nullopt;
  }
  return *email;
}

void UserManagerImpl::RecordOwner(const AccountId& owner) {
  base::Value::Dict owner_dict;
  owner_dict.Set(prefs::kOwnerAccountType,
                 static_cast<int>(OwnerAccountType::kGoogleEmail));
  owner_dict.Set(prefs::kOwnerAccountIdentity, owner.GetUserEmail());
  local_state_->SetDict(prefs::kOwnerAccount, std::move(owner_dict));
  // The information about the owner might be needed for recovery if Chrome
  // crashes before establishing ownership, so it needs to be written on disk as
  // soon as possible.
  local_state_->CommitPendingWrite();
}

void UserManagerImpl::UpdateUserAccountData(
    const AccountId& account_id,
    const UserAccountData& account_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SaveUserDisplayName(account_id, account_data.display_name());

  if (User* user = FindUserAndModify(account_id)) {
    std::u16string given_name = account_data.given_name();
    user->set_given_name(given_name);
    if (!IsUserNonCryptohomeDataEphemeral(account_id)) {
      ScopedDictPrefUpdate given_name_update(local_state_.get(),
                                             prefs::kUserGivenName);
      given_name_update->Set(account_id.GetUserEmail(), given_name);
    }
  }

  UpdateUserAccountLocale(account_id, account_data.locale());
}

void UserManagerImpl::ParseUserList(const base::Value::List& users_list,
                                    const std::set<AccountId>& existing_users,
                                    std::vector<AccountId>* users_vector,
                                    std::set<AccountId>* users_set) {
  users_vector->clear();
  users_set->clear();
  for (size_t i = 0; i < users_list.size(); ++i) {
    const std::string* email = users_list[i].GetIfString();
    if (!email || email->empty()) {
      LOG(ERROR) << "Corrupt entry in user list at index " << i << ".";
      continue;
    }

    const AccountId account_id =
        KnownUser(local_state_.get())
            .GetAccountId(*email, std::string() /* id */, AccountType::UNKNOWN);

    if (existing_users.find(account_id) != existing_users.end() ||
        !users_set->insert(account_id).second) {
      LOG(ERROR) << "Duplicate user: " << *email;
      continue;
    }
    users_vector->push_back(account_id);
  }
}

bool UserManagerImpl::IsOwnerUser(const User* user) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user && owner_account_id_.has_value() &&
         user->GetAccountId() == *owner_account_id_;
}

bool UserManagerImpl::IsPrimaryUser(const User* user) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user && user == primary_user_;
}

bool UserManagerImpl::IsEphemeralUser(const User* user) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user) {
    return false;
  }

  return IsEphemeralAccountId(user->GetAccountId());
}

bool UserManagerImpl::IsCurrentUserOwner() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsOwnerUser(active_user_);
}

bool UserManagerImpl::IsCurrentUserNew() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceFirstRunUI)) {
    return true;
  }

  return is_current_user_new_;
}

bool UserManagerImpl::IsCurrentUserNonCryptohomeDataEphemeral() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() &&
         IsUserNonCryptohomeDataEphemeral(GetActiveUser()->GetAccountId());
}

bool UserManagerImpl::IsCurrentUserCryptohomeDataEphemeral() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() &&
         IsUserCryptohomeDataEphemeral(GetActiveUser()->GetAccountId());
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_user_;
}

bool UserManagerImpl::IsLoggedInAsUserWithGaiaAccount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->HasGaiaAccount();
}

bool UserManagerImpl::IsLoggedInAsChildUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kChild;
}

bool UserManagerImpl::IsLoggedInAsManagedGuestSession() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() &&
         active_user_->GetType() == UserType::kPublicAccount;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kGuest;
}

bool UserManagerImpl::IsLoggedInAsKioskChromeApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() &&
         active_user_->GetType() == UserType::kKioskChromeApp;
}

bool UserManagerImpl::IsLoggedInAsKioskWebApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kKioskWebApp;
}

bool UserManagerImpl::IsLoggedInAsKioskIWA() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kKioskIWA;
}

bool UserManagerImpl::IsLoggedInAsKioskArcvmApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() &&
         active_user_->GetType() == UserType::kKioskArcvmApp;
}

bool UserManagerImpl::IsLoggedInAsAnyKioskApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->IsKioskType();
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsUserLoggedIn() && active_user_->GetAccountId() == StubAccountId();
}

bool UserManagerImpl::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  // Data belonging to the guest and stub users is always ephemeral.
  if (account_id == GuestAccountId() || account_id == StubAccountId()) {
    return true;
  }

  // Data belonging to the owner, anyone found on the user list and obsolete
  // device local accounts whose data has not been removed yet is not ephemeral.
  if (account_id == GetOwnerAccountId() || UserExistsInList(account_id) ||
      IsDeviceLocalAccountMarkedForRemoval(account_id)) {
    return false;
  }

  // Even though device-local accounts might be ephemeral (e.g. kiosk accounts),
  // non-cryptohome data of device-local accounts should be non-ephemeral.
  const User* user = FindUser(account_id);
  if (user && user->IsDeviceLocalAccount()) {
    return false;
  }

  // Data belonging to the currently logged-in user is ephemeral when:
  // a) The user logged into a regular gaia account while the ephemeral users
  //    policy was enabled.
  //    - or -
  // b) The user logged into any other account type.
  // TODO(crbug.com/278643115): The first condition may be redundant, because
  // we already check it in UserExistsInList above.
  if (user && user == active_user_ &&
      ((user->GetType() == UserType::kRegular &&
        std::ranges::find(persisted_users_, user) == persisted_users_.end()) ||
       !user->HasGaiaAccount())) {
    return true;
  }

  // Data belonging to any other user is ephemeral when:
  // a) Going through the regular login flow and the ephemeral users policy is
  //    enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return IsEphemeralAccountId(account_id) || HasBrowserRestarted();
}

bool UserManagerImpl::IsUserCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  return IsEphemeralAccountId(account_id);
}

bool UserManagerImpl::IsEphemeralAccountId(const AccountId& account_id) const {
  // Data belonging to the device owner is never ephemeral.
  if (account_id == GetOwnerAccountId()) {
    return false;
  }

  // Data belonging to the stub users is never ephemeral.
  if (account_id == StubAccountId()) {
    return false;
  }

  // Data belonging to the guest user is always ephemeral.
  if (account_id == GuestAccountId()) {
    return true;
  }

  // Data belonging to the public accounts (e.g. managed guest sessions) is
  // always ephemeral.
  if (const User* user = FindUser(account_id);
      user && user->GetType() == UserType::kPublicAccount) {
    return true;
  }

  const bool device_is_owned =
      IsEnterpriseManaged() || GetOwnerAccountId().is_valid();

  return device_is_owned &&
         GetEphemeralModeConfig().IsAccountIdIncluded(account_id);
}

void UserManagerImpl::AddObserver(UserManager::Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveObserver(UserManager::Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(obs);
}

void UserManagerImpl::AddSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_state_observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_state_observer_list_.RemoveObserver(obs);
}

void UserManagerImpl::NotifyLocalStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.LocalStateChanged(this);
  }
}

void UserManagerImpl::NotifyUserImageChanged(const User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserImageChanged(user);
  }
}

void UserManagerImpl::NotifyUserImageIsEnterpriseManagedChanged(
    const User& user,
    bool is_enterprise_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserImageIsEnterpriseManagedChanged(user, is_enterprise_managed);
  }
}

void UserManagerImpl::NotifyUserProfileImageUpdateFailed(const User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserProfileImageUpdateFailed(user);
  }
}

void UserManagerImpl::NotifyUserProfileImageUpdated(
    const User& user,
    const gfx::ImageSkia& profile_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserProfileImageUpdated(user, profile_image);
  }
}

void UserManagerImpl::NotifyUsersSignInConstraintsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUsersSignInConstraintsChanged();
  }
}

void UserManagerImpl::NotifyUserAffiliationUpdated(const User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserAffiliationUpdated(user);
  }
}

void UserManagerImpl::NotifyUserToBeRemoved(const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserToBeRemoved(account_id);
  }
}

void UserManagerImpl::NotifyUserRemoved(const AccountId& account_id,
                                        UserRemovalReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserRemoved(account_id, reason);
  }
}

void UserManagerImpl::NotifyUserNotAllowed(const std::string& user_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserNotAllowed(user_email);
  }
}

bool UserManagerImpl::IsGuestSessionAllowed() const {
  return guest_session_allowed_;
}

void UserManagerImpl::SetGuestSessionAllowed(bool value) {
  guest_session_allowed_ = value;
}

bool UserManagerImpl::IsGaiaUserAllowed(const User& user) const {
  DCHECK(user.HasGaiaAccount());
  return ash::UserLoginPermissionTracker::Get()->IsUserAllowlisted(
      user.GetAccountId().GetUserEmail(), nullptr, user.GetType());
}

bool UserManagerImpl::IsUserAllowed(const User& user) const {
  DCHECK(user.GetType() == UserType::kRegular ||
         user.GetType() == UserType::kGuest ||
         user.GetType() == UserType::kChild);

  return UserManager::IsUserAllowed(
      user, IsGuestSessionAllowed(),
      user.HasGaiaAccount() && IsGaiaUserAllowed(user));
}

bool UserManagerImpl::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         kSupervisedUserDomain;
}

bool UserManagerImpl::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return account_id == AccountId::FromUserEmail(GetLocalState()->GetString(
                           prefs::kDeviceLocalAccountPendingDataRemoval));
}

bool UserManagerImpl::CanUserBeRemoved(const User* user) const {
  // Only regular users are allowed to be manually removed.
  if (!user || !user->HasGaiaAccount()) {
    return false;
  }

  // Sanity check: we must not remove single user unless it's an enterprise
  // device. This check may seem redundant at a first sight because
  // this single user must be an owner and we perform special check later
  // in order not to remove an owner. However due to non-instant nature of
  // ownership assignment this later check may sometimes fail.
  // See http://crosbug.com/12723
  if (persisted_users_.size() < 2 && !IsEnterpriseManaged()) {
    return false;
  }

  // Sanity check: do not allow any of the the logged in users to be removed.
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end(); ++it) {
    if ((*it)->GetAccountId() == user->GetAccountId()) {
      return false;
    }
  }

  return true;
}

const UserManagerImpl::EphemeralModeConfig&
UserManagerImpl::GetEphemeralModeConfig() const {
  return ephemeral_mode_config_;
}

void UserManagerImpl::SetEphemeralModeConfig(
    EphemeralModeConfig ephemeral_mode_config) {
  ephemeral_mode_config_ = std::move(ephemeral_mode_config);
}

void UserManagerImpl::SetIsCurrentUserNew(bool is_new) {
  is_current_user_new_ = is_new;
}

void UserManagerImpl::ResetOwnerId() {
  owner_account_id_ = std::nullopt;
}

void UserManagerImpl::SetOwnerId(const AccountId& owner_account_id) {
  owner_account_id_ = owner_account_id;
  pending_owner_callbacks_.Notify(owner_account_id);
  NotifyLoginStateUpdated();
}

void UserManagerImpl::EnsureUsersLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_state_) {
    return;
  }

  const base::Value::List& prefs_regular_users =
      local_state_->GetList(prefs::kRegularUsersPref);

  const base::Value::Dict& prefs_display_names =
      local_state_->GetDict(prefs::kUserDisplayName);
  const base::Value::Dict& prefs_given_names =
      local_state_->GetDict(prefs::kUserGivenName);
  const base::Value::Dict& prefs_display_emails =
      local_state_->GetDict(prefs::kUserDisplayEmail);
  const base::Value::Dict& prefs_user_types =
      local_state_->GetDict(prefs::kUserType);

  // Load public sessions first.
  std::set<AccountId> device_local_accounts_set;
  LoadDeviceLocalAccounts(&device_local_accounts_set);

  // Load regular users and supervised users.
  std::vector<AccountId> regular_users;
  std::set<AccountId> regular_users_set;
  ParseUserList(prefs_regular_users, device_local_accounts_set, &regular_users,
                &regular_users_set);
  for (std::vector<AccountId>::const_iterator it = regular_users.begin();
       it != regular_users.end(); ++it) {
    if (IsDeprecatedSupervisedAccountId(*it)) {
      RemoveLegacySupervisedUser(*it);
      // Hide legacy supervised users from the login screen if not removed.
      continue;
    }

    UserDirectoryIntegrityManager integrity_manager(local_state_.get());
    if (integrity_manager.IsUserMisconfigured(*it)) {
      // Skip misconfigured user.
      VLOG(1) << "Encountered misconfigured user while loading list of "
                 "users, skipping";
      continue;
    }

    base::UmaHistogramEnumeration(
        kLegacySupervisedUsersHistogramName,
        LegacySupervisedUserStatus::kGaiaUserDisplayed);
    User* user =
        User::CreateRegularUser(*it, GetStoredUserType(prefs_user_types, *it));
    user->set_oauth_token_status(LoadUserOAuthStatus(*it));
    user->set_force_online_signin(LoadForceOnlineSignin(*it));
    KnownUser known_user(local_state_.get());
    user->set_using_saml(known_user.IsUsingSAML(*it));

    user_storage_.emplace_back(user);
    persisted_users_.push_back(user);
  }

  for (user_manager::User* user : persisted_users_) {
    auto& account_id = user->GetAccountId();
    const std::string* display_name =
        prefs_display_names.FindString(account_id.GetUserEmail());
    if (display_name) {
      user->set_display_name(base::UTF8ToUTF16(*display_name));
    }

    const std::string* given_name =
        prefs_given_names.FindString(account_id.GetUserEmail());
    if (given_name) {
      user->set_given_name(base::UTF8ToUTF16(*given_name));
    }

    const std::string* display_email =
        prefs_display_emails.FindString(account_id.GetUserEmail());
    if (display_email) {
      user->set_display_email(*display_email);
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnUserListLoaded();
  }
}

void UserManagerImpl::LoadDeviceLocalAccounts(
    std::set<AccountId>* device_local_accounts_set) {
  const base::Value::List& prefs_device_local_accounts =
      GetLocalState()->GetList(prefs::kDeviceLocalAccountsWithSavedData);
  std::vector<AccountId> device_local_accounts;
  ParseUserList(prefs_device_local_accounts, std::set<AccountId>(),
                &device_local_accounts, device_local_accounts_set);
  for (const AccountId& account_id : device_local_accounts) {
    if (IsDeprecatedArcKioskAccountId(account_id)) {
      RemoveDeprecatedArcKioskUser(account_id);
      // Remove or hide deprecated ARC kiosk users from the login screen.
      continue;
    }

    auto device_local_account_type =
        policy::GetDeviceLocalAccountType(account_id.GetUserEmail());
    CHECK(device_local_account_type.has_value());

    // Using `new` to access a non-public constructor.
    user_storage_.push_back(base::WrapUnique(new User(
        account_id,
        DeviceLocalAccountTypeToUserType(*device_local_account_type))));
    persisted_users_.push_back(user_storage_.back().get());
  }
}

void UserManagerImpl::RemovePendingDeviceLocalAccount() {
  PrefService* local_state = GetLocalState();
  const std::string device_local_account_pending_data_removal =
      local_state->GetString(prefs::kDeviceLocalAccountPendingDataRemoval);
  if (device_local_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       device_local_account_pending_data_removal ==
           GetActiveUser()->GetAccountId().GetUserEmail())) {
    return;
  }

  RemoveUserFromListImpl(
      AccountId::FromUserEmail(device_local_account_pending_data_removal),
      user_manager::UserRemovalReason::DEVICE_LOCAL_ACCOUNT_UPDATED,
      /*trigger_cryptohome_removal=*/false);
  local_state->ClearPref(prefs::kDeviceLocalAccountPendingDataRemoval);
}

UserList& UserManagerImpl::GetUsersAndModify() {
  return persisted_users_;
}

const User* UserManagerImpl::FindUserInList(const AccountId& account_id) const {
  for (const User* user : persisted_users_) {
    if (user->GetAccountId() == account_id) {
      return user;
    }
  }
  return nullptr;
}

bool UserManagerImpl::UserExistsInList(const AccountId& account_id) const {
  const base::Value::List& user_list =
      local_state_->GetList(prefs::kRegularUsersPref);
  for (const base::Value& i : user_list) {
    const std::string* email = i.GetIfString();
    if (email && (account_id.GetUserEmail() == *email)) {
      return true;
    }
  }
  return false;
}

User* UserManagerImpl::FindUserInListAndModify(const AccountId& account_id) {
  UserList& users = GetUsersAndModify();
  for (UserList::iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      return *it;
    }
  }
  return nullptr;
}

// TODO(crbug.com/278643115): Verify AccountId uniqueness in the following
// User creation methods.

User* UserManagerImpl::AddGaiaUser(const AccountId& account_id,
                                   UserType user_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* user = User::CreateRegularUser(account_id, user_type);
  user_storage_.emplace_back(user);
  SaveUserType(user);

  user->set_oauth_token_status(LoadUserOAuthStatus(account_id));
  SaveUserDisplayName(account_id,
                      base::UTF8ToUTF16(user->GetAccountName(
                          /*use_display_email=*/true)));

  // Add to the persisted list.
  persisted_users_.push_back(user);
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          prefs::kRegularUsersPref);
  prefs_users_update->Append(base::Value(account_id.GetUserEmail()));

  return user;
}

User* UserManagerImpl::AddEphemeralUser(const AccountId& account_id,
                                        UserType user_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* user = User::CreateRegularUser(account_id, user_type);
  user_storage_.emplace_back(user);

  return user;
}

User* UserManagerImpl::AddGuestUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* user = User::CreateGuestUser(GuestAccountId());
  user_storage_.emplace_back(user);
  return user;
}

User* UserManagerImpl::AddPublicAccountUser(const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* user = User::CreatePublicAccountUser(account_id);
  user_storage_.emplace_back(user);

  return user;
}

bool UserManagerImpl::OnUserProfileCreated(const AccountId& account_id,
                                           PrefService* prefs) {
  // Find a User from `user_storage_`.
  // FindUserAndModify may overlook some existing User instance, because
  // the list may not contain ephemeral users that are getting stale.
  auto it = std::ranges::find(user_storage_, account_id,
                              [](auto& ptr) { return ptr->GetAccountId(); });
  auto* user = it == user_storage_.end() ? nullptr : it->get();
  CHECK(user);
  if (user->is_profile_created()) {
    // This happens sometimes in browser_tests.
    // See also kIgnoreUserProfileMappingForTests and its uses.
    // TODO(b/294452567): Consider how to remove this workaround for testing.
    LOG(ERROR) << "user profile duplicated";
    CHECK_IS_TEST();
    return false;
  }

  // Update `user` profile profs before profile created notification so that
  // observers could get profile prefs via `user`.
  CHECK(!user->GetProfilePrefs());
  user->SetProfilePrefs(prefs);

  user->SetProfileIsCreated();

  observer_list_.Notify(&UserManager::Observer::OnUserProfileCreated, *user);

  return true;
}

void UserManagerImpl::OnUserProfileWillBeDestroyed(
    const AccountId& account_id) {
  // Find from user_stroage_. See OnUserProfileCreated for the reason why not
  // using FindUserAndModify.
  auto it = std::ranges::find(user_storage_, account_id,
                              [](auto& ptr) { return ptr->GetAccountId(); });
  auto* user = it == user_storage_.end() ? nullptr : it->get();
  CHECK(user);

  observer_list_.Notify(&UserManager::Observer::OnUserProfileWillBeDestroyed,
                        *user);

  user->SetProfilePrefs(nullptr);
}

void UserManagerImpl::NotifyActiveUserChanged(User* active_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : session_state_observer_list_) {
    observer.ActiveUserChanged(active_user);
  }
}

void UserManagerImpl::NotifyLoginStateUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : session_state_observer_list_) {
    observer.OnLoginStateUpdated(active_user_);
  }
}

void UserManagerImpl::NotifyOnLogin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_user_);

  // TODO(crbug.com/278643115): Migrate into ActiveUserChanged
  // and UserAddedToSession calls.
  for (auto& observer : observer_list_) {
    observer.OnUserLoggedIn(*active_user_);
  }

  NotifyActiveUserChanged(active_user_);
  NotifyLoginStateUpdated();
}

User::OAuthTokenStatus UserManagerImpl::LoadUserOAuthStatus(
    const AccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::Dict& prefs_oauth_status =
      local_state_->GetDict(prefs::kUserOAuthTokenStatus);

  std::optional<int> oauth_token_status =
      prefs_oauth_status.FindInt(account_id.GetUserEmail());
  if (!oauth_token_status.has_value()) {
    return User::OAUTH_TOKEN_STATUS_UNKNOWN;
  }

  return static_cast<User::OAuthTokenStatus>(oauth_token_status.value());
}

bool UserManagerImpl::LoadForceOnlineSignin(const AccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::Dict& prefs_force_online =
      local_state_->GetDict(prefs::kUserForceOnlineSignin);

  return prefs_force_online.FindBool(account_id.GetUserEmail()).value_or(false);
}

void UserManagerImpl::RemoveNonCryptohomeData(const AccountId& account_id) {
  ScopedDictPrefUpdate(local_state_.get(), prefs::kUserDisplayName)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), prefs::kUserGivenName)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), prefs::kUserDisplayEmail)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), prefs::kUserOAuthTokenStatus)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), prefs::kUserForceOnlineSignin)
      ->Remove(account_id.GetUserEmail());

  KnownUser(local_state_.get()).RemovePrefs(account_id);

  const AccountId last_active_user =
      AccountId::FromUserEmail(local_state_->GetString(prefs::kLastActiveUser));
  if (account_id == last_active_user) {
    local_state_->SetString(prefs::kLastActiveUser, std::string());
  }
}

User* UserManagerImpl::RemoveRegularOrSupervisedUserFromList(
    const AccountId& account_id,
    bool notify) {
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          prefs::kRegularUsersPref);
  prefs_users_update->clear();
  User* user = nullptr;
  for (UserList::iterator it = persisted_users_.begin();
       it != persisted_users_.end();) {
    if ((*it)->GetAccountId() == account_id) {
      user = *it;
      it = persisted_users_.erase(it);
    } else {
      if ((*it)->HasGaiaAccount()) {
        const std::string user_email = (*it)->GetAccountId().GetUserEmail();
        prefs_users_update->Append(user_email);
      }
      ++it;
    }
  }
  if (notify) {
    NotifyLocalStateChanged();
  }
  return user;
}

void UserManagerImpl::NotifyUserAddedToSession(const User* added_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : session_state_observer_list_) {
    observer.UserAddedToSession(added_user);
  }
}

PrefService* UserManagerImpl::GetLocalState() const {
  return local_state_.get();
}

bool UserManagerImpl::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kFirstExecAfterBoot);
}

void UserManagerImpl::SetUserPolicyStatus(const AccountId& account_id,
                                          bool is_managed,
                                          bool is_affiliated) {
  User* user = FindUserAndModify(account_id);
  if (!user) {
    return;
  }
  user->SetUserPolicyStatus(is_managed, is_affiliated);
  NotifyUserAffiliationUpdated(*user);
}

bool UserManagerImpl::HasBrowserRestarted() const {
  return base::SysInfo::IsRunningOnChromeOS() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kLoginUser);
}

void UserManagerImpl::Initialize() {
  UserManager::Initialize();
  if (!HasBrowserRestarted()) {
    // local_state may be null in unit tests.
    if (local_state_) {
      KnownUser known_user(local_state_.get());
      known_user.CleanObsoletePrefs();
    }
  }
  EnsureUsersLoaded();
  NotifyLoginStateUpdated();
}

void UserManagerImpl::OnActiveUserSwitched(User& new_active_user) {
  if (active_user_) {
    active_user_->set_is_active(false);
  }
  active_user_ = &new_active_user;
  active_user_->set_is_active(true);

  local_state_->SetString(prefs::kLastActiveUser,
                          active_user_->GetAccountId().GetUserEmail());
  local_state_->CommitPendingWrite();

  // Move the user to the front of the list.
  UserList::iterator it = std::ranges::find(lru_logged_in_users_, active_user_);
  CHECK(it != lru_logged_in_users_.end());
  std::rotate(lru_logged_in_users_.begin(), it, it + 1);
}

// static
void UserManagerImpl::UpdateNumLoggedInUsersCrashKey(size_t num_users) {
  static crash_reporter::CrashKeyString<64> crash_key("num-users");
  crash_key.Set(base::NumberToString(num_users));
}

// static
void UserManagerImpl::UpdateSessionTypeCrashKey(UserType active_user_type) {
  static crash_reporter::CrashKeyString<32> session_type("session-type");
  session_type.Set(UserTypeToString(active_user_type));
}

void UserManagerImpl::SendGaiaUserLoginMetrics(const AccountId& account_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (IsFirstExecAfterBoot()) {
    return;
  }

  const std::string last_email =
      local_state_->GetString(prefs::kLastLoggedInGaiaUser);
  const base::TimeDelta time_to_login =
      base::TimeTicks::Now() - manager_creation_time_;
  if (!last_email.empty() &&
      account_id != AccountId::FromUserEmail(last_email) &&
      time_to_login.InSeconds() <= kLogoutToLoginDelayMaxSec) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("UserManager.LogoutToLoginDelay",
                                time_to_login.InSeconds(), 1,
                                kLogoutToLoginDelayMaxSec, 50);
  }
}

void UserManagerImpl::SendMultiUserSignInMetrics() {
  size_t users = logged_in_users_.size();
  if (!users) {
    return;
  }

  // Write the user number as UMA stat when a multi user session is possible.
  if (users + GetUsersAllowedForMultiUserSignIn().size() > 1) {
    // Keep MultiProfile name here for compatibility of historical reason.
    // It is for multi-user sign-in.
    UMA_HISTOGRAM_COUNTS_100("MultiProfile.UsersPerSessionIncremental", users);
  }
}

void UserManagerImpl::UpdateUserAccountLocale(const AccountId& account_id,
                                              const std::string& locale) {
  if (!locale.empty() && locale != delegate_->GetApplicationLocale()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](const std::string& locale) {
              return l10n_util::CheckAndResolveLocale(locale).value_or("");
            },
            locale),
        base::BindOnce(&UserManagerImpl::DoUpdateAccountLocale,
                       weak_factory_.GetWeakPtr(), account_id));
  } else {
    DoUpdateAccountLocale(account_id, locale);
  }
}

void UserManagerImpl::DoUpdateAccountLocale(
    const AccountId& account_id,
    const std::string& resolved_locale) {
  User* user = FindUserAndModify(account_id);
  if (user) {
    user->SetAccountLocale(resolved_locale);
  }
}

void UserManagerImpl::DeleteUser(User* user) {
  if (active_user_ == user) {
    active_user_ = nullptr;
  }
  if (primary_user_ == user) {
    primary_user_ = nullptr;
  }
  std::erase(persisted_users_, user);
  std::erase(logged_in_users_, user);
  std::erase(lru_logged_in_users_, user);

  std::erase_if(user_storage_, [user](auto& ptr) { return ptr.get() == user; });
}

// TODO(crbug.com/40755604): Remove dormant legacy supervised user cryptohomes.
// After we have enough confidence that there are no more supervised users on
// devices in the wild, remove this.
void UserManagerImpl::RemoveLegacySupervisedUser(const AccountId& account_id) {
  DCHECK(IsDeprecatedSupervisedAccountId(account_id));
  // Since we skip adding legacy supervised users to the users list,
  // FindUser(account_id) returns nullptr and CanUserBeRemoved() returns
  // false. This is why we call RemoveUserInternal() directly instead of
  // RemoveUser().
  RemoveUserInternal(account_id, UserRemovalReason::UNKNOWN);
  base::UmaHistogramEnumeration(kLegacySupervisedUsersHistogramName,
                                LegacySupervisedUserStatus::kLSUDeleted);
}

bool UserManagerImpl::IsDeprecatedArcKioskAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) == kArcKioskDomain;
}

// TODO(b/355590943): Remove dormant deprecated ARC kiosk user cryptohomes.
// Remove this once confident that all ARC kiosk cryptohomes are cleaned up.
void UserManagerImpl::RemoveDeprecatedArcKioskUser(
    const AccountId& account_id) {
  CHECK(IsDeprecatedArcKioskAccountId(account_id));
  if (base::FeatureList::IsEnabled(kRemoveDeprecatedArcKioskUsersOnStartup)) {
    RemoveUserInternal(account_id, UserRemovalReason::UNKNOWN);
    base::UmaHistogramEnumeration(kDeprecatedArcKioskUsersHistogramName,
                                  DeprecatedArcKioskUserStatus::kDeleted);
  } else {
    base::UmaHistogramEnumeration(kDeprecatedArcKioskUsersHistogramName,
                                  DeprecatedArcKioskUserStatus::kHidden);
  }
}

bool UserManagerImpl::IsEnterpriseManaged() const {
  if (!ash::InstallAttributes::IsInitialized()) {
    // In unit tests, InstallAttributes may not be initialized.
    // For enterprise test cases, it must be.
    CHECK_IS_TEST();
    return false;
  }
  return ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

}  // namespace user_manager
