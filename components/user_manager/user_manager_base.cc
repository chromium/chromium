// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager_base.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/crash/core/common/crash_key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {
namespace {

// A dictionary that maps user IDs to the displayed name.
const char kUserDisplayName[] = "UserDisplayName";

// A dictionary that maps user IDs to the user's given name.
const char kUserGivenName[] = "UserGivenName";

// A dictionary that maps user IDs to the displayed (non-canonical) emails.
const char kUserDisplayEmail[] = "UserDisplayEmail";

// A dictionary that maps user IDs to OAuth token presence flag.
const char kUserOAuthTokenStatus[] = "OAuthTokenStatus";

// A dictionary that maps user IDs to a flag indicating whether online
// authentication against GAIA should be enforced during the next sign-in.
const char kUserForceOnlineSignin[] = "UserForceOnlineSignin";

// A dictionary that maps user ID to the user type.
const char kUserType[] = "UserType";

// A string pref containing the ID of the last user who logged in if it was
// a user with gaia account (regular) or an empty string if it was another type
// of user (guest, kiosk, public account, etc.).
const char kLastLoggedInGaiaUser[] = "LastLoggedInRegularUser";

// A string pref containing the ID of the last active user.
// In case of browser crash, this pref will be used to set active user after
// session restore.
const char kLastActiveUser[] = "LastActiveUser";

// Upper bound for a histogram metric reporting the amount of time between
// one regular user logging out and a different regular user logging in.
const int kLogoutToLoginDelayMaxSec = 1800;

// Stores a dictionary that describes who is the owner user of the device.
// If present, currently always contains "type": 1 (i.e. kGoogleEmail) and
// "account" that holds of the email of the owner user.
const char kOwnerAccount[] = "owner.account";
// Inner fields for the kOwnerAccount dict.
constexpr char kOwnerAccountType[] = "type";
constexpr char kOwnerAccountIdentity[] = "account";

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
    return USER_TYPE_REGULAR;

  int int_user_type = stored_user_type->GetInt();
  if (int_user_type < 0 || int_user_type >= NUM_USER_TYPES ||
      int_user_type == 2) {
    LOG(ERROR) << "Bad user type " << int_user_type;
    return USER_TYPE_REGULAR;
  }
  return static_cast<UserType>(int_user_type);
}

std::string UserTypeToString(UserType user_type) {
  switch (user_type) {
    case USER_TYPE_REGULAR:
      return "regular";
    case USER_TYPE_CHILD:
      return "child";
    case USER_TYPE_GUEST:
      return "guest";
    case USER_TYPE_PUBLIC_ACCOUNT:
      return "managed-guest-session";
    case USER_TYPE_KIOSK_APP:
      return "chrome-app-kiosk";
    case USER_TYPE_ARC_KIOSK_APP:
      return "arc-kiosk";
    case USER_TYPE_WEB_KIOSK_APP:
      return "web-kiosk";
    case USER_TYPE_ACTIVE_DIRECTORY:
      return "active-directory";
    case NUM_USER_TYPES:
      NOTREACHED();
      return "";
  }
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
void UserManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRegularUsersPref);
  registry->RegisterStringPref(kLastLoggedInGaiaUser, std::string());
  registry->RegisterDictionaryPref(kUserDisplayName);
  registry->RegisterDictionaryPref(kUserGivenName);
  registry->RegisterDictionaryPref(kUserDisplayEmail);
  registry->RegisterDictionaryPref(kUserOAuthTokenStatus);
  registry->RegisterDictionaryPref(kUserForceOnlineSignin);
  registry->RegisterDictionaryPref(kUserType);
  registry->RegisterStringPref(kLastActiveUser, std::string());
  registry->RegisterDictionaryPref(kOwnerAccount);

  UserDirectoryIntegrityManager::RegisterLocalStatePrefs(registry);
  KnownUser::RegisterPrefs(registry);
}

UserManagerBase::UserManagerBase(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    PrefService* local_state)
    : task_runner_(std::move(task_runner)), local_state_(local_state) {
  // |local_state| can be nullptr only for testing.
  if (!local_state) {
    CHECK_IS_TEST();
  }
}

UserManagerBase::~UserManagerBase() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (UserList::iterator it = users_.begin(); it != users_.end();
       it = users_.erase(it)) {
    DeleteUser(*it);
  }
  // These are pointers to the same User instances that were in users_ list.
  logged_in_users_.clear();
  lru_logged_in_users_.clear();

  DeleteUser(active_user_);
}

void UserManagerBase::Shutdown() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
}

const UserList& UserManagerBase::GetUsers() const {
  const_cast<UserManagerBase*>(this)->EnsureUsersLoaded();
  return users_;
}

const UserList& UserManagerBase::GetLoggedInUsers() const {
  return logged_in_users_;
}

const UserList& UserManagerBase::GetLRULoggedInUsers() const {
  return lru_logged_in_users_;
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
    last_session_active_account_id_ =
        AccountId::FromUserEmail(local_state_->GetString(kLastActiveUser));
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
    NotifyUserAddedToSession(user, true /* user switch pending */);

    return;
  }

  switch (user_type) {
    case USER_TYPE_REGULAR:  // fallthrough
    case USER_TYPE_CHILD:    // fallthrough
    case USER_TYPE_ACTIVE_DIRECTORY:
      if (account_id != GetOwnerAccountId() && !user &&
          (IsEphemeralAccountId(account_id) || browser_restart)) {
        RegularUserLoggedInAsEphemeral(account_id, user_type);
      } else {
        RegularUserLoggedIn(account_id, user_type);
      }
      break;

    case USER_TYPE_GUEST:
      GuestUserLoggedIn();
      break;

    case USER_TYPE_PUBLIC_ACCOUNT:
      PublicAccountUserLoggedIn(
          user ? user : User::CreatePublicAccountUser(account_id));
      break;

    case USER_TYPE_KIOSK_APP:
    case USER_TYPE_ARC_KIOSK_APP:
    case USER_TYPE_WEB_KIOSK_APP:
      KioskAppLoggedIn(user);
      break;

    default:
      NOTREACHED() << "Unhandled usert type " << user_type;
  }

  DCHECK(active_user_);
  active_user_->set_is_logged_in(true);
  active_user_->set_is_active(true);
  active_user_->set_username_hash(username_hash);

  logged_in_users_.push_back(active_user_);
  SetLRUUser(active_user_);

  if (!primary_user_) {
    primary_user_ = active_user_;
    if (primary_user_->HasGaiaAccount())
      SendGaiaUserLoginMetrics(account_id);
  } else if (primary_user_ != active_user_) {
    // This is only needed for tests where a new user session is created
    // for non-existent user. The new user is created and automatically set
    // to active and there will be no pending user switch in such case.
    SetIsCurrentUserNew(true);
    NotifyUserAddedToSession(active_user_, false /* user switch pending */);
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UserManager.LoginUserType", active_user_->GetType(), NUM_USER_TYPES);

  static crash_reporter::CrashKeyString<32> session_type("session-type");
  session_type.Set(UserTypeToString(active_user_->GetType()));

  local_state_->SetString(kLastLoggedInGaiaUser, active_user_->HasGaiaAccount()
                                                     ? account_id.GetUserEmail()
                                                     : "");

  NotifyOnLogin();
  PerformPostUserLoggedInActions(browser_restart);
}

void UserManagerBase::SwitchActiveUser(const AccountId& account_id) {
  User* user = FindUserAndModify(account_id);
  if (!user) {
    NOTREACHED() << "Switching to a non-existing user";
    return;
  }
  if (user == active_user_) {
    NOTREACHED() << "Switching to a user who is already active";
    return;
  }
  if (!user->is_logged_in()) {
    NOTREACHED() << "Switching to a user that is not logged in";
    return;
  }
  if (!user->HasGaiaAccount()) {
    NOTREACHED() <<
        "Switching to a user without gaia account (non-regular one)";
    return;
  }
  if (user->username_hash().empty()) {
    NOTREACHED() << "Switching to a user that doesn't have username_hash set";
    return;
  }

  DCHECK(active_user_);
  active_user_->set_is_active(false);
  user->set_is_active(true);
  active_user_ = user;

  // Move the user to the front.
  SetLRUUser(active_user_);

  NotifyActiveUserChanged(active_user_);
  CallUpdateLoginState();
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

  CallUpdateLoginState();
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
  RemoveNonOwnerUserInternal(account_id, reason);
}

void UserManagerBase::RemoveNonOwnerUserInternal(AccountId account_id,
                                                 UserRemovalReason reason) {
  RemoveUserFromListImpl(account_id, reason,
                         /*trigger_cryptohome_removal=*/true);
}

void UserManagerBase::RemoveUserFromList(const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, UserRemovalReason::UNKNOWN,
                         /*trigger_cryptohome_removal=*/false);
}

void UserManagerBase::RemoveUserFromListForRecreation(
    const AccountId& account_id) {
  RemoveUserFromListImpl(account_id, /*reason=*/absl::nullopt,
                         /*trigger_cryptohome_removal=*/false);
}

void UserManagerBase::RemoveUserFromListImpl(
    const AccountId& account_id,
    absl::optional<UserRemovalReason> reason,
    bool trigger_cryptohome_removal) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (reason.has_value()) {
    NotifyUserToBeRemoved(account_id);
  }
  if (trigger_cryptohome_removal) {
    AsyncRemoveCryptohome(account_id);
  }

  RemoveNonCryptohomeData(account_id);
  KnownUser(local_state_.get()).RemovePrefs(account_id);
  if (user_loading_stage_ == STAGE_LOADED) {
    // After the User object is deleted from memory in DeleteUser() here,
    // the account_id reference will be invalid if the reference points
    // to the account_id in the User object.
    DeleteUser(
        RemoveRegularOrSupervisedUserFromList(account_id, reason.has_value()));
  } else {
    NOTREACHED() << "Users are not loaded yet.";
    return;
  }

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
                                             kUserOAuthTokenStatus);
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
                                             kUserForceOnlineSignin);
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
                                               kUserDisplayName);
      display_name_update->Set(account_id.GetUserEmail(), display_name);
    }
  }
}

std::u16string UserManagerBase::GetUserDisplayName(
    const AccountId& account_id) const {
  const User* user = FindUser(account_id);
  return user ? user->display_name() : std::u16string();
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
                                            kUserDisplayEmail);
  display_email_update->Set(account_id.GetUserEmail(), display_email);
}

UserType UserManagerBase::GetUserType(const AccountId& account_id) {
  const base::Value::Dict& prefs_user_types = local_state_->GetDict(kUserType);
  return GetStoredUserType(prefs_user_types, account_id);
}

void UserManagerBase::SaveUserType(const User* user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  CHECK(user);
  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (IsUserNonCryptohomeDataEphemeral(user->GetAccountId()))
    return;

  ScopedDictPrefUpdate user_type_update(local_state_.get(), kUserType);
  user_type_update->Set(user->GetAccountId().GetAccountIdKey(),
                        static_cast<int>(user->GetType()));
  local_state_->CommitPendingWrite();
}

absl::optional<std::string> UserManagerBase::GetOwnerEmail() {
  const base::Value::Dict& owner = local_state_->GetDict(kOwnerAccount);
  absl::optional<int> type = owner.FindInt(kOwnerAccountType);
  if (!type.has_value() || (static_cast<OwnerAccountType>(type.value())) !=
                               OwnerAccountType::kGoogleEmail) {
    return absl::nullopt;
  }

  const std::string* email = owner.FindString(kOwnerAccountIdentity);
  if (!email) {
    return absl::nullopt;
  }
  return *email;
}

void UserManagerBase::RecordOwner(const AccountId& owner) {
  base::Value::Dict owner_dict;
  owner_dict.Set(kOwnerAccountType,
                 static_cast<int>(OwnerAccountType::kGoogleEmail));
  owner_dict.Set(kOwnerAccountIdentity, owner.GetUserEmail());
  local_state_->SetDict(kOwnerAccount, std::move(owner_dict));
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
                                             kUserGivenName);
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
  return owner_account_id_.has_value() && active_user_ &&
         active_user_->GetAccountId() == *owner_account_id_;
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

bool UserManagerBase::CanCurrentUserLock() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->can_lock();
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
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_CHILD;
}

bool UserManagerBase::IsLoggedInAsPublicAccount() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() &&
         active_user_->GetType() == USER_TYPE_PUBLIC_ACCOUNT;
}

bool UserManagerBase::IsLoggedInAsGuest() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_GUEST;
}

bool UserManagerBase::IsLoggedInAsKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_KIOSK_APP;
}

bool UserManagerBase::IsLoggedInAsArcKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_ARC_KIOSK_APP;
}

bool UserManagerBase::IsLoggedInAsWebKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->GetType() == USER_TYPE_WEB_KIOSK_APP;
}

bool UserManagerBase::IsLoggedInAsAnyKioskApp() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && active_user_->IsKioskType();
}

bool UserManagerBase::IsLoggedInAsStub() const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  return IsUserLoggedIn() && IsStubAccountId(active_user_->GetAccountId());
}

bool UserManagerBase::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  // Data belonging to the guest and stub users is always ephemeral.
  if (IsGuestAccountId(account_id) || IsStubAccountId(account_id))
    return true;

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
  if (IsStubAccountId(account_id)) {
    return false;
  }

  // Data belonging to the guest user is always ephemeral.
  if (IsGuestAccountId(account_id)) {
    return true;
  }

  // Data belonging to the public accounts (e.g. managed guest sessions) is
  // always ephemeral.
  if (const User* user = FindUser(account_id);
      user && user->GetType() == USER_TYPE_PUBLIC_ACCOUNT) {
    return true;
  }

  return IsEphemeralAccountIdByPolicy(account_id);
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

bool UserManagerBase::CanUserBeRemoved(const User* user) const {
  // Only regular users are allowed to be manually removed.
  if (!user || !(user->HasGaiaAccount() || user->IsActiveDirectoryUser()))
    return false;

  // Sanity check: we must not remove single user unless it's an enterprise
  // device. This check may seem redundant at a first sight because
  // this single user must be an owner and we perform special check later
  // in order not to remove an owner. However due to non-instant nature of
  // ownership assignment this later check may sometimes fail.
  // See http://crosbug.com/12723
  if (users_.size() < 2 && !IsEnterpriseManaged())
    return false;

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
  owner_account_id_ = absl::nullopt;
}

void UserManagerBase::SetOwnerId(const AccountId& owner_account_id) {
  owner_account_id_ = owner_account_id;
  pending_owner_callbacks_.Notify(owner_account_id);
  CallUpdateLoginState();
}

const AccountId& UserManagerBase::GetPendingUserSwitchID() const {
  return pending_user_switch_;
}

void UserManagerBase::SetPendingUserSwitchId(const AccountId& account_id) {
  pending_user_switch_ = account_id;
}

void UserManagerBase::EnsureUsersLoaded() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  if (!local_state_) {
    return;
  }

  if (user_loading_stage_ != STAGE_NOT_LOADED)
    return;
  user_loading_stage_ = STAGE_LOADING;

  const base::Value::List& prefs_regular_users =
      local_state_->GetList(kRegularUsersPref);

  const base::Value::Dict& prefs_display_names =
      local_state_->GetDict(kUserDisplayName);
  const base::Value::Dict& prefs_given_names =
      local_state_->GetDict(kUserGivenName);
  const base::Value::Dict& prefs_display_emails =
      local_state_->GetDict(kUserDisplayEmail);
  const base::Value::Dict& prefs_user_types = local_state_->GetDict(kUserType);

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
    users_.push_back(user);
  }

  for (auto* user : users_) {
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
  user_loading_stage_ = STAGE_LOADED;

  for (auto& observer : observer_list_) {
    observer.OnUserListLoaded();
  }
}

UserList& UserManagerBase::GetUsersAndModify() {
  EnsureUsersLoaded();
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
  const base::Value::List& user_list = local_state_->GetList(kRegularUsersPref);
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
  active_user_ = User::CreateGuestUser(GetGuestAccountId());
}

void UserManagerBase::AddUserRecord(User* user) {
  // Add the user to the front of the user list.
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          kRegularUsersPref);
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
    active_user_ = User::CreateRegularUser(account_id, user_type);
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
  active_user_ = User::CreateRegularUser(account_id, user_type);
  KnownUser(local_state_.get())
      .SetIsEphemeralUser(active_user_->GetAccountId(), true);
}

void UserManagerBase::NotifyActiveUserChanged(User* active_user) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : session_state_observer_list_)
    observer.ActiveUserChanged(active_user);
}

void UserManagerBase::NotifyOnLogin() {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  DCHECK(active_user_);

  // TODO(b/278643115): Call Observer::OnUserLoggedIn() from here.

  NotifyActiveUserChanged(active_user_);
  CallUpdateLoginState();
}

User::OAuthTokenStatus UserManagerBase::LoadUserOAuthStatus(
    const AccountId& account_id) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  const base::Value::Dict& prefs_oauth_status =
      local_state_->GetDict(kUserOAuthTokenStatus);

  absl::optional<int> oauth_token_status =
      prefs_oauth_status.FindInt(account_id.GetUserEmail());
  if (!oauth_token_status.has_value())
    return User::OAUTH_TOKEN_STATUS_UNKNOWN;

  return static_cast<User::OAuthTokenStatus>(oauth_token_status.value());
}

bool UserManagerBase::LoadForceOnlineSignin(const AccountId& account_id) const {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());

  const base::Value::Dict& prefs_force_online =
      local_state_->GetDict(kUserForceOnlineSignin);

  return prefs_force_online.FindBool(account_id.GetUserEmail()).value_or(false);
}

void UserManagerBase::RemoveNonCryptohomeData(const AccountId& account_id) {
  ScopedDictPrefUpdate(local_state_.get(), kUserDisplayName)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), kUserGivenName)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), kUserDisplayEmail)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), kUserOAuthTokenStatus)
      ->Remove(account_id.GetUserEmail());

  ScopedDictPrefUpdate(local_state_.get(), kUserForceOnlineSignin)
      ->Remove(account_id.GetUserEmail());

  KnownUser(local_state_.get()).RemovePrefs(account_id);

  const AccountId last_active_user =
      AccountId::FromUserEmail(local_state_->GetString(kLastActiveUser));
  if (account_id == last_active_user) {
    local_state_->SetString(kLastActiveUser, std::string());
  }
}

User* UserManagerBase::RemoveRegularOrSupervisedUserFromList(
    const AccountId& account_id,
    bool notify) {
  ScopedListPrefUpdate prefs_users_update(local_state_.get(),
                                          kRegularUsersPref);
  prefs_users_update->clear();
  User* user = nullptr;
  for (UserList::iterator it = users_.begin(); it != users_.end();) {
    if ((*it)->GetAccountId() == account_id) {
      user = *it;
      it = users_.erase(it);
    } else {
      if ((*it)->HasGaiaAccount() || (*it)->IsActiveDirectoryUser()) {
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

void UserManagerBase::NotifyUserAddedToSession(const User* added_user,
                                               bool user_switch_pending) {
  DCHECK(!task_runner_ || task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : session_state_observer_list_)
    observer.UserAddedToSession(added_user);
}

PrefService* UserManagerBase::GetLocalState() const {
  return local_state_.get();
}

bool UserManagerBase::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kFirstExecAfterBoot);
}

bool UserManagerBase::HasBrowserRestarted() const {
  return base::SysInfo::IsRunningOnChromeOS() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kLoginUser);
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
  CallUpdateLoginState();
}

void UserManagerBase::CallUpdateLoginState() {
  UpdateLoginState(active_user_, primary_user_, IsCurrentUserOwner());
}

void UserManagerBase::SetLRUUser(User* user) {
  local_state_->SetString(kLastActiveUser, user->GetAccountId().GetUserEmail());
  local_state_->CommitPendingWrite();

  UserList::iterator it = base::ranges::find(lru_logged_in_users_, user);
  if (it != lru_logged_in_users_.end())
    lru_logged_in_users_.erase(it);
  lru_logged_in_users_.insert(lru_logged_in_users_.begin(), user);
}

void UserManagerBase::SendGaiaUserLoginMetrics(const AccountId& account_id) {
  // If this isn't the first time Chrome was run after the system booted,
  // assume that Chrome was restarted because a previous session ended.
  if (IsFirstExecAfterBoot())
    return;

  const std::string last_email = local_state_->GetString(kLastLoggedInGaiaUser);
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

void UserManagerBase::UpdateUserAccountLocale(const AccountId& account_id,
                                              const std::string& locale) {
  std::unique_ptr<std::string> resolved_locale(new std::string());
  if (!locale.empty() && locale != GetApplicationLocale()) {
    // std::move will nullptr out |resolved_locale|, so cache the underlying
    // ptr.
    std::string* raw_resolved_locale = resolved_locale.get();
    ScheduleResolveLocale(
        locale,
        base::BindOnce(&UserManagerBase::DoUpdateAccountLocale,
                       weak_factory_.GetWeakPtr(), account_id,
                       std::move(resolved_locale)),
        raw_resolved_locale);
  } else {
    resolved_locale = std::make_unique<std::string>(locale);
    DoUpdateAccountLocale(account_id, std::move(resolved_locale));
  }
}

void UserManagerBase::DoUpdateAccountLocale(
    const AccountId& account_id,
    std::unique_ptr<std::string> resolved_locale) {
  User* user = FindUserAndModify(account_id);
  if (user && resolved_locale)
    user->SetAccountLocale(*resolved_locale);
}

void UserManagerBase::DeleteUser(User* user) {
  const bool is_active_user = (user == active_user_);
  delete user;
  if (is_active_user)
    active_user_ = nullptr;
}

// TODO(crbug/1189715): Remove dormant legacy supervised user cryptohomes. After
// we have enough confidence that there are no more supervised users on devices
// in the wild, remove this.
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

}  // namespace user_manager
