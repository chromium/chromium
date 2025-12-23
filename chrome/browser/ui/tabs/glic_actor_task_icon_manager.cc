// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace {
// TODO(crbug.com/469817191): Add failed and finished states here as well to be
// processed.
bool RequiresTaskProcessing(actor::ActorTask::State state) {
  return state == actor::ActorTask::State::kPausedByActor ||
         state == actor::ActorTask::State::kWaitingOnUser;
}
}  // namespace

namespace tabs {

using actor::ActorKeyedService;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using actor::ActorTask;

GlicActorTaskIconManager::GlicActorTaskIconManager(
    Profile* profile,
    actor::ActorKeyedService* actor_service)
    : profile_(profile), actor_service_(actor_service) {
  CHECK(actor_service);
  RegisterSubscriptions();
}

GlicActorTaskIconManager::~GlicActorTaskIconManager() = default;

void GlicActorTaskIconManager::RegisterSubscriptions() {
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStateChange(base::BindRepeating(
              &GlicActorTaskIconManager::OnActorTaskStateUpdate,
              base::Unretained(this))));
  // TODO(crbug.com/469817191): Stopped tasks should also be added to the
  // popover.
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStopped(
              base::BindRepeating(&GlicActorTaskIconManager::OnActorTaskStopped,
                                  base::Unretained(this))));
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    callback_subscriptions_.push_back(
        actor::ActorKeyedService::Get(profile_)
            ->GetActorUiStateManager()
            ->RegisterActorTaskRemoved(base::BindRepeating(
                &GlicActorTaskIconManager::OnActorTaskRemoved,
                base::Unretained(this))));
  }
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  current_task_id_ = task_id;
  UpdateTaskListBubble(task_id);
  UpdateTaskNudge();
}


void GlicActorTaskIconManager::OnActorTaskRemoved(actor::TaskId task_id) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicActorUiGlobalTaskIndicator)) {
    return;
  }
  // TODO(crbug.com/470106502): Implement.
}
void GlicActorTaskIconManager::OnActorTaskStopped(actor::TaskId task_id) {
  // TODO(crbug.com/470106502): Implement.
}

void GlicActorTaskIconManager::Shutdown() {}

void GlicActorTaskIconManager::UpdateTaskNudge() {
  // TODO(mjenn): Remove this once kGlicActorUiGlobalTaskIndicator is removed.
  auto paused_or_yielded_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return (task.GetState() == actor::ActorTask::State::kPausedByActor ||
                task.GetState() == actor::ActorTask::State::kWaitingOnUser);
      });

  ActorTaskNudgeState old_state = current_actor_task_nudge_state_;

  current_actor_task_nudge_state_.task_list_size =
      actor_task_list_bubble_rows_.size();

  // TODO(crbug.com/469817191): Accommodate completed/failed tasks.
  bool has_unprocessed_tasks = std::any_of(
      actor_task_list_bubble_rows_.begin(), actor_task_list_bubble_rows_.end(),
      [](const auto& pair) { return pair.second.requires_processing; });

  bool needs_attention =
      base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)
          ? has_unprocessed_tasks
          : !paused_or_yielded_actor_tasks.empty() &&
                !actor_task_list_bubble_rows_.empty();
  if (needs_attention) {
    current_actor_task_nudge_state_.text =
        ActorTaskNudgeState::Text::kNeedsAttention;
  } else {
    // If no tasks needing attention, hide the nudge.
    current_actor_task_nudge_state_.text = ActorTaskNudgeState::Text::kDefault;
  }

  if (old_state != current_actor_task_nudge_state_) {
    task_nudge_state_change_callback_list_.Notify(
        current_actor_task_nudge_state_);
  }
}

void GlicActorTaskIconManager::ProcessRowInTaskListBubble(
    actor::TaskId task_id) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    actor_task_list_bubble_rows_.find(task_id)->second.requires_processing =
        false;
  } else {
    actor_task_list_bubble_rows_.erase(task_id);
  }
  UpdateTaskNudge();
}

void GlicActorTaskIconManager::UpdateTaskListBubble(actor::TaskId task_id) {
  if (actor::ActorTask* task = actor_service_->GetTask(task_id)) {
    if (base::FeatureList::IsEnabled(
            features::kGlicActorUiGlobalTaskIndicator)) {
      ActorTaskListBubbleRowState task_state = {
          .task_id = task_id,
          .title = task->title(),
          .requires_processing = RequiresTaskProcessing(task->GetState())};
      actor_task_list_bubble_rows_[task_id] = task_state;
    } else if (RequiresTaskProcessing(task->GetState())) {
      ActorTaskListBubbleRowState task_state = {.task_id = task_id,
                                                .title = task->title()};
      actor_task_list_bubble_rows_.insert({task_state.task_id, task_state});
    }
    task_list_bubble_change_callback_list_.Notify(task_id);
    return;
  }
  // TODO(crbug.com/470106502): In the new path, we only remove
  // stopped tasks on a timer. Otherwise they should remain in the popover.
  if (!base::FeatureList::IsEnabled(
          features::kGlicActorUiGlobalTaskIndicator)) {
    actor_task_list_bubble_rows_.erase(task_id);
  }
}

base::CallbackListSubscription
GlicActorTaskIconManager::RegisterTaskNudgeStateChange(
    TaskNudgeChangeCallback callback) {
  return task_nudge_state_change_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicActorTaskIconManager::RegisterTaskListBubbleStateChange(
    TaskListBubbleChangeCallback callback) {
  return task_list_bubble_change_callback_list_.Add(std::move(callback));
}

ActorTaskNudgeState GlicActorTaskIconManager::GetCurrentActorTaskNudgeState()
    const {
  return current_actor_task_nudge_state_;
}

raw_ptr<tabs::TabInterface>
GlicActorTaskIconManager::GetLastUpdatedTabForTaskId(actor::TaskId task_id) {
  if (ActorTask* task = actor_service_->GetTask(task_id)) {
    actor::ActorTask::TabHandleSet tabs = task->GetLastActedTabs();
    // TODO(crbug.com/441064175): Will need to be updated for multi-tab
    // actuation.
    return tabs.empty() ? nullptr : tabs.begin()->Get();
  }
  return nullptr;
}

}  // namespace tabs
