// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/user_adding_screen.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

UserAddingScreen* g_instance = nullptr;

}  // namespace

UserAddingScreen::UserAddingScreen() {
  CHECK(!g_instance);
  g_instance = this;
}

UserAddingScreen::~UserAddingScreen() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserAddingScreen* UserAddingScreen::Get() {
  CHECK(g_instance);
  return g_instance;
}

void UserAddingScreen::Start() {
  // TODO(crbug.com/403154552): Avoid g_browser_process.
  PrefService* local_state = g_browser_process->local_state();
  ::metrics::MetricsService* metrics_service =
      g_browser_process->metrics_service();
  ApplicationLocaleStorage* application_locale_storage =
      g_browser_process->GetFeatures()->application_locale_storage();
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  scoped_refptr<component_updater::ComponentManagerAsh> component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();
  policy::DeviceRestrictionScheduleController*
      device_restriction_schedule_controller =
          g_browser_process->platform_part()
              ->device_restriction_schedule_controller();
  const user_manager::MultiUserSignInPolicyController*
      multi_user_sign_in_policy_controller =
          g_browser_process->platform_part()
              ->multi_user_sign_in_policy_controller();
  system::SystemClock* system_clock =
      g_browser_process->platform_part()->GetSystemClock();

  CHECK(!IsRunning());
  display_host_ = new LoginDisplayHostMojo(
      local_state, metrics_service, application_locale_storage,
      shared_url_loader_factory, browser_policy_connector_ash,
      component_manager_ash, device_restriction_schedule_controller,
      multi_user_sign_in_policy_controller, system_clock,
      DisplayedScreen::USER_ADDING_SCREEN,
      /*update_geolocation_usage_allowed=*/false);

  // The screen adds a user to an existing session, but it is also reachable
  // from the login screen, where there is none yet.
  const session_manager::Session* const session =
      session_manager::SessionManager::Get()->GetActiveSession();
  interrupted_user_ =
      session ? std::make_optional(session->account_id()) : std::nullopt;

  // This triggers input method manager to filter login screen methods. This
  // should happen before setting user input method, which happens when focusing
  // user pod (triggered by SetSessionState)"
  im_controller_.OnScreenShown(interrupted_user_.has_value());

  for (auto& observer : observers_) {
    observer.OnBeforeUserAddingScreenStarted();
  }

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  display_host_->StartUserAdding(base::BindOnce(
      &UserAddingScreen::OnDisplayHostCompletion, base::Unretained(this)));
}

void UserAddingScreen::Cancel() {
  CHECK(IsRunning());

  display_host_->CancelUserAdding();

  // Reset wallpaper if cancel adding user from multiple user sign in page.
  // Nothing was added, so the user to put back is the one the screen
  // interrupted.
  if (interrupted_user_.has_value()) {
    WallpaperControllerClientImpl::Get()->ShowUserWallpaper(
        interrupted_user_.value());
  }
}

bool UserAddingScreen::IsRunning() const {
  return display_host_ != nullptr;
}

void UserAddingScreen::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAddingScreen::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAddingScreen::OnDisplayHostCompletion() {
  CHECK(IsRunning());
  display_host_ = nullptr;

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // The user just added, or the one the screen interrupted if the adding was
  // cancelled.
  const session_manager::Session* const session =
      session_manager::SessionManager::Get()->GetActiveSession();
  const std::optional<AccountId> active_user =
      session ? std::make_optional(session->account_id()) : std::nullopt;
  im_controller_.OnScreenHidden(active_user != interrupted_user_);

  for (auto& observer : observers_) {
    observer.OnUserAddingFinished();
  }
}

}  // namespace ash
