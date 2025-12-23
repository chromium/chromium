// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace tabs {
using actor::ui::ActorTaskNudgeState;
using glic::GlicInstance;
using glic::GlicKeyedService;
using glic::GlicWindowController;
using glic::Host;
using glic::mojom::CurrentView;

DEFINE_USER_DATA(GlicActorNudgeController);
GlicActorNudgeController::GlicActorNudgeController(
    BrowserWindowInterface* browser,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(browser->GetProfile()),
      browser_(browser),
      tab_strip_action_container_(tab_strip_action_container),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterActorNudgeStateCallback();
    UpdateCurrentActorNudgeState();
  }
}

GlicActorNudgeController::~GlicActorNudgeController() = default;

// static
GlicActorNudgeController* GlicActorNudgeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void GlicActorNudgeController::OnStateUpdate(
    ActorTaskNudgeState actor_task_nudge_state) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GlicActorNudgeController::OnStateUpdateImpl,
                     weak_ptr_factory_.GetWeakPtr(), actor_task_nudge_state));
}

void GlicActorNudgeController::OnStateUpdateImpl(
    ActorTaskNudgeState actor_task_nudge_state) {
  ActorTaskListBubbleController* bubble_controller =
      ActorTaskListBubbleController::From(browser_);

  // If the task icon is inactive, hide it and perform no additional style
  // changes.
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator) &&
      actor_task_nudge_state.task_list_size < 1) {
    tab_strip_action_container_->HideGlicActorTaskIcon();
    return;
  }

  switch (actor_task_nudge_state.text) {
    case ActorTaskNudgeState::Text::kDefault:
      if (base::FeatureList::IsEnabled(
              features::kGlicActorUiGlobalTaskIndicator)) {
        tab_strip_action_container_->ShowGlicActorTaskIcon();
      } else {
        tab_strip_action_container_->HideGlicActorTaskIcon();
        // All bubbles should close when the nudge is hidden.
        if (bubble_controller->GetBubbleWidget()) {
          bubble_controller->GetBubbleWidget()->Close();
        }
      }
      break;
    case ActorTaskNudgeState::Text::kNeedsAttention:
      UpdateNudgeLabelOrRetrigger(l10n_util::GetPluralStringFUTF16(
          IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL,
          actor_task_nudge_state.task_list_size));
      break;
      // TODO(crbug.com/458391262) revisit or cleanup implementation here for
      // m144.
    case ActorTaskNudgeState::Text::kCompleteTasks:
      break;
    default:
      NOTREACHED();
  }

  if (tab_strip_action_container_->GetIsShowingGlicActorTaskIconNudge()) {
    actor::ui::RecordTaskNudgeShown(actor_task_nudge_state);
  }
}

void GlicActorNudgeController::UpdateNudgeLabelOrRetrigger(
    std::u16string nudge_label_text) {
  if (tab_strip_action_container_->GetIsShowingGlicActorTaskIconNudge()) {
    tab_strip_action_container_->glic_actor_task_icon()->ShowNudgeLabel(
        nudge_label_text);
  } else {
    tab_strip_action_container_->TriggerGlicActorNudge(nudge_label_text);
  }
  ActorTaskListBubbleController::From(browser_)->ShowBubble(
      tab_strip_action_container_->glic_actor_button_container());
}

void GlicActorNudgeController::RegisterActorNudgeStateCallback() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    actor_nudge_state_change_callback_subscription_.push_back(
        manager->RegisterTaskNudgeStateChange(base::BindRepeating(
            &GlicActorNudgeController::OnStateUpdate, base::Unretained(this))));
  }
}

void GlicActorNudgeController::UpdateCurrentActorNudgeState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    OnStateUpdate(manager->GetCurrentActorTaskNudgeState());
  }
}

}  // namespace tabs
