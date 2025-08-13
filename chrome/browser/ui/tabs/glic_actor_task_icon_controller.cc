// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif

namespace tabs {

GlicActorTaskIconController::GlicActorTaskIconController(
    Profile* profile,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(profile),
      tab_strip_action_container_(tab_strip_action_container) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterTaskIconStateCallback();
    UpdateCurrentTaskIconUiState();
  }
}

GlicActorTaskIconController::~GlicActorTaskIconController() = default;

void GlicActorTaskIconController::RegisterTaskIconStateCallback() {
#if BUILDFLAG(ENABLE_GLIC)
  task_icon_state_change_callback_subscription_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterTaskIconStateChange(
              base::BindRepeating(&GlicActorTaskIconController::OnStateUpdate,
                                  base::Unretained(this))));
#endif
}

void GlicActorTaskIconController::UpdateCurrentTaskIconUiState() {
#if BUILDFLAG(ENABLE_GLIC)
  actor::ui::ActorUiStateManagerInterface::TaskIconUiState task_icon_state =
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->GetTaskIconUiState();
  auto* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  OnStateUpdate(task_icon_state,
                glic_keyed_service->window_controller().state(),
                glic_keyed_service->host().GetPrimaryCurrentView());
#endif
}

#if BUILDFLAG(ENABLE_GLIC)
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
        break;
      case glic::GlicWindowController::State::kClosed:
        tab_strip_action_container_->UnhighlightGlicActorTaskIcon();
        tab_strip_action_container_->UnhighlightGlicButton();
        break;
      case glic::GlicWindowController::State::kWaitingForGlicToLoad:
        break;
    }
  }
}
#endif

}  // namespace tabs
