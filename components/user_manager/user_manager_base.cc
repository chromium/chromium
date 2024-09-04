// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager_base.h"

#include <stddef.h>

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
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/crash/core/common/crash_key.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
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
  if (!stored_user_type || !stored_user_type->is_int())
    return UserType::kRegular;

  int int_user_type = stored_user_type->GetInt();
  if (int_user_type < 0 ||
      int_user_type > static_cast<int>(UserType::kMaxValue) ||
      int_user_type == 2) {
    LOG(ERROR) << "Bad user type " << int_user_type;
    return UserType::kRegular;
  }
  return static_cast<UserType>(int_user_type);
}

std::unique_ptr<UserImage> CreateStubImage() {
  return std::make_unique<user_manager::UserImage>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGIN_DEFAULT_USER));
}

}  // namespace

// static
const char UserManagerBase::kLegacySupervisedUsersHistogramName[] =
    "ChromeOS.LegacySupervisedUsers.HiddenFromLoginScreen";
// static
BASE_FEATURE(kRemoveLegacySupervisedUsersOnStartup,
             "RemoveLegacySupervisedUsersOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
const char UserManagerBase::kDeprecatedArcKioskUsersHistogramName[] =
    "Kiosk.DeprecatedArcKioskUsers";
// static
BASE_FEATURE(kRemoveDeprecatedArcKioskUsersOnStartup,
             "RemoveDeprecatedArcKioskUsersOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
void UserManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRegularUsersPref);
  registry->RegisterStringPref(prefs::kLastLoggedInGaiaUser, std::string());
  registry->RegisterDictionaryPref(prefs::kUserDisplayName);
  registry->RegisterDictionaryPref(prefs::kUserGivenName);
  registry->RegisterDictionaryPref(prefs::kUserDisplayEmail);
  registry->RegisterDictionaryPref(prefs::kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(prefs::kUserForceOnlineSignin);
  registry->RegisterDictionaryPref(prefs::kUserType);
  registry->RegisterStringPref(prefs::kLastActiveUser, std::string());
  registry->RegisterDictionaryPref(prefs::kOwnerAccount);

  registry->RegisterListPref(prefs::kDeviceLocalAccountsWithSavedData);
  registry->RegisterStringPref(prefs::kDeviceLocalAccountPendingDataRemoval,
                               std::string());

  UserDirectoryIntegrityManager::RegisterLocalStatePrefs(registry);
  KnownUser::RegisterPrefs(registry);
  MultiUserSignInPolicyController::RegisterPrefs(registry);
}

// static
void UserManagerBase::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMultiProfileUserBehaviorPref,
                               std::string(MultiUserSignInPolicyToPrefValue(
                                   MultiUserSignInPolicy::kUnrestricted)));
  registry->RegisterBooleanPref(
      prefs::kMultiProfileNeverShowIntro, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileWarningShowDismissed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

UserManagerBase::UserManagerBase(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    PrefService* local_state,
    ash::CrosSettings* cros_settings)
    : delegate_(std::move(delegate)),
      task_runner_(std::move(task_runner)),
      local_state_(local_state),
      cros_settings_(cros_settings),
      multi_user_sign_in_policy_controller_(local_state, this) {
  // |local_state| can be nullptr only for testing.
  if (!local_state) {
    CHECK_IS_TEST();
  }
  UpdateCrashKey(0, std::nullopt);
}

UserManagerBase::~UserManagerBase() = default;

void UserManagerBase::Shutdown() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
}

const UserList& UserManagerBase::GetUsers() const {
  return users_;
}

UserList UserManagerBase::GetUsersAllowedForMultiProfile() const {
  // Supervised users are not allowed to use multi-profiles.
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
  for (user_manager::User* user : GetUsers()) {
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

UserList UserManagerBase::FindLoginAllowedUsersFrom(
    const UserList& users) const {
  bool show_users_on_signin;
  cros_settings_->GetBoolean(ash::kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  UserList found_users;
  for (User* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskChromeAppManager and WebKioskAppManager.
    if (user->IsKioskType()) {
      continue;
    }
    const bool meets_allowlist_requirements =
        !user->HasGaiaAccount() || IsGaiaUserAllowed(*user);
    // Public session accounts are always shown on login screen.
    const bool meets_show_users_requirements =
        show_users_on_signin || user->GetType() == UserType::kPublicAccount;
    if (meets_allowlist_requirements && meets_show_users_requirements) {
      found_users.push_back(user);
    }
  }
  return found_users;
}

const UserList& UserManagerBase::GetLoggedInUsers() const {
  return logged_in_users_;
}

const UserList& UserManagerBase::GetLRULoggedInUsers() const {
  return lru_logged_in_users_;
}

UserList UserManagerBase::GetUnlockUsers() const {
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
      NOTREACHED_IN_MIGRATION()
          << "Spotted primary-only multi-user policy for non-primary user";
    }
  }

  return unlock_users;
}

const AccountId& UserManagerBase::GetOwnerAccountId() const {
  if (!owner_account_id_.has_value()) {
    return EmptyAccountId();
  }
  return *owner_account_id_;
}

void UserManagerBase::GetOwnerAccountIdAsync(
    base::OnceCallback<void(const AccountId&)> callback) const {
  if (owner_account_id_.has_value()) {
    std::move(callback).Run(*owner_account_id_);
    return;
  }
  pending_owner_callbacks_.AddUnsafe(std::move(callback));
}

const AccountId& UserManagerBase::GetLastSessionActiveAccountId() const {
  return last_session_active_account_id_;
}

void UserManagerBase::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash,
                                   bool browser_restart,
                                   bool is_child) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  if (!last_session_active_account_id_initialized_) {
    last_session_active_account_id_ = AccountId::FromUserEmail(
        local_state_->GetString(prefs::kLastActiveUser));
    last_session_active_account_id_initialized_ = true;
  }

  User* user = FindUserInListAndModify(account_id);

  const UserType user_type =
      CalculateUserType(account_id, user, browser_restart, is_child);
  if (active_user_ && user) {
    user->set_is_logged_in(true);
    user->set_username_hash(username_hash);
    logged_in_users_.push_back(user);
    lru_logged_in_users_.push_back(user);

    // Reset the new user flag if the user already exists.
    SetIsCurrentUserNew(false);
    UpdateCrashKey(logged_in_users_.size(), std::nullopt);
    SendMultiUserSignInMetrics();

    // Special case for user session restoration after browser crash.
    // We don't switch to each user session that has been restored as once all
    // session will be restored we'll switch to the session that has been used
    // before the crash.
    if (!delegate_->IsUserSessionRestoreInProgress()) {
      pending_user_switch_ = account_id;
    }
    NotifyUserAddedToSession(user);

    return;
  }

  switch (user_type) {
    case UserType::kRegular:
      [[fallthrough]];
    case UserType::kChild:
      if (account_id != GetOwnerAccountId() && !user &&
          (IsEphemeralAccountId(account_id) || browser_restart)) {
        RegularUserLoggedInAsEphemeral(account_id, user_type);
      } else {
        RegularUserLoggedIn(account_id, user_type);
      }
      break;

    case UserType::kGuest:
      GuestUserLoggedIn();
      break;

    case UserType::kPublicAccount:
      if (!user) {
        user = User::CreatePublicAccountUser(account_id);
        user_storage_.emplace_back(user);
      }
      PublicAccountUserLoggedIn(user);
      break;

    case UserType::kKioskApp:
    case UserType::kWebKioskApp:
      KioskAppLoggedIn(user);
      break;

    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled usert type " << user_type;
  }

  DCHECK(active_user_);
  active_user_->set_is_logged_in(true);
  active_user_->set_is_active(true);
  active_user_->set_username_hash(username_hash);

  logged_in_users_.push_back(active_user_.get());
  SetLRUUser(active_user_);

  if (!primary_user_) {
    primary_user_ = active_user_;
    delegate_->OverrideDirHome(*primary_user_);
    if (primary_user_->HasGaiaAccount())
      SendGaiaUserLoginMetrics(account_id);
  } else if (primary_user_ != active_user_) {
    // This is only needed for tests where a new user session is created
    // for non-existent user. The new user is created and automatically set
    // to active and there will be no pending user switch in such case.
    SetIsCurrentUserNew(true);
    NotifyUserAddedToSession(active_user_);
  }

  base::UmaHistogramEnumeration("UserManager.LoginUserType",
                                active_user_->GetType());

  UpdateCrashKey(logged_in_users_.size(), active_user_->GetType());

  local_state_->SetString(
      prefs::kLastLoggedInGaiaUser,
      active_user_->HasGaiaAccount() ? account_id.GetUserEmail() : "");

  delegate_->CheckProfileOnLogin(*active_user_);
  NotifyOnLogin();
}

void UserManagerBase::SwitchActiveUser(const AccountId& account_id) {
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
  active_user_->set_is_active(false);
  user->set_is_active(true);
  active_user_ = user;

  // Move the user to the front.
  SetLRUUser(active_user_);

  NotifyActiveUserChanged(active_user_);
  NotifyLoginStateUpdated();
}

void UserManagerBase::SwitchToLastActiveUser() {
  if (!last_session_active_account_id_.is_valid())
    return;

  if (AccountId::FromUserEmail(
          GetActiveUser()->GetAccountId().GetUserEmail()) !=
      last_session_active_account_id_)
    SwitchActiveUser(last_session_active_account_id_);

  // Make sure that this function gets run only once.
  last_session_active_account_id_.clear();
}

void UserManagerBase::OnSessionStarted() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  NotifyLoginStateUpdated();
  local_state_->CommitPendingWrite();
}

void UserManagerBase::RemoveUser(const AccountId& account_id,
                                 UserRemovalReason reason) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  UserDirectoryIntegrityManager integrity_manager(local_state_.get());
  // Misconfigured user would not be included in GetUsers(),
  // account for them separately.
  if (!CanUserBeRemoved(FindUser(account_id)) &&
      !integrity_manager.IsUserMisconfigured(account_id)) {
    return;
  }

  RemoveUserInternal(account_id, reason);
}

void UserManagerBase::RemoveUserInternal(const AccountId& account_id,
                                         UserRemovalReason reason) {
  auto callback =
      base::BindOnce(&UserManagerBase::RemoveUserInternal,
                     weak_factory_.GetWeakPtr(), account_id, reason);

  // Ensure the value of owner email has been fetched.
  if (cros_settings()->PrepareTrustedValues(std::move(callback)) !=
      ash::CrosSettingsProvider::TRUSTED) {
    // Value of owner email is not fetched yet.  RemoveUserInternal will be
    // called again after fetch completion.
    return;
  }
  std::string owner;
  cros_settings()->GetString(ash::kDeviceOwner, &owner);
  if (account_id == AccountId::FromUserEmail(owner)) {
    // Owner is not allowed to be removed from the device.
    return;
  }
  delegate_->RemoveProfileByAccountId(account_id);

  RemoveUserFromListImpl(account_id, reason,
                         /*trigger_cryptohome_removal=*/true);
}

void UserManagerBase::RemoveUserFromList(const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, UserRemovalReason::UNKNOWN,
                         /*trigger_cryptohome_removal=*/false);
}

void UserManagerBase::RemoveUserFromListForRecreation(
    const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, /*reason=*/std::nullopt,
                         /*trigger_cryptohome_removal=*/false);
}

void UserManagerBase::CleanStaleUserInformationFor(
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
void UserManagerBase::RemoveUserFromListImpl(
    AccountId account_id,
    std::optional<UserRemovalReason> reason,
    bool trigger_cryptohome_removal) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
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

bool UserManagerBase::IsKnownUser(const AccountId& account_id) const {
  // We check for the presence of a misconfigured user as well. This is because
  // `WallpaperControllerClientImpl::RemoveUserWallpaper` would not remove
  // the wallpaper prefs if we return false here, thus leaving behind
  // orphan prefs for the misconfigured users.
  UserDirectoryIntegrityManager integrity_manager(local_state_.get());
  return FindUser(account_id) != nullptr ||
         integrity_manager.IsUserMisconfigured(account_id);
}

const User* UserManagerBase::FindUser(const AccountId& account_id) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (active_user_ && active_user_->GetAccountId() == account_id)
    return active_user_;
  return FindUserInList(account_id);
}

User* UserManagerBase::FindUserAndModify(const AccountId& account_id) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (active_user_ && active_user_->GetAccountId() == account_id)
    return active_user_;
  return FindUserInListAndModify(account_id);
}

const User* UserManagerBase::GetActiveUser() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return active_user_;
}

User* UserManagerBase::GetActiveUser() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return active_user_;
}

const User* UserManagerBase::GetPrimaryUser() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return primary_user_;
}

void UserManagerBase::SaveUserOAuthStatus(
    const AccountId& account_id,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = FindUserAndModify(account_id);
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  {
    ScopedDictPrefUpdate oauth_status_update(local_state_.get(),
                                             prefs::kUserOAuthTokenStatus);
    oauth_status_update->Set(account_id.GetUserEmail(),
                             static_cast<int>(oauth_token_status));
  }
  local_state_->CommitPendingWrite();
}

void UserManagerBase::SaveForceOnlineSignin(const AccountId& account_id,
                                            bool force_online_signin) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  User* const user = FindUserAndModify(account_id);
  if (user)
    user->set_force_online_signin(force_online_signin);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  {
    ScopedDictPrefUpdate force_online_update(local_state_.get(),
                                             prefs::kUserForceOnlineSignin);
    force_online_update->Set(account_id.GetUserEmail(), force_online_signin);
  }
  local_state_->CommitPendingWrite();
}

void UserManagerBase::SaveUserDisplayName(const AccountId& account_id,
                                          const std::u16string& display_name) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

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

void UserManagerBase::SaveUserDisplayEmail(const AccountId& account_id,
                                           const std::string& display_email) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  User* user = FindUserAndModify(account_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
    return;  // Ignore if there is no such user.
  }

  user->set_display_email(display_email);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  ScopedDictPrefUpdate display_email_update(local_state_.get(),
                                            prefs::kUserDisplayEmail);
  display_email_update->Set(account_id.GetUserEmail(), display_email);
}

UserType UserManagerBase::GetUserType(const AccountId& account_id) {
  const base::Value::Dict& prefs_user_types =
      local_state_->GetDict(prefs::kUserType);
  return GetStoredUserType(prefs_user_types, account_id);
}

void UserManagerBase::SaveUserType(const User* user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  CHECK(user);
  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user->GetAccountId()))
    return;

  ScopedDictPrefUpdate user_type_update(local_state_.get(), prefs::kUserType);
  user_type_update->Set(user->GetAccountId().GetAccountIdKey(),
                        static_cast<int>(user->GetType()));
  local_state_->CommitPendingWrite();
}

void UserManagerBase::SetUserUsingSaml(const AccountId& account_id,
                                       bool using_saml,
                                       bool using_saml_principals_api) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

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

std::optional<std::string> UserManagerBase::GetOwnerEmail() {
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

void UserManagerBase::RecordOwner(const AccountId& owner) {
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

void UserManagerBase::UpdateUserAccountData(
    const AccountId& account_id,
    const UserAccountData& account_data) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

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

void UserManagerBase::ParseUserList(const base::Value::List& users_list,
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

bool UserManagerBase::IsOwnerUser(const User* user) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return user && owner_account_id_.has_value() &&
         user->GetAccountId() == *owner_account_id_;
}

bool UserManagerBase::IsPrimaryUser(const User* user) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return user && user == primary_user_;
}

bool UserManagerBase::IsEphemeralUser(const User* user) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (!user) {
    return false;
  }

  return IsEphemeralAccountId(user->GetAccountId());
}

bool UserManagerBase::IsCurrentUserOwner() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsOwnerUser(active_user_);
}

bool UserManagerBase::IsCurrentUserNew() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceFirstRunUI)) {
    return true;
  }

  return is_current_user_new_;
}

bool UserManagerBase::IsCurrentUserNonCryptohomeDataEphemeral() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() &&
         IsUserNonCryptohomeDataEphemeral(GetActiveUser()->GetAccountId());
}

bool UserManagerBase::IsCurrentUserCryptohomeDataEphemeral() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() &&
         IsUserCryptohomeDataEphemeral(GetActiveUser()->GetAccountId());
}

bool UserManagerBase::IsUserLoggedIn() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return active_user_;
}

bool UserManagerBase::IsLoggedInAsUserWithGaiaAccount() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->HasGaiaAccount();
}

bool UserManagerBase::IsLoggedInAsChildUser() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kChild;
}

bool UserManagerBase::IsLoggedInAsManagedGuestSession() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() &&
         active_user_->GetType() == UserType::kPublicAccount;
}

bool UserManagerBase::IsLoggedInAsGuest() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kGuest;
}

bool UserManagerBase::IsLoggedInAsKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kKioskApp;
}

bool UserManagerBase::IsLoggedInAsWebKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == UserType::kWebKioskApp;
}

bool UserManagerBase::IsLoggedInAsAnyKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->IsKioskType();
}

bool UserManagerBase::IsLoggedInAsStub() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetAccountId() == StubAccountId();
}

bool UserManagerBase::IsUserNonCryptohomeDataEphemeral(
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
  if (const User* user = FindUser(account_id);
      user && user->IsDeviceLocalAccount()) {
    return false;
  }

  // Data belonging to the currently logged-in user is ephemeral when:
  // a) The user logged into a regular gaia account while the ephemeral users
  //    policy was enabled.
  //    - or -
  // b) The user logged into any other account type.
  if (IsUserLoggedIn() && (account_id == GetActiveUser()->GetAccountId()) &&
      (is_current_user_ephemeral_regular_user_ ||
       !IsLoggedInAsUserWithGaiaAccount())) {
    return true;
  }

  // Data belonging to any other user is ephemeral when:
  // a) Going through the regular login flow and the ephemeral users policy is
  //    enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return IsEphemeralAccountId(account_id) || HasBrowserRestarted();
}

bool UserManagerBase::IsUserCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  return IsEphemeralAccountId(account_id);
}

bool UserManagerBase::IsEphemeralAccountId(const AccountId& account_id) const {
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
      ash::InstallAttributes::Get()->IsEnterpriseManaged() ||
      GetOwnerAccountId().is_valid();

  return device_is_owned &&
         GetEphemeralModeConfig().IsAccountIdIncluded(account_id);
}

void UserManagerBase::AddObserver(UserManager::Observer* obs) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  observer_list_.AddObserver(obs);
}

void UserManagerBase::RemoveObserver(UserManager::Observer* obs) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  observer_list_.RemoveObserver(obs);
}

void UserManagerBase::AddSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  session_state_observer_list_.AddObserver(obs);
}

void UserManagerBase::RemoveSessionStateObserver(
    UserManager::UserSessionStateObserver* obs) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  session_state_observer_list_.RemoveObserver(obs);
}

void UserManagerBase::NotifyLocalStateChanged() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.LocalStateChanged(this);
}

void UserManagerBase::NotifyUserImageChanged(const User& user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUserImageChanged(user);
}

void UserManagerBase::NotifyUserImageIsEnterpriseManagedChanged(
    const User& user,
    bool is_enterprise_managed) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_) {
    observer.OnUserImageIsEnterpriseManagedChanged(user, is_enterprise_managed);
  }
}

void UserManagerBase::NotifyUserProfileImageUpdateFailed(const User& user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUserProfileImageUpdateFailed(user);
}

void UserManagerBase::NotifyUserProfileImageUpdated(
    const User& user,
    const gfx::ImageSkia& profile_image) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUserProfileImageUpdated(user, profile_image);
}

void UserManagerBase::NotifyUsersSignInConstraintsChanged() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUsersSignInConstraintsChanged();
}

void UserManagerBase::NotifyUserAffiliationUpdated(const User& user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_) {
    observer.OnUserAffiliationUpdated(user);
  }
}

void UserManagerBase::NotifyUserToBeRemoved(const AccountId& account_id) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUserToBeRemoved(account_id);
}

void UserManagerBase::NotifyUserRemoved(const AccountId& account_id,
                                        UserRemovalReason reason) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_)
    observer.OnUserRemoved(account_id, reason);
}

void UserManagerBase::NotifyUserNotAllowed(const std::string& user_email) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observer_list_) {
    observer.OnUserNotAllowed(user_email);
  }
}

bool UserManagerBase::IsGuestSessionAllowed() const {
  // In tests CrosSettings might not be initialized.
  if (!cros_settings()) {
    return false;
  }

  bool is_guest_allowed = false;
  cros_settings()->GetBoolean(ash::kAccountsPrefAllowGuest, &is_guest_allowed);
  return is_guest_allowed;
}

bool UserManagerBase::IsGaiaUserAllowed(const User& user) const {
  DCHECK(user.HasGaiaAccount());
  return cros_settings()->IsUserAllowlisted(user.GetAccountId().GetUserEmail(),
                                            nullptr, user.GetType());
}

bool UserManagerBase::IsUserAllowed(const User& user) const {
  DCHECK(user.GetType() == UserType::kRegular ||
         user.GetType() == UserType::kGuest ||
         user.GetType() == UserType::kChild);

  return UserManager::IsUserAllowed(
      user, IsGuestSessionAllowed(),
      user.HasGaiaAccount() && IsGaiaUserAllowed(user));
}

bool UserManagerBase::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         kSupervisedUserDomain;
}

bool UserManagerBase::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return account_id == AccountId::FromUserEmail(GetLocalState()->GetString(
                           prefs::kDeviceLocalAccountPendingDataRemoval));
}

bool UserManagerBase::CanUserBeRemoved(const User* user) const {
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
  if (users_.size() < 2 &&
      !ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }

  // Sanity check: do not allow any of the the logged in users to be removed.
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end();
       ++it) {
    if ((*it)->GetAccountId() == user->GetAccountId())
      return false;
  }

  return true;
}

const UserManagerBase::EphemeralModeConfig&
UserManagerBase::GetEphemeralModeConfig() const {
  return ephemeral_mode_config_;
}

void UserManagerBase::SetEphemeralModeConfig(
    EphemeralModeConfig ephemeral_mode_config) {
  ephemeral_mode_config_ = std::move(ephemeral_mode_config);
}

void UserManagerBase::SetIsCurrentUserNew(bool is_new) {
  is_current_user_new_ = is_new;
}

void UserManagerBase::ResetOwnerId() {
  owner_account_id_ = std::nullopt;
}

void UserManagerBase::SetOwnerId(const AccountId& owner_account_id) {
  owner_account_id_ = owner_account_id;
  pending_owner_callbacks_.Notify(owner_account_id);
  NotifyLoginStateUpdated();
}

void UserManagerBase::ProcessPendingUserSwitchId() {
  if (pending_user_switch_.is_valid()) {
    SwitchActiveUser(std::exchange(pending_user_switch_, EmptyAccountId()));
  }
}

void UserManagerBase::EnsureUsersLoaded() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
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
    users_.push_back(user);
  }

  for (user_manager::User* user : users_) {
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

void UserManagerBase::LoadDeviceLocalAccounts(
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

    auto type =
        delegate_->GetDeviceLocalAccountUserType(account_id.GetUserEmail());
    if (!type.has_value()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    // Using `new` to access a non-public constructor.
    user_storage_.push_back(base::WrapUnique(new User(account_id, *type)));
    users_.push_back(user_storage_.back().get());
  }
}

UserList& UserManagerBase::GetUsersAndModify() {
  return users_;
}

const User* UserManagerBase::FindUserInList(const AccountId& account_id) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      return *it;
  }
  return nullptr;
}

bool UserManagerBase::UserExistsInList(const AccountId& account_id) const {
  const base::Value::List& user_list =
      local_state_->GetList(prefs::kRegularUsersPref);
  for (const base::Value& i : user_list) {
    const std::string* email = i.GetIfString();
    if (email && (account_id.GetUserEmail() == *email))
      return true;
  }
  return false;
}

User* UserManagerBase::FindUserInListAndModify(const AccountId& account_id) {
  UserList& users = GetUsersAndModify();
  for (UserList::iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      return *it;
  }
  return nullptr;
}

void UserManagerBase::GuestUserLoggedIn() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  auto* user = User::CreateGuestUser(GuestAccountId());
  user->SetStubImage(CreateStubImage(), UserImage::Type::kInvalid,
                     /*is_loading=*/false);
  user_storage_.emplace_back(user);
  active_user_ = user;
}

void UserManagerBase::AddUserRecord(User* user) {
  // Add the user to the front of the user list.
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          prefs::kRegularUsersPref);
  prefs_users_update->Insert(prefs_users_update->begin(),
                             base::Value(user->GetAccountId().GetUserEmail()));
  users_.insert(users_.begin(), user);
}

void UserManagerBase::RegularUserLoggedIn(const AccountId& account_id,
                                          const UserType user_type) {
  // Remove the user from the user list.
  active_user_ =
      RemoveRegularOrSupervisedUserFromList(account_id, false /* notify */);
  KnownUser known_user(local_state_.get());

  if (active_user_ && active_user_->GetType() != user_type) {
    active_user_->UpdateType(user_type);
    // Clear information about profile policy requirements to enforce setting it
    // again for the new account type.
    known_user.ClearProfileRequiresPolicy(account_id);
  }

  // If the user was not found on the user list, create a new user.
  SetIsCurrentUserNew(!active_user_);
  if (IsCurrentUserNew()) {
    auto* user = User::CreateRegularUser(account_id, user_type);
    user_storage_.emplace_back(user);
    active_user_ = user;
    SaveUserType(active_user_);

    active_user_->set_oauth_token_status(LoadUserOAuthStatus(account_id));
    SaveUserDisplayName(active_user_->GetAccountId(),
                        base::UTF8ToUTF16(active_user_->GetAccountName(true)));
  } else {
    SaveUserType(active_user_);
  }

  AddUserRecord(active_user_);
  known_user.SetIsEphemeralUser(active_user_->GetAccountId(), false);

  // Make sure that new data is persisted to Local State.
  local_state_->CommitPendingWrite();
}

void UserManagerBase::RegularUserLoggedInAsEphemeral(
    const AccountId& account_id,
    const UserType user_type) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  SetIsCurrentUserNew(true);
  is_current_user_ephemeral_regular_user_ = true;
  auto* user = User::CreateRegularUser(account_id, user_type);
  user_storage_.emplace_back(user);
  active_user_ = user;
  KnownUser(local_state_.get())
      .SetIsEphemeralUser(active_user_->GetAccountId(), true);
}

void UserManagerBase::PublicAccountUserLoggedIn(user_manager::User* user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  SetIsCurrentUserNew(true);
  active_user_ = user;
}

void UserManagerBase::KioskAppLoggedIn(user_manager::User* user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  user->SetStubImage(CreateStubImage(), UserImage::Type::kInvalid,
                     /*is_loading=*/false);
  active_user_ = user;
}

bool UserManagerBase::OnUserProfileCreated(const AccountId& account_id,
                                           PrefService* prefs) {
  // Find a User from `user_storage_`.
  // FindUserAndModify may overlook some existing User instance, because
  // the list may not contain ephemeral users that are getting stale.
  auto it = base::ranges::find(user_storage_, account_id,
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

  CHECK(!user->GetProfilePrefs());
  user->SetProfileIsCreated();
  user->SetProfilePrefs(prefs);

  if (IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsAnyKioskApp()) {
    multi_user_sign_in_policy_controller_.StartObserving(user);
  }

  for (auto& observer : observer_list_) {
    observer.OnUserProfileCreated(*user);
  }
  return true;
}

void UserManagerBase::OnUserProfileWillBeDestroyed(
    const AccountId& account_id) {
  // Find from user_stroage_. See OnUserProfileCreated for the reason why not
  // using FindUserAndModify.
  auto it = base::ranges::find(user_storage_, account_id,
                               [](auto& ptr) { return ptr->GetAccountId(); });
  auto* user = it == user_storage_.end() ? nullptr : it->get();
  CHECK(user);

  multi_user_sign_in_policy_controller_.StopObserving(user);

  user->SetProfilePrefs(nullptr);
}

void UserManagerBase::NotifyActiveUserChanged(User* active_user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : session_state_observer_list_)
    observer.ActiveUserChanged(active_user);
}

void UserManagerBase::NotifyLoginStateUpdated() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : session_state_observer_list_) {
    observer.OnLoginStateUpdated(active_user_);
  }
}

void UserManagerBase::NotifyOnLogin() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  DCHECK(active_user_);

  for (auto& observer : observer_list_) {
    observer.OnUserLoggedIn(*active_user_);
  }

  NotifyActiveUserChanged(active_user_);
  NotifyLoginStateUpdated();
}

User::OAuthTokenStatus UserManagerBase::LoadUserOAuthStatus(
    const AccountId& account_id) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  const base::Value::Dict& prefs_oauth_status =
      local_state_->GetDict(prefs::kUserOAuthTokenStatus);

  std::optional<int> oauth_token_status =
      prefs_oauth_status.FindInt(account_id.GetUserEmail());
  if (!oauth_token_status.has_value())
    return User::OAUTH_TOKEN_STATUS_UNKNOWN;

  return static_cast<User::OAuthTokenStatus>(oauth_token_status.value());
}

bool UserManagerBase::LoadForceOnlineSignin(const AccountId& account_id) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  const base::Value::Dict& prefs_force_online =
      local_state_->GetDict(prefs::kUserForceOnlineSignin);

  return prefs_force_online.FindBool(account_id.GetUserEmail()).value_or(false);
}

void UserManagerBase::RemoveNonCryptohomeData(const AccountId& account_id) {
  multi_user_sign_in_policy_controller_.RemoveCachedValues(
      account_id.GetUserEmail());

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

User* UserManagerBase::RemoveRegularOrSupervisedUserFromList(
    const AccountId& account_id,
    bool notify) {
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          prefs::kRegularUsersPref);
  prefs_users_update->clear();
  User* user = nullptr;
  for (UserList::iterator it = users_.begin(); it != users_.end();) {
    if ((*it)->GetAccountId() == account_id) {
      user = *it;
      it = users_.erase(it);
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

void UserManagerBase::NotifyUserAddedToSession(const User* added_user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : session_state_observer_list_) {
    observer.UserAddedToSession(added_user);
  }
}

PrefService* UserManagerBase::GetLocalState() const {
  return local_state_.get();
}

bool UserManagerBase::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kFirstExecAfterBoot);
}

void UserManagerBase::SetUserAffiliated(const AccountId& account_id,
                                        bool is_affiliated) {
  User* user = FindUserAndModify(account_id);
  if (!user) {
    return;
  }
  user->SetAffiliated(is_affiliated);
  NotifyUserAffiliationUpdated(*user);
}

bool UserManagerBase::HasBrowserRestarted() const {
  return base::SysInfo::IsRunningOnChromeOS() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kLoginUser);
}

MultiUserSignInPolicyController*
UserManagerBase::GetMultiUserSignInPolicyController() {
  return &multi_user_sign_in_policy_controller_;
}

void UserManagerBase::Initialize() {
  UserManager::Initialize();
  if (!HasBrowserRestarted()) {
    // local_state may be null in unit tests.
    if (local_state_) {
      KnownUser known_user(local_state_.get());
      known_user.CleanEphemeralUsers();
      known_user.CleanObsoletePrefs();
    }
  }
  EnsureUsersLoaded();
  NotifyLoginStateUpdated();
}

const User* UserManagerBase::AddKioskAppUserForTesting(
    const AccountId& account_id,
    const std::string& username_hash) {
  User* user = User::CreateKioskAppUser(account_id);
  user->set_username_hash(username_hash);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

void UserManagerBase::SetLRUUser(User* user) {
  local_state_->SetString(prefs::kLastActiveUser,
                          user->GetAccountId().GetUserEmail());
  local_state_->CommitPendingWrite();

  UserList::iterator it = base::ranges::find(lru_logged_in_users_, user);
  if (it != lru_logged_in_users_.end())
    lru_logged_in_users_.erase(it);
  lru_logged_in_users_.insert(lru_logged_in_users_.begin(), user);
}

void UserManagerBase::UpdateCrashKey(int num_users,
                                     std::optional<UserType> active_user_type) {
  static crash_reporter::CrashKeyString<64> crash_key("num-users");
  crash_key.Set(base::NumberToString(GetLoggedInUsers().size()));

  static crash_reporter::CrashKeyString<32> session_type("session-type");
  if (active_user_type.has_value()) {
    session_type.Set(UserTypeToString(active_user_type.value()));
  }
}

void UserManagerBase::SendGaiaUserLoginMetrics(const AccountId& account_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (IsFirstExecAfterBoot())
    return;

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

void UserManagerBase::SendMultiUserSignInMetrics() {
  size_t users = logged_in_users_.size();
  if (!users) {
    return;
  }

  // Write the user number as UMA stat when a multi user session is possible.
  if (users + GetUsersAllowedForMultiProfile().size() > 1) {
    UMA_HISTOGRAM_COUNTS_100("MultiProfile.UsersPerSessionIncremental", users);
  }
}

void UserManagerBase::UpdateUserAccountLocale(const AccountId& account_id,
                                              const std::string& locale) {
  if (!locale.empty() && locale != delegate_->GetApplicationLocale()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](const std::string& locale) {
              std::string resolved_locale;
              std::ignore =
                  l10n_util::CheckAndResolveLocale(locale, &resolved_locale);
              return resolved_locale;
            },
            locale),
        base::BindOnce(&UserManagerBase::DoUpdateAccountLocale,
                       weak_factory_.GetWeakPtr(), account_id));
  } else {
    DoUpdateAccountLocale(account_id, locale);
  }
}

void UserManagerBase::DoUpdateAccountLocale(
    const AccountId& account_id,
    const std::string& resolved_locale) {
  User* user = FindUserAndModify(account_id);
  if (user) {
    user->SetAccountLocale(resolved_locale);
  }
}

void UserManagerBase::DeleteUser(User* user) {
  if (active_user_ == user) {
    active_user_ = nullptr;
  }
  if (primary_user_ == user) {
    primary_user_ = nullptr;
  }
  std::erase(users_, user);
  std::erase(logged_in_users_, user);
  std::erase(lru_logged_in_users_, user);

  std::erase_if(user_storage_, [user](auto& ptr) { return ptr.get() == user; });
}

// TODO(crbug.com/40755604): Remove dormant legacy supervised user cryptohomes.
// After we have enough confidence that there are no more supervised users on
// devices in the wild, remove this.
void UserManagerBase::RemoveLegacySupervisedUser(const AccountId& account_id) {
  DCHECK(IsDeprecatedSupervisedAccountId(account_id));
  if (base::FeatureList::IsEnabled(kRemoveLegacySupervisedUsersOnStartup)) {
    // Since we skip adding legacy supervised users to the users list,
    // FindUser(account_id) returns nullptr and CanUserBeRemoved() returns
    // false. This is why we call RemoveUserInternal() directly instead of
    // RemoveUser().
    RemoveUserInternal(account_id, UserRemovalReason::UNKNOWN);
    base::UmaHistogramEnumeration(kLegacySupervisedUsersHistogramName,
                                  LegacySupervisedUserStatus::kLSUDeleted);
  } else {
    base::UmaHistogramEnumeration(kLegacySupervisedUsersHistogramName,
                                  LegacySupervisedUserStatus::kLSUHidden);
  }
}

bool UserManagerBase::IsDeprecatedArcKioskAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) == kArcKioskDomain;
}

// TODO(b/355590943): Remove dormant deprecated ARC kiosk user cryptohomes.
// Remove this once confident that all ARC kiosk cryptohomes are cleaned up.
void UserManagerBase::RemoveDeprecatedArcKioskUser(
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

}  // namespace user_manager
