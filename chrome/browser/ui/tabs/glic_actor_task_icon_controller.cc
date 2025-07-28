// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"

namespace tabs {

GlicActorTaskIconController::GlicActorTaskIconController(
    Profile* profile,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(profile),
      tab_strip_action_container_(tab_strip_action_container) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterFloatyTaskStateCallback();
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
  // TODO(crbug.com/422439520): Call GetUiState() and update current window to
  // maintain consistency across multiple windows.
#endif
}

#if BUILDFLAG(ENABLE_GLIC)
void GlicActorTaskIconController::OnStateUpdate(
    actor::ui::ActorUiStateManagerInterface::UiState task_state,
    glic::GlicWindowController::State floaty_state) {
  switch (task_state) {
    case actor::ui::ActorUiStateManagerInterface::UiState::kActive:
      tab_strip_action_container_->ShowGlicActorTaskIcon();
      break;
    case actor::ui::ActorUiStateManagerInterface::UiState::kCheckTasks:
      tab_strip_action_container_->TriggerGlicActorTaskIconCheckTasksNudge();
      break;
    case actor::ui::ActorUiStateManagerInterface::UiState::kInactive:
      tab_strip_action_container_->HideGlicActorTaskIcon();
      break;
  }

  switch (floaty_state) {
    // Floaty state will only ever be sent if a task is not inactive (so if
    // the Task Icon is already open).
    case glic::GlicWindowController::State::kOpen:
      // TODO(crbug.com/422439931): Highlight Gemini icon.
    case glic::GlicWindowController::State::kClosed:
      // TODO(crbug.com/422439931): Unhighlight Gemini icon.
    case glic::GlicWindowController::State::kWaitingForGlicToLoad:
      break;
  }
}
#endif

}  // namespace tabs
