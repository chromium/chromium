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
    RegisterFloatyTaskStateCallback();
    UpdateCurrentUiState();
  }
}

GlicActorTaskIconController::~GlicActorTaskIconController() = default;

void GlicActorTaskIconController::RegisterFloatyTaskStateCallback() {
#if BUILDFLAG(ENABLE_GLIC)
  floaty_task_state_change_callback_subscription_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterFloatyTaskStateChange(
              base::BindRepeating(&GlicActorTaskIconController::OnStateUpdate,
                                  base::Unretained(this))));
#endif
}

void GlicActorTaskIconController::UpdateCurrentUiState() {
#if BUILDFLAG(ENABLE_GLIC)
  actor::ui::ActorUiStateManagerInterface::UiState ui_state =
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->GetUiState();
  auto* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  OnStateUpdate(ui_state, glic_keyed_service->window_controller().state(),
                glic_keyed_service->host().GetPrimaryCurrentView());
#endif
}

#if BUILDFLAG(ENABLE_GLIC)
void GlicActorTaskIconController::OnStateUpdate(
    actor::ui::ActorUiStateManagerInterface::UiState task_state,
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView floaty_view) {
  switch (task_state) {
    case actor::ui::ActorUiStateManagerInterface::UiState::kActive:
      tab_strip_action_container_->ShowGlicActorTaskIcon();
      break;
    case actor::ui::ActorUiStateManagerInterface::UiState::kCheckTasks:
      tab_strip_action_container_->TriggerGlicActorTaskIconCheckTasksNudge();
      break;
    case actor::ui::ActorUiStateManagerInterface::UiState::kCompleteTasks:
      tab_strip_action_container_->TriggerGlicActorTaskIconCompleteTasksNudge();
      break;
    case actor::ui::ActorUiStateManagerInterface::UiState::kInactive:
      tab_strip_action_container_->HideGlicActorTaskIcon();
      break;
  }

  if (task_state !=
      actor::ui::ActorUiStateManagerInterface::UiState::kInactive) {
    switch (floaty_state) {
      case glic::GlicWindowController::State::kOpen:
        if (floaty_view == glic::mojom::CurrentView::kConversation &&
            task_state !=
                actor::ui::ActorUiStateManagerInterface::UiState::kInactive) {
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
