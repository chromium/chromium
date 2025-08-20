// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"

namespace tabs {

DEFINE_USER_DATA(GlicActorTaskIconController);
GlicActorTaskIconController::GlicActorTaskIconController(
    BrowserWindowInterface* browser,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(browser->GetProfile()),
      tab_strip_action_container_(tab_strip_action_container),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterTaskIconStateCallback();
    UpdateCurrentTaskIconUiState();
  }
}

GlicActorTaskIconController::~GlicActorTaskIconController() = default;

// static
GlicActorTaskIconController* GlicActorTaskIconController::From(
    BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<GlicActorTaskIconController>::Get(
      browser->GetUnownedUserDataHost());
}

void GlicActorTaskIconController::RegisterTaskIconStateCallback() {
  task_icon_state_change_callback_subscription_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterTaskIconStateChange(
              base::BindRepeating(&GlicActorTaskIconController::OnStateUpdate,
                                  base::Unretained(this))));
}

void GlicActorTaskIconController::UpdateCurrentTaskIconUiState() {
  actor::ui::ActorUiStateManagerInterface::TaskIconUiState task_icon_state =
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->GetTaskIconUiState();
  auto* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  OnStateUpdate(task_icon_state,
                glic_keyed_service->window_controller().state(),
                glic_keyed_service->host().GetPrimaryCurrentView());
}

void GlicActorTaskIconController::OnStateUpdate(
    actor::ui::ActorUiStateManagerInterface::TaskIconUiState task_icon_state,
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView floaty_view) {
  switch (task_icon_state) {
    case actor::ui::ActorUiStateManagerInterface::TaskIconUiState::kShown:
      tab_strip_action_container_->ShowGlicActorTaskIcon();
      break;
    case actor::ui::ActorUiStateManagerInterface::TaskIconUiState::
        kNeedsAttention:
      tab_strip_action_container_->TriggerGlicActorTaskIconCheckTasksNudge();
      break;
    case actor::ui::ActorUiStateManagerInterface::TaskIconUiState::
        kCompleteTasks:
      tab_strip_action_container_->TriggerGlicActorTaskIconCompleteTasksNudge();
      break;
    case actor::ui::ActorUiStateManagerInterface::TaskIconUiState::kHidden:
      tab_strip_action_container_->HideGlicActorTaskIcon();
      break;
  }

  if (task_icon_state !=
      actor::ui::ActorUiStateManagerInterface::TaskIconUiState::kHidden) {
    switch (floaty_state) {
      case glic::GlicWindowController::State::kOpen:
        if (floaty_view == glic::mojom::CurrentView::kConversation) {
          tab_strip_action_container_->UnhighlightGlicActorTaskIcon();
          tab_strip_action_container_->HighlightGlicButton();
        } else if (floaty_view == glic::mojom::CurrentView::kActuation) {
          tab_strip_action_container_->UnhighlightGlicButton();
          tab_strip_action_container_->HighlightGlicActorTaskIcon();
        }
        tab_strip_action_container_->glic_actor_task_icon()
            ->SetFloatyOpenTooltipText();
        break;
      case glic::GlicWindowController::State::kClosed:
        tab_strip_action_container_->UnhighlightGlicActorTaskIcon();
        tab_strip_action_container_->UnhighlightGlicButton();
        tab_strip_action_container_->glic_actor_task_icon()
            ->SetFloatyClosedTooltipText();
        break;
      case glic::GlicWindowController::State::kWaitingForGlicToLoad:
        break;
    }
  }
}

}  // namespace tabs
