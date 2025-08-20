// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"

namespace tabs {
using glic::GlicKeyedService;
using glic::GlicWindowController;
using glic::mojom::CurrentView;

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
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    task_icon_state_change_callback_subscription_.push_back(
        manager->RegisterTaskIconStateChange(
            base::BindRepeating(&GlicActorTaskIconController::OnStateUpdate,
                                base::Unretained(this))));
  }
}

void GlicActorTaskIconController::UpdateCurrentTaskIconUiState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    OnStateUpdate(
        GlicKeyedService::Get(profile_)->window_controller().state(),
        GlicKeyedService::Get(profile_)->host().GetPrimaryCurrentView(),
        manager->GetCurrentActorTaskIconState());
  }
}

void GlicActorTaskIconController::OnStateUpdate(
    GlicWindowController::State floaty_state,
    CurrentView floaty_view,
    const ActorTaskIconState& actor_task_icon_state) {
  // If the task icon is inactive, hide it and perform no additional style
  // changes.
  if (!actor_task_icon_state.is_visible) {
    tab_strip_action_container_->HideGlicActorTaskIcon();
    return;
  }

  // Determines the text to be shown.
  // TODO(crbug.com/431015299): Consider consolidating these 3 calls and pass
  // the text as a string from the manager instead.
  switch (actor_task_icon_state.text) {
    case ActorTaskIconState::Text::kDefault:
      tab_strip_action_container_->ShowGlicActorTaskIcon();
      break;
    case ActorTaskIconState::Text::kNeedsAttention:
      tab_strip_action_container_->TriggerGlicActorTaskIconCheckTasksNudge();
      break;
    case ActorTaskIconState::Text::kCompleteTasks:
      tab_strip_action_container_->TriggerGlicActorTaskIconCompleteTasksNudge();
      break;
  }

  // Determines highlight + tooltip styling.
  switch (floaty_state) {
    case GlicWindowController::State::kOpen:
      if (floaty_view == CurrentView::kConversation) {
        tab_strip_action_container_->UnhighlightGlicActorTaskIcon();
        tab_strip_action_container_->HighlightGlicButton();
      } else if (floaty_view == CurrentView::kActuation) {
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
    case GlicWindowController::State::kWaitingForGlicToLoad:
      break;
  }
}

}  // namespace tabs
