// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace tabs {
using actor::ui::ActorTaskNudgeState;
using glic::GlicInstance;
using glic::GlicKeyedService;
using glic::GlicWindowController;
using glic::Host;

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
  }

    ActorTaskListBubbleController* bubble_controller =
        ActorTaskListBubbleController::From(browser_);
    bubble_visibility_change_subscription_.push_back(
        bubble_controller->RegisterBubbleShownCallback(base::BindRepeating(
            &GlicActorNudgeController::OnBubbleVisibilityChange,
            weak_ptr_factory_.GetWeakPtr(), /*is_bubble_open=*/true)));
    bubble_visibility_change_subscription_.push_back(
        bubble_controller->RegisterBubbleDestroyedCallback(base::BindRepeating(
            &GlicActorNudgeController::OnBubbleVisibilityChange,
            weak_ptr_factory_.GetWeakPtr(), /*is_bubble_open=*/false)));
}

GlicActorNudgeController::~GlicActorNudgeController() = default;

// static
GlicActorNudgeController* GlicActorNudgeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void GlicActorNudgeController::OnStateUpdate(
    bool show_bubble,
    ActorTaskNudgeState actor_task_nudge_state) {
  // If the task icon is inactive, hide it and perform no additional style
  // changes.
  GlicActorTaskIconManager* manager =
      GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  DCHECK(manager);
  if (manager->actor_task_list_bubble_rows().empty()) {
    tab_strip_action_container_->HideGlicActorTaskIcon();
    CloseBubble();
    return;
  }

  size_t num_tasks_need_processing = manager->GetNumActorTasksNeedProcessing();
  switch (actor_task_nudge_state.text) {
    case ActorTaskNudgeState::Text::kDefault:
        tab_strip_action_container_->ShowGlicActorTaskIcon();
      // In either case, close the bubble as the nudge has been either hidden or
      // reset.
      CloseBubble();
      break;
    case ActorTaskNudgeState::Text::kNeedsAttention:
      UpdateNudgeLabelOrRetrigger(
          l10n_util::GetPluralStringFUTF16(
              IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, num_tasks_need_processing),
          show_bubble);
      break;
    case ActorTaskNudgeState::Text::kCompleteTasks:
      UpdateNudgeLabelOrRetrigger(l10n_util::GetPluralStringFUTF16(
                                      IDS_ACTOR_TASK_NUDGE_TASK_COMPLETE_LABEL,
                                      actor::ActorKeyedService::Get(profile_)
                                          ->GetActorUiStateManager()
                                          ->GetInactiveTaskCount()),
                                  show_bubble);
      break;
    default:
      NOTREACHED();
  }

  if (tab_strip_action_container_->GetIsShowingGlicActorTaskIconNudge()) {
      actor::ui::RecordGlobalTaskIndicatorNudgeShown(actor_task_nudge_state);
  }
}

void GlicActorNudgeController::UpdateNudgeLabelOrRetrigger(
    std::u16string nudge_label_text,
    bool show_bubble) {
  if (tab_strip_action_container_->GetIsShowingGlicActorTaskIconNudge()) {
    tab_strip_action_container_->glic_actor_task_icon()->ShowNudgeLabel(
        nudge_label_text);
  } else {
    tab_strip_action_container_->TriggerGlicActorNudge(nudge_label_text);
  }
  if (show_bubble) {
    ActorTaskListBubbleController::From(browser_)->ShowBubble(
        tab_strip_action_container_->glic_actor_task_icon());
  }
}

void GlicActorNudgeController::RegisterActorNudgeStateCallback() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    actor_nudge_state_change_callback_subscription_.push_back(
        manager->RegisterTaskNudgeStateChange(
            base::BindRepeating(&GlicActorNudgeController::OnStateUpdate,
                                base::Unretained(this), /*show_bubble=*/true)));
  }
}

void GlicActorNudgeController::UpdateCurrentActorNudgeState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    // This will "sync" a new window's state to the current nudge state. Do not
    // show the bubble in the new window as the user navigated away from the
    // bubble that was previously shown.
    OnStateUpdate(/*show_bubble=*/false,
                  manager->GetCurrentActorTaskNudgeState());
  }
}

void GlicActorNudgeController::CloseBubble() {
  ActorTaskListBubbleController* bubble_controller =
      ActorTaskListBubbleController::From(browser_);
  if (bubble_controller->GetBubbleWidget()) {
    bubble_controller->GetBubbleWidget()->Close();
  }
}

void GlicActorNudgeController::OnBubbleVisibilityChange(bool is_bubble_open) {
  tab_strip_action_container_->glic_actor_task_icon()->SetPressedColor(
      is_bubble_open);
}

}  // namespace tabs
