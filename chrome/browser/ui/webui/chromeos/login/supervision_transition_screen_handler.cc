// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"

#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/supervision_transition_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"

namespace {

constexpr base::TimeDelta kWaitingTimeout = base::TimeDelta::FromMinutes(2);

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId SupervisionTransitionScreenView::kScreenId;

SupervisionTransitionScreenHandler::SupervisionTransitionScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

SupervisionTransitionScreenHandler::~SupervisionTransitionScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
  timer_.Stop();
}

void SupervisionTransitionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("removingSupervisionTitle", IDS_REMOVING_SUPERVISION_TITLE);
  builder->Add("addingSupervisionTitle", IDS_ADDING_SUPERVISION_TITLE);
  builder->Add("supervisionTransitionIntroMessage",
               IDS_SUPERVISION_TRANSITION_MESSAGE);
  builder->Add("supervisionTransitionErrorTitle",
               IDS_SUPERVISION_TRANSITION_ERROR_TITLE);
  builder->Add("supervisionTransitionErrorMessage",
               IDS_SUPERVISION_TRANSITION_ERROR_MESSAGE);
  builder->Add("supervisionTransitionButton",
               IDS_SUPERVISION_TRANSITION_ERROR_BUTTON);
}

void SupervisionTransitionScreenHandler::RegisterMessages() {
  AddCallback(
      "finishSupervisionTransition",
      &SupervisionTransitionScreenHandler::OnSupervisionTransitionFinished);
  BaseScreenHandler::RegisterMessages();
}

void SupervisionTransitionScreenHandler::Bind(
    SupervisionTransitionScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void SupervisionTransitionScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
  timer_.Stop();
}

void SupervisionTransitionScreenHandler::Show() {
  if (!page_is_ready() || !screen_) {
    show_on_init_ = true;
    return;
  }

  screen_shown_time_ = base::TimeTicks::Now();

  timer_.Start(
      FROM_HERE, kWaitingTimeout,
      base::BindOnce(
          &SupervisionTransitionScreenHandler::OnSupervisionTransitionFailed,
          weak_factory_.GetWeakPtr()));

  registrar_.Init(profile_->GetPrefs());
  registrar_.Add(
      arc::prefs::kArcSupervisionTransition,
      base::BindRepeating(
          &SupervisionTransitionScreenHandler::OnSupervisionTransitionFinished,
          weak_factory_.GetWeakPtr()));

  // Disable system tray, shutdown button and prevent login as guest when
  // supervision transition screen is shown.
  SystemTrayClient::Get()->SetPrimaryTrayEnabled(false);
  ash::LoginScreen::Get()->EnableShutdownButton(false);
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(false);
  ash::LoginScreen::Get()->ShowGuestButtonInOobe(false);

  base::DictionaryValue data;
  data.SetBoolean("isRemovingSupervision",
                  arc::GetSupervisionTransition(profile_) ==
                      arc::ArcSupervisionTransition::CHILD_TO_REGULAR);
  ShowScreenWithData(kScreenId, &data);
}

void SupervisionTransitionScreenHandler::Hide() {}

base::OneShotTimer* SupervisionTransitionScreenHandler::GetTimerForTesting() {
  return &timer_;
}

void SupervisionTransitionScreenHandler::Initialize() {
  profile_ = ProfileManager::GetPrimaryUserProfile();

  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void SupervisionTransitionScreenHandler::OnSupervisionTransitionFailed() {
  LOG(ERROR) << "Supervision transition failed; resetting ARC++ data.";
  // Prevent ARC++ data removal below from triggering the success flow (since it
  // will reset the supervision transition pref).
  registrar_.RemoveAll();
  timed_out_ = true;
  arc::ArcSessionManager::Get()->RequestArcDataRemoval();
  arc::ArcSessionManager::Get()->StopAndEnableArc();
  if (screen_) {
    AllowJavascript();
    FireWebUIListener("supervision-transition-failed");
  }
}

void SupervisionTransitionScreenHandler::OnSupervisionTransitionFinished() {
  // This method is called both when supervision transition succeeds (observing
  // pref changes) and when it fails ("OK" button from error screen, see
  // RegisterMessages()). Once this screen exits, user session will be started,
  // so there's no need to re-enable shutdown button from login screen, only the
  // system tray.
  SystemTrayClient::Get()->SetPrimaryTrayEnabled(true);
  if (screen_)
    screen_->OnSupervisionTransitionFinished();

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
