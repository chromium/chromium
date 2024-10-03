// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

using session_manager::Session;
using session_manager::SessionManager;
using session_manager::SessionState;
using user_manager::User;
using user_manager::UserList;
using user_manager::UserManager;

// TODO(b/228873153): Remove after figuring out the root cause of the bug
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

SessionControllerClientImpl* g_session_controller_client_instance = nullptr;

// Returns the session id of a given user or 0 if user has no session.
uint32_t GetSessionId(const User& user) {
  const AccountId& account_id = user.GetAccountId();
  for (auto& session : SessionManager::Get()->sessions()) {
    if (session.user_account_id == account_id)
      return session.id;
  }

  return 0u;
}

// Creates a mojom::UserSession for the given user. Returns nullptr if there is
// no user session started for the given user.
std::unique_ptr<ash::UserSession> UserToUserSession(const User& user) {
  const uint32_t user_session_id = GetSessionId(user);
  DCHECK_NE(0u, user_session_id);

  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(&user);
  DCHECK(profile);

  auto session = std::make_unique<ash::UserSession>();
  session->session_id = user_session_id;
  session->user_info.type = user.GetType();
  session->user_info.account_id = user.GetAccountId();
  session->user_info.display_name = base::UTF16ToUTF8(user.display_name());
  session->user_info.display_email = user.display_email();
  session->user_info.given_name = base::UTF16ToUTF8(user.GetGivenName());
  session->user_info.is_ephemeral =
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user.GetAccountId());
  session->user_info.has_gaia_account = user.has_gaia_account();
  session->user_info.should_display_managed_ui =
      profile && chrome::ShouldDisplayManagedUi(profile);
  session->user_info.is_new_profile = profile->IsNewProfile();
  session->user_info.is_managed =
      profile->GetProfilePolicyConnector()->IsManaged();

  session->user_info.avatar.image = user.GetImage();
  if (session->user_info.avatar.image.isNull()) {
    session->user_info.avatar.image =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_LOGIN_DEFAULT_USER);
  }

  return session;
}

void DoSwitchUser(const AccountId& account_id, bool switch_user) {
  if (switch_user)
    UserManager::Get()->SwitchActiveUser(account_id);
}

// Callback for the dialog that warns the user about multi-profile, which has
// a "never show again" checkbox.
void OnAcceptMultiprofilesIntroDialog(bool accept, bool never_show_again) {
  if (!accept)
    return;

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(user_manager::prefs::kMultiProfileNeverShowIntro,
                    never_show_again);
  ash::UserAddingScreen::Get()->Start();
}

}  // namespace

SessionControllerClientImpl::SessionControllerClientImpl() {
  SessionManager::Get()->AddObserver(this);
  UserManager::Get()->AddSessionStateObserver(this);
  UserManager::Get()->AddObserver(this);

  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &SessionControllerClientImpl::OnAppTerminating, base::Unretained(this)));

  local_state_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_registrar_->Init(g_browser_process->local_state());
  local_state_registrar_->Add(
      prefs::kSessionStartTime,
      base::BindRepeating(&SessionControllerClientImpl::SendSessionLengthLimit,
                          base::Unretained(this)));
  local_state_registrar_->Add(
      prefs::kSessionLengthLimit,
      base::BindRepeating(&SessionControllerClientImpl::SendSessionLengthLimit,
                          base::Unretained(this)));
  ash::DeviceSettingsService::Get()->device_off_hours_controller()->AddObserver(
      this);
  DCHECK(!g_session_controller_client_instance);
  g_session_controller_client_instance = this;
}

SessionControllerClientImpl::~SessionControllerClientImpl() {
  DCHECK_EQ(this, g_session_controller_client_instance);
  g_session_controller_client_instance = nullptr;
  if (session_controller_ &&
      session_controller_ == ash::SessionController::Get()) {
    session_controller_->SetClient(nullptr);
  }

  if (supervised_user_profile_) {
    SupervisedUserServiceFactory::GetForProfile(supervised_user_profile_)
        ->RemoveObserver(this);
  }

  SessionManager::Get()->RemoveObserver(this);
  UserManager::Get()->RemoveObserver(this);
  UserManager::Get()->RemoveSessionStateObserver(this);
  ash::DeviceSettingsService::Get()
      ->device_off_hours_controller()
      ->RemoveObserver(this);
}

void SessionControllerClientImpl::Init() {
  session_controller_ = ash::SessionController::Get();
  session_controller_->SetClient(this);

  SendSessionInfoIfChanged();
  SendSessionLengthLimit();
  // User sessions and their order will be sent via UserSessionStateObserver
  // even for crash-n-restart.
}

// static
SessionControllerClientImpl* SessionControllerClientImpl::Get() {
  return g_session_controller_client_instance;
}

void SessionControllerClientImpl::PrepareForLock(base::OnceClosure callback) {
  if (ash::floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    User* active_user = UserManager::Get()->GetActiveUser();
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(active_user);
    if (profile) {
      auto* floating_workspace_service =
          ash::FloatingWorkspaceServiceFactory::GetForProfile(profile);
      if (floating_workspace_service) {
        floating_workspace_service->CaptureAndUploadActiveDesk();
      }
    }
  }
  session_controller_->PrepareForLock(std::move(callback));
}

void SessionControllerClientImpl::StartLock(StartLockCallback callback) {
  session_controller_->StartLock(std::move(callback));
}

void SessionControllerClientImpl::NotifyChromeLockAnimationsComplete() {
  session_controller_->NotifyChromeLockAnimationsComplete();
}

void SessionControllerClientImpl::RunUnlockAnimation(
    ash::SessionController::RunUnlockAnimationCallback
        animation_finished_callback) {
  session_controller_->RunUnlockAnimation(
      std::move(animation_finished_callback));
}

void SessionControllerClientImpl::ShowTeleportWarningDialog(
    base::OnceCallback<void(bool, bool)> on_accept) {
  session_controller_->ShowTeleportWarningDialog(std::move(on_accept));
}

void SessionControllerClientImpl::RequestLockScreen() {
  DoLockScreen();
}

void SessionControllerClientImpl::RequestHideLockScreen() {
  ash::ScreenLocker::Hide();
}

void SessionControllerClientImpl::RequestSignOut() {
  chrome::AttemptUserExit();
}

void SessionControllerClientImpl::RequestRestartForUpdate() {
  chrome::AttemptRelaunch();
}

void SessionControllerClientImpl::AttemptRestartChrome() {
  chrome::AttemptRestart();
}

void SessionControllerClientImpl::SwitchActiveUser(
    const AccountId& account_id) {
  DoSwitchActiveUser(account_id);
}

void SessionControllerClientImpl::CycleActiveUser(
    ash::CycleUserDirection direction) {
  DoCycleActiveUser(direction);
}

void SessionControllerClientImpl::ShowMultiProfileLogin() {
  if (!IsMultiProfileAvailable())
    return;

  // Only regular non-supervised users could add other users to current session.
  if (UserManager::Get()->GetActiveUser()->GetType() !=
      user_manager::UserType::kRegular) {
    return;
  }

  DCHECK(UserManager::Get()->GetLoggedInUsers().size() <
         session_manager::kMaximumNumberOfUserSessions);

  // Launch sign in screen to add another user to current session.
  DCHECK(!UserManager::Get()->GetUsersAllowedForMultiProfile().empty());

  // Lacros and multiprofile are mutually exclusive.
  const auto* primary_user = UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(!crosapi::browser_util::IsLacrosEnabledForMigration(
      primary_user,
      ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));

  // Don't show the dialog if any logged-in user in the multi-profile session
  // dismissed it.
  bool show_intro = true;
  const user_manager::UserList logged_in_users =
      UserManager::Get()->GetLoggedInUsers();
  for (User* user : logged_in_users) {
    show_intro &=
        !multi_user_util::GetProfileFromAccountId(user->GetAccountId())
             ->GetPrefs()
             ->GetBoolean(user_manager::prefs::kMultiProfileNeverShowIntro);
    if (!show_intro)
      break;
  }
  if (show_intro) {
    session_controller_->ShowMultiprofilesIntroDialog(
        base::BindOnce(&OnAcceptMultiprofilesIntroDialog));
  } else {
    ash::UserAddingScreen::Get()->Start();
  }
}

void SessionControllerClientImpl::EmitAshInitialized() {
  // Emit the ash-initialized upstart signal to start Chrome OS tasks that
  // expect that Ash is listening to D-Bus signals they emit. For example,
  // hammerd, which handles detachable base state, communicates the base state
  // purely by emitting D-Bus signals, and thus has to be run whenever Ash is
  // started so Ash (DetachableBaseHandler in particular) gets the proper view
  // of the current detachable base state.
  ash::SessionManagerClient::Get()->EmitAshInitialized();
}

PrefService* SessionControllerClientImpl::GetSigninScreenPrefService() {
  return ash::ProfileHelper::Get()->GetSigninProfile()->GetPrefs();
}

PrefService* SessionControllerClientImpl::GetUserPrefService(
    const AccountId& account_id) {
  Profile* const user_profile =
      multi_user_util::GetProfileFromAccountId(account_id);
  if (!user_profile)
    return nullptr;

  return user_profile->GetPrefs();
}

base::FilePath SessionControllerClientImpl::GetProfilePath(
    const AccountId& account_id) {
  Profile* const user_profile =
      multi_user_util::GetProfileFromAccountId(account_id);
  if (!user_profile) {
    return base::FilePath();
  }

  return user_profile->GetPath();
}

std::tuple<bool, bool> SessionControllerClientImpl::IsEligibleForSeaPen(
    const AccountId& account_id) {
  Profile* const user_profile =
      multi_user_util::GetProfileFromAccountId(account_id);
  if (!user_profile) {
    return {false, false};
  }

  return {ash::personalization_app::IsEligibleForSeaPen(user_profile),
          ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(
              user_profile)};
}

std::optional<int> SessionControllerClientImpl::GetExistingUsersCount() const {
  const auto* user_manager = UserManager::Get();
  return !user_manager ? std::nullopt
                       : std::optional<int>(user_manager->GetUsers().size());
}

// static
bool SessionControllerClientImpl::IsMultiProfileAvailable() {
  if (!profiles::IsMultipleProfilesEnabled() || !UserManager::IsInitialized())
    return false;
  if (ash::SessionTerminationManager::Get() &&
      ash::SessionTerminationManager::Get()->IsLockedToSingleUser()) {
    return false;
  }
  // Multiprofile mode is not allowed if Lacros is enabled.
  const auto* primary_user = UserManager::Get()->GetPrimaryUser();
  if (primary_user && crosapi::browser_util::IsLacrosEnabledForMigration(
                          primary_user, ash::standalone_browser::migrator_util::
                                            PolicyInitState::kAfterInit)) {
    return false;
  }
  size_t users_logged_in = UserManager::Get()->GetLoggedInUsers().size();
  // Does not include users that are logged in.
  size_t users_available_to_add =
      UserManager::Get()->GetUsersAllowedForMultiProfile().size();
  return (users_logged_in + users_available_to_add) > 1;
}

void SessionControllerClientImpl::ActiveUserChanged(User* active_user) {
  SendSessionInfoIfChanged();

  // Try to send user session before updating the order. Skip sending session
  // order if user session ends up to be pending (due to user profile loading).
  // TODO(crbug.com/40489520): Get rid of this after refactoring.
  SendUserSession(*active_user);
  if (pending_users_.find(active_user->GetAccountId()) != pending_users_.end())
    return;

  SendUserSessionOrder();
}

void SessionControllerClientImpl::UserAddedToSession(const User* added_user) {
  SendSessionInfoIfChanged();
  SendUserSession(*added_user);
}

void SessionControllerClientImpl::LocalStateChanged(
    user_manager::UserManager* user_manager) {
  SendSessionInfoIfChanged();
}

void SessionControllerClientImpl::OnUserImageChanged(const User& user) {
  // Only sends user session for signed-in user.
  if (GetSessionId(user) != 0)
    SendUserSession(user);
}

void SessionControllerClientImpl::OnUserNotAllowed(
    const std::string& user_email) {
  LOG(ERROR) << "Shutdown session because a user is not allowed to be in the "
                "current session";
  session_controller_->ShowMultiprofilesSessionAbortedDialog(user_email);
}

void SessionControllerClientImpl::OnUserToBeRemoved(
    const AccountId& account_id) {
  session_controller_->NotifyUserToBeRemoved(account_id);
}

// static
bool SessionControllerClientImpl::CanLockScreen() {
  return !UserManager::Get()->GetUnlockUsers().empty();
}

// static
bool SessionControllerClientImpl::ShouldLockScreenAutomatically() {
  const UserList logged_in_users = UserManager::Get()->GetLoggedInUsers();
  for (user_manager::User* user : logged_in_users) {
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    if (profile &&
        profile->GetPrefs()->GetBoolean(ash::prefs::kEnableAutoScreenLock)) {
      return true;
    }
  }
  return false;
}

// static
ash::AddUserSessionPolicy
SessionControllerClientImpl::GetAddUserSessionPolicy() {
  if (ash::SessionTerminationManager::Get()->IsLockedToSingleUser())
    return ash::AddUserSessionPolicy::ERROR_LOCKED_TO_SINGLE_USER;

  UserManager* const user_manager = UserManager::Get();
  if (user_manager->GetUsersAllowedForMultiProfile().empty())
    return ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;

  if (user_manager::GetMultiUserSignInPolicy(user_manager->GetPrimaryUser()) ==
      user_manager::MultiUserSignInPolicy::kNotAllowed) {
    return ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
  }

  if (user_manager->GetLoggedInUsers().size() >=
      session_manager::kMaximumNumberOfUserSessions) {
    return ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;
  }

  const auto* primary_user = user_manager->GetPrimaryUser();
  if (primary_user) {
    if (crosapi::browser_util::IsLacrosEnabledForMigration(
            primary_user, ash::standalone_browser::migrator_util::
                              PolicyInitState::kAfterInit)) {
      return ash::AddUserSessionPolicy::ERROR_LACROS_ENABLED;
    }
  }

  return ash::AddUserSessionPolicy::ALLOWED;
}

// static
void SessionControllerClientImpl::DoLockScreen() {
  if (!CanLockScreen())
    return;

  VLOG(1) << "b/228873153 : Requesting screen lock from "
             "SessionControllerClientImpl";
  ash::SessionManagerClient::Get()->RequestLockScreen();
}

// static
void SessionControllerClientImpl::DoSwitchActiveUser(
    const AccountId& account_id) {
  // Disallow switching to an already active user since that might crash.
  if (account_id == UserManager::Get()->GetActiveUser()->GetAccountId())
    return;

  // |client| may be null in tests.
  SessionControllerClientImpl* client = SessionControllerClientImpl::Get();
  if (client) {
    SessionControllerClientImpl::Get()
        ->session_controller_->CanSwitchActiveUser(
            base::BindOnce(&DoSwitchUser, account_id));
  } else {
    DoSwitchUser(account_id, true);
  }
}

// static
void SessionControllerClientImpl::DoCycleActiveUser(
    ash::CycleUserDirection direction) {
  const UserList& logged_in_users = UserManager::Get()->GetLoggedInUsers();
  if (logged_in_users.size() <= 1)
    return;

  AccountId account_id = UserManager::Get()->GetActiveUser()->GetAccountId();

  // Get an iterator positioned at the active user.
  auto it =
      base::ranges::find(logged_in_users, account_id, &User::GetAccountId);

  // Active user not found.
  if (it == logged_in_users.end())
    return;

  // Get the user's email to select, wrapping to the start/end of the list if
  // necessary.
  if (direction == ash::CycleUserDirection::NEXT) {
    if (++it == logged_in_users.end())
      account_id = (*logged_in_users.begin())->GetAccountId();
    else
      account_id = (*it)->GetAccountId();
  } else if (direction == ash::CycleUserDirection::PREVIOUS) {
    if (it == logged_in_users.begin())
      it = logged_in_users.end();
    account_id = (*(--it))->GetAccountId();
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Invalid direction=" << static_cast<int>(direction);
    return;
  }

  DoSwitchActiveUser(account_id);
}

void SessionControllerClientImpl::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "SessionControllerClientImpl::OnSessionStateChanged");
  if (SessionManager::Get()->session_state() == SessionState::ACTIVE) {
    // The active user should not be pending when the session becomes active.
    DCHECK(pending_users_.find(
               UserManager::Get()->GetActiveUser()->GetAccountId()) ==
           pending_users_.end());
  }

  SendSessionInfoIfChanged();
}

void SessionControllerClientImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  OnLoginUserProfilePrepared(
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id));
}

void SessionControllerClientImpl::OnUserSessionStartUpTaskCompleted() {
  session_controller_->NotifyFirstSessionReady();
}

void SessionControllerClientImpl::OnCustodianInfoChanged() {
  DCHECK(supervised_user_profile_);
  User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(supervised_user_profile_);
  if (user)
    SendUserSession(*user);
}

void SessionControllerClientImpl::OnAppTerminating() {
  session_controller_->NotifyChromeTerminating();
}

void SessionControllerClientImpl::OnLoginUserProfilePrepared(Profile* profile) {
  const User* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);

  if (profile->IsChild()) {
    // There can be only one supervised user per session.
    DCHECK(!supervised_user_profile_);
    supervised_user_profile_ = profile;

    // Watch for changes to supervised user manager/custodians.
    SupervisedUserServiceFactory::GetForProfile(supervised_user_profile_)
        ->AddObserver(this);
  }

  base::RepeatingClosure session_info_changed_closure = base::BindRepeating(
      &SessionControllerClientImpl::SendSessionInfoIfChanged,
      weak_ptr_factory_.GetWeakPtr());
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar =
      std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar->Init(profile->GetPrefs());
  pref_change_registrar->Add(ash::prefs::kAllowScreenLock,
                             session_info_changed_closure);
  pref_change_registrar->Add(ash::prefs::kEnableAutoScreenLock,
                             session_info_changed_closure);
  pref_change_registrars_.push_back(std::move(pref_change_registrar));

  SendUserSession(*user);
}

void SessionControllerClientImpl::OnOffHoursEndTimeChanged() {
  SendSessionLengthLimit();
}

void SessionControllerClientImpl::SendSessionInfoIfChanged() {
  SessionManager* const session_manager = SessionManager::Get();

  auto info = std::make_unique<ash::SessionInfo>();
  info->can_lock_screen = CanLockScreen();
  info->should_lock_screen_automatically = ShouldLockScreenAutomatically();
  info->is_running_in_app_mode = IsRunningInAppMode();
  info->is_demo_session =
      ash::DemoSession::Get() && ash::DemoSession::Get()->started();
  info->add_user_session_policy = GetAddUserSessionPolicy();
  info->state = session_manager->session_state();

  if (last_sent_session_info_ && *info == *last_sent_session_info_)
    return;

  last_sent_session_info_ = std::move(info);
  session_controller_->SetSessionInfo(*last_sent_session_info_);
}

void SessionControllerClientImpl::SendUserSession(const User& user) {
  // |user| must have a session, i.e. signed-in already.
  DCHECK_NE(0u, GetSessionId(user));

  // Check user profile via GetProfileByUser() instead of is_profile_created()
  // flag because many tests have only setup testing user profile in
  // ProfileHelper but do not have the flag updated.
  if (!ash::ProfileHelper::Get()->GetProfileByUser(&user)) {
    pending_users_.insert(user.GetAccountId());
    return;
  }

  auto user_session = UserToUserSession(user);
  if (last_sent_user_session_ && *user_session == *last_sent_user_session_)
    return;

  last_sent_user_session_ = std::move(user_session);
  session_controller_->UpdateUserSession(*last_sent_user_session_);

  if (!pending_users_.empty()) {
    pending_users_.erase(user.GetAccountId());
    if (pending_users_.empty())
      SendUserSessionOrder();
  }
}

void SessionControllerClientImpl::SendUserSessionOrder() {
  UserManager* const user_manager = UserManager::Get();

  const UserList logged_in_users = user_manager->GetLoggedInUsers();
  std::vector<uint32_t> user_session_ids;
  for (user_manager::User* user : user_manager->GetLRULoggedInUsers()) {
    const uint32_t user_session_id = GetSessionId(*user);
    DCHECK_NE(0u, user_session_id);
    user_session_ids.push_back(user_session_id);
  }

  session_controller_->SetUserSessionOrder(user_session_ids);
}

void SessionControllerClientImpl::SendSessionLengthLimit() {
  const PrefService* local_state = local_state_registrar_->prefs();
  base::TimeDelta session_length_limit;
  if (local_state->HasPrefPath(prefs::kSessionLengthLimit)) {
    session_length_limit = base::Milliseconds(
        std::clamp(local_state->GetInteger(prefs::kSessionLengthLimit),
                    kSessionLengthLimitMinMs, kSessionLengthLimitMaxMs));
  }
  base::Time session_start_time;
  if (local_state->HasPrefPath(prefs::kSessionStartTime)) {
    session_start_time = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kSessionStartTime));
  }

  policy::off_hours::DeviceOffHoursController* off_hours_controller =
      ash::DeviceSettingsService::Get()->device_off_hours_controller();
  base::Time off_hours_session_end_time;
  // Use "OffHours" end time only if the session will be actually terminated.
  if (off_hours_controller->IsCurrentSessionAllowedOnlyForOffHours())
    off_hours_session_end_time = off_hours_controller->GetOffHoursEndTime();

  // If |session_length_limit| is zero or |session_start_time| is null then
  // "SessionLengthLimit" policy is unset.
  const bool session_length_limit_policy_set =
      !session_length_limit.is_zero() && !session_start_time.is_null();

  // If |off_hours_session_end_time| is null then either "OffHours" policy mode
  // is off or the current session shouldn't be terminated after "OffHours".
  if (off_hours_session_end_time.is_null()) {
    // Send even if both values are zero because enterprise policy could turn
    // both features off in the middle of the session.
    session_controller_->SetSessionLengthLimit(session_length_limit,
                                               session_start_time);
    return;
  }
  if (session_length_limit_policy_set &&
      session_start_time + session_length_limit < off_hours_session_end_time) {
    session_controller_->SetSessionLengthLimit(session_length_limit,
                                               session_start_time);
    return;
  }
  base::Time off_hours_session_start_time = base::Time::Now();
  base::TimeDelta off_hours_session_length_limit =
      off_hours_session_end_time - off_hours_session_start_time;
  session_controller_->SetSessionLengthLimit(off_hours_session_length_limit,
                                             off_hours_session_start_time);
}

void SessionControllerClientImpl::OnStateChanged() {
  // Lacros is mutually exclusive with multi sign-in. If Lacros was running
  // (or launching/terminating) and now is not (or vice-versa), we want to
  // propagate this change to make multi sign-in unavailable or available.
  SendSessionInfoIfChanged();
}
