// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/management_transition_screen_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/management_transition_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

constexpr base::TimeDelta kWaitingTimeout = base::Minutes(2);

}  // namespace

namespace chromeos {
namespace {

// Management transition screen step names.
const char kManagementTransitionStepError[] = "error";

}  // namespace

constexpr StaticOobeScreenId ManagementTransitionScreenView::kScreenId;

ManagementTransitionScreenHandler::ManagementTransitionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ManagementTransitionScreenHandler::~ManagementTransitionScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
  timer_.Stop();
}

void ManagementTransitionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("addingManagementTitle", IDS_ADDING_MANAGEMENT_TITLE);
  builder->Add("addingManagementTitleUnknownAdmin",
               IDS_ADDING_MANAGEMENT_TITLE_UNKNOWN_ADMIN);
  builder->Add("removingSupervisionTitle", IDS_REMOVING_SUPERVISION_TITLE);
  builder->Add("addingSupervisionTitle", IDS_ADDING_SUPERVISION_TITLE);
  builder->Add("managementTransitionIntroMessage",
               IDS_SUPERVISION_TRANSITION_MESSAGE);
  builder->Add("managementTransitionErrorTitle",
               IDS_SUPERVISION_TRANSITION_ERROR_TITLE);
  builder->Add("managementTransitionErrorMessage",
               IDS_SUPERVISION_TRANSITION_ERROR_MESSAGE);
  builder->Add("managementTransitionErrorButton",
               IDS_SUPERVISION_TRANSITION_ERROR_BUTTON);
}

void ManagementTransitionScreenHandler::RegisterMessages() {
  AddCallback(
      "finishManagementTransition",
      &ManagementTransitionScreenHandler::OnManagementTransitionFinished);
  BaseScreenHandler::RegisterMessages();
}

void ManagementTransitionScreenHandler::Bind(
    ManagementTransitionScreen* screen) {
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
  screen_ = screen;
  if (IsJavascriptAllowed())
    InitializeDeprecated();
}

void ManagementTransitionScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
  timer_.Stop();
}

void ManagementTransitionScreenHandler::Show() {
  if (!IsJavascriptAllowed() || !screen_) {
    show_on_init_ = true;
    return;
  }

  screen_shown_time_ = base::TimeTicks::Now();

  timer_.Start(
      FROM_HERE, kWaitingTimeout,
      base::BindOnce(
          &ManagementTransitionScreenHandler::OnManagementTransitionFailed,
          weak_factory_.GetWeakPtr()));

  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(!ProfileHelper::IsSigninProfile(profile));

  registrar_.Init(profile->GetPrefs());
  registrar_.Add(
      arc::prefs::kArcManagementTransition,
      base::BindRepeating(
          &ManagementTransitionScreenHandler::OnManagementTransitionFinished,
          weak_factory_.GetWeakPtr()));

  // Disable system tray, shutdown button and prevent login as guest when
  // management transition screen is shown.
  SystemTrayClientImpl::Get()->SetPrimaryTrayEnabled(false);
  ash::LoginScreen::Get()->EnableShutdownButton(false);
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(false);
  ash::LoginScreen::Get()->SetIsFirstSigninStep(false);

  base::Value::Dict data;
  data.Set("arcTransition",
           static_cast<int>(arc::GetManagementTransition(profile)));
  data.Set("managementEntity",
           chrome::GetAccountManagerIdentity(profile).value_or(std::string()));
  ShowInWebUI(std::move(data));
}

void ManagementTransitionScreenHandler::Hide() {}

base::OneShotTimer* ManagementTransitionScreenHandler::GetTimerForTesting() {
  return &timer_;
}

void ManagementTransitionScreenHandler::InitializeDeprecated() {
  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void ManagementTransitionScreenHandler::ShowStep(const char* step) {
  CallJS("login.ManagementTransitionScreen.showStep", std::string(step));
}

void ManagementTransitionScreenHandler::OnManagementTransitionFailed() {
  LOG(ERROR) << "Management transition failed; resetting ARC++ data.";
  // Prevent ARC++ data removal below from triggering the success flow (since it
  // will reset the management transition pref).
  registrar_.RemoveAll();
  timed_out_ = true;
  arc::ArcSessionManager::Get()->RequestArcDataRemoval();
  arc::ArcSessionManager::Get()->StopAndEnableArc();
  ShowStep(kManagementTransitionStepError);
}

void ManagementTransitionScreenHandler::OnManagementTransitionFinished() {
  // This method is called both when management transition succeeds (observing
  // pref changes) and when it fails ("OK" button from error screen, see
  // RegisterMessages()). Once this screen exits, user session will be started,
  // so there's no need to re-enable shutdown button from login screen, only the
  // system tray.
  SystemTrayClientImpl::Get()->SetPrimaryTrayEnabled(true);
  if (screen_)
    screen_->OnManagementTransitionFinished();

  UMA_HISTOGRAM_BOOLEAN("Arc.Supervision.Transition.Screen.Successful",
                        !timed_out_);
  if (!timed_out_) {
    base::TimeDelta timeDelta = base::TimeTicks::Now() - screen_shown_time_;
    DVLOG(1) << "Transition succeeded in: " << timeDelta.InSecondsF();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Arc.Supervision.Transition.Screen.Success.TimeDelta", timeDelta);
  }
}

}  // namespace chromeos
