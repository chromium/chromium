// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_controller_client_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

using session_manager::Session;
using session_manager::SessionManager;
using session_manager::SessionState;
using user_manager::User;
using user_manager::UserList;
using user_manager::UserManager;

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
  if (user_session_id == 0u)
    return nullptr;

  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(&user);
  DCHECK(profile);

  auto session = std::make_unique<ash::UserSession>();
  session->session_id = user_session_id;
  session->user_info.type = user.GetType();
  session->user_info.account_id = user.GetAccountId();
  session->user_info.display_name = base::UTF16ToUTF8(user.display_name());
  session->user_info.display_email = user.display_email();
  session->user_info.is_ephemeral =
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user.GetAccountId());
  session->user_info.has_gaia_account = user.has_gaia_account();
  session->user_info.should_display_managed_ui =
      profile && chrome::ShouldDisplayManagedUi(profile);
  session->user_info.service_instance_group =
      content::BrowserContext::GetServiceInstanceGroupFor(profile);
  session->user_info.is_new_profile = profile->IsNewProfile();

  session->user_info.avatar.image = user.GetImage();
  if (session->user_info.avatar.image.isNull()) {
    session->user_info.avatar.image =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_LOGIN_DEFAULT_USER);
  }

  if (user.IsSupervised()) {
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    session->custodian_email = service->GetCustodianEmailAddress();
    session->second_custodian_email = service->GetSecondCustodianEmailAddress();
  }

  chromeos::UserFlow* const user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user.GetAccountId());
  session->should_enable_settings = user_flow->ShouldEnableSettings();
  session->should_show_notification_tray =
      user_flow->ShouldShowNotificationTray();

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
  prefs->SetBoolean(prefs::kMultiProfileNeverShowIntro, never_show_again);
  chromeos::UserAddingScreen::Get()->Start();
}

}  // namespace

namespace mojo {

// When comparing two mojom::UserSession objects we need to decide if the avatar
// images are changed. Consider them equal if they have the same storage rather
// than comparing the backing pixels.
template <>
struct EqualsTraits<gfx::ImageSkia> {
  static bool Equals(const gfx::ImageSkia& a, const gfx::ImageSkia& b) {
    return a.BackedBySameObjectAs(b);
  }
};

}  // namespace mojo

SessionControllerClientImpl::SessionControllerClientImpl() {
  SessionManager::Get()->AddObserver(this);
  UserManager::Get()->AddSessionStateObserver(this);
  UserManager::Get()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

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
  chromeos::DeviceSettingsService::Get()
      ->device_off_hours_controller()
      ->AddObserver(this);
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
  chromeos::DeviceSettingsService::Get()
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
  session_controller_->PrepareForLock(std::move(callback));
}

void SessionControllerClientImpl::StartLock(StartLockCallback callback) {
  session_controller_->StartLock(std::move(callback));
}

void SessionControllerClientImpl::NotifyChromeLockAnimationsComplete() {
  session_controller_->NotifyChromeLockAnimationsComplete();
}

void SessionControllerClientImpl::RunUnlockAnimation(
    base::OnceClosure animation_finished_callback) {
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

void SessionControllerClientImpl::RequestSignOut() {
  chrome::AttemptUserExit();
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
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  if (UserManager::Get()->GetLoggedInUsers().size() >=
      session_manager::kMaximumNumberOfUserSessions) {
    return;
  }

  // Launch sign in screen to add another user to current session.
  if (!UserManager::Get()->GetUsersAllowedForMultiProfile().empty()) {
    // Don't show the dialog if any logged-in user in the multi-profile session
    // dismissed it.
    bool show_intro = true;
    const user_manager::UserList logged_in_users =
        UserManager::Get()->GetLoggedInUsers();
    for (User* user : logged_in_users) {
      show_intro &=
          !multi_user_util::GetProfileFromAccountId(user->GetAccountId())
               ->GetPrefs()
               ->GetBoolean(prefs::kMultiProfileNeverShowIntro);
      if (!show_intro)
        break;
    }
    if (show_intro) {
      session_controller_->ShowMultiprofilesIntroDialog(
          base::BindOnce(&OnAcceptMultiprofilesIntroDialog));
    } else {
      chromeos::UserAddingScreen::Get()->Start();
    }
  }
}

void SessionControllerClientImpl::EmitAshInitialized() {
  // Emit the ash-initialized upstart signal to start Chrome OS tasks that
  // expect that Ash is listening to D-Bus signals they emit. For example,
  // hammerd, which handles detachable base state, communicates the base state
  // purely by emitting D-Bus signals, and thus has to be run whenever Ash is
  // started so Ash (DetachableBaseHandler in particular) gets the proper view
  // of the current detachable base state.
  chromeos::SessionManagerClient::Get()->EmitAshInitialized();
}

PrefService* SessionControllerClientImpl::GetSigninScreenPrefService() {
  return chromeos::ProfileHelper::Get()->GetSigninProfile()->GetPrefs();
}

PrefService* SessionControllerClientImpl::GetUserPrefService(
    const AccountId& account_id) {
  Profile* const user_profile =
      multi_user_util::GetProfileFromAccountId(account_id);
  if (!user_profile)
    return nullptr;

  return user_profile->GetPrefs();
}

// static
bool SessionControllerClientImpl::IsMultiProfileAvailable() {
  if (!profiles::IsMultipleProfilesEnabled() || !UserManager::IsInitialized())
    return false;
  if (chromeos::SessionTerminationManager::Get() &&
      chromeos::SessionTerminationManager::Get()->IsLockedToSingleUser()) {
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
  // TODO(crbug.com/657149): Get rid of this after refactoring.
  SendUserSession(*active_user);
  if (pending_users_.find(active_user->GetAccountId()) != pending_users_.end())
    return;

  SendUserSessionOrder();
}

void SessionControllerClientImpl::UserAddedToSession(const User* added_user) {
  SendSessionInfoIfChanged();
  SendUserSession(*added_user);
}

void SessionControllerClientImpl::OnUserImageChanged(const User& user) {
  SendUserSession(user);
}

// static
bool SessionControllerClientImpl::CanLockScreen() {
  return !UserManager::Get()->GetUnlockUsers().empty();
}

// static
bool SessionControllerClientImpl::ShouldLockScreenAutomatically() {
  // TODO(xiyuan): Observe ash::prefs::kEnableAutoScreenLock and update ash.
  // Tracked in http://crbug.com/670423
  const UserList logged_in_users = UserManager::Get()->GetLoggedInUsers();
  for (auto* user : logged_in_users) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
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
  if (chromeos::SessionTerminationManager::Get()->IsLockedToSingleUser())
    return ash::AddUserSessionPolicy::ERROR_LOCKED_TO_SINGLE_USER;

  UserManager* const user_manager = UserManager::Get();
  if (user_manager->GetUsersAllowedForMultiProfile().empty())
    return ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;

  if (chromeos::MultiProfileUserController::GetPrimaryUserPolicy() !=
      chromeos::MultiProfileUserController::ALLOWED) {
    return ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
  }

  if (UserManager::Get()->GetLoggedInUsers().size() >=
      session_manager::kMaximumNumberOfUserSessions) {
    return ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;
  }

  return ash::AddUserSessionPolicy::ALLOWED;
}

// static
void SessionControllerClientImpl::DoLockScreen() {
  if (!CanLockScreen())
    return;

  VLOG(1) << "Requesting screen lock from SessionControllerClientImpl";
  chromeos::SessionManagerClient::Get()->RequestLockScreen();
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
  auto it = std::find_if(logged_in_users.begin(), logged_in_users.end(),
                         [account_id](const User* user) {
                           return user->GetAccountId() == account_id;
                         });

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
    NOTREACHED() << "Invalid direction=" << static_cast<int>(direction);
    return;
  }

  DoSwitchActiveUser(account_id);
}

void SessionControllerClientImpl::OnSessionStateChanged() {
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
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id));
}

void SessionControllerClientImpl::OnCustodianInfoChanged() {
  DCHECK(supervised_user_profile_);
  User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(
      supervised_user_profile_);
  if (user)
    SendUserSession(*user);
}

void SessionControllerClientImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      session_controller_->NotifyChromeTerminating();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void SessionControllerClientImpl::OnLoginUserProfilePrepared(Profile* profile) {
  const User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);

  if (profile->IsSupervised()) {
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
  info->is_running_in_app_mode = chrome::IsRunningInAppMode();
  info->is_demo_session =
      chromeos::DemoSession::Get() && chromeos::DemoSession::Get()->started();
  info->add_user_session_policy = GetAddUserSessionPolicy();
  info->state = session_manager->session_state();

  if (last_sent_session_info_ && *info == *last_sent_session_info_)
    return;

  last_sent_session_info_ = std::move(info);
  session_controller_->SetSessionInfo(*last_sent_session_info_);
}

void SessionControllerClientImpl::SendUserSession(const User& user) {
  // Check user profile via GetProfileByUser() instead of is_profile_created()
  // flag because many tests have only setup testing user profile in
  // ProfileHelper but do not have the flag updated.
  if (!chromeos::ProfileHelper::Get()->GetProfileByUser(&user)) {
    pending_users_.insert(user.GetAccountId());
    return;
  }

  auto user_session = UserToUserSession(user);

  // Bail if the user has no session. Currently the only code path that hits
  // this condition is from OnUserImageChanged when user images are changed
  // on the login screen (e.g. policy change that adds a public session user,
  // or tests that create new users on the login screen).
  if (!user_session)
    return;

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
  for (auto* user : user_manager->GetLRULoggedInUsers()) {
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
    session_length_limit = base::TimeDelta::FromMilliseconds(
        std::min(std::max(local_state->GetInteger(prefs::kSessionLengthLimit),
                          kSessionLengthLimitMinMs),
                 kSessionLengthLimitMaxMs));
  }
  base::TimeTicks session_start_time;
  if (local_state->HasPrefPath(prefs::kSessionStartTime)) {
    session_start_time = base::TimeTicks::FromInternalValue(
        local_state->GetInt64(prefs::kSessionStartTime));
  }

  policy::off_hours::DeviceOffHoursController* off_hours_controller =
      chromeos::DeviceSettingsService::Get()->device_off_hours_controller();
  base::TimeTicks off_hours_session_end_time;
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
  base::TimeTicks off_hours_session_start_time = base::TimeTicks::Now();
  base::TimeDelta off_hours_session_length_limit =
      off_hours_session_end_time - off_hours_session_start_time;
  session_controller_->SetSessionLengthLimit(off_hours_session_length_limit,
                                             off_hours_session_start_time);
}
