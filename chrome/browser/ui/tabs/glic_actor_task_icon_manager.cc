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
bool RequiresTaskProcessing(actor::ActorTask::State state) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    return state == actor::ActorTask::State::kPausedByActor ||
           state == actor::ActorTask::State::kWaitingOnUser ||
           state == actor::ActorTask::State::kFinished ||
           state == actor::ActorTask::State::kFailed;
  } else {
    return state == actor::ActorTask::State::kPausedByActor ||
           state == actor::ActorTask::State::kWaitingOnUser;
  }
}
}  // namespace

namespace tabs {

using actor::ActorKeyedService;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using actor::ActorTask;

GlicActorTaskIconManager::GlicActorTaskIconManager(
    Profile* profile,
    ActorKeyedService* actor_service)
    : profile_(profile), actor_service_(actor_service) {
  CHECK(actor_service);
  RegisterSubscriptions();
}

GlicActorTaskIconManager::~GlicActorTaskIconManager() = default;

void GlicActorTaskIconManager::RegisterSubscriptions() {
  callback_subscriptions_.push_back(
      actor_service_->GetActorUiStateManager()->RegisterActorTaskStateChange(
          base::BindRepeating(&GlicActorTaskIconManager::OnActorTaskStateUpdate,
                              base::Unretained(this))));
  callback_subscriptions_.push_back(
      actor_service_->GetActorUiStateManager()->RegisterActorTaskStopped(
          base::BindRepeating(
              &GlicActorTaskIconManager::UpdateTaskIconComponents,
              base::Unretained(this))));
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    callback_subscriptions_.push_back(
        actor_service_->GetActorUiStateManager()->RegisterActorTaskRemoved(
            base::BindRepeating(
                &GlicActorTaskIconManager::UpdateTaskIconComponents,
                base::Unretained(this))));
  }
}

void GlicActorTaskIconManager::UpdateTaskIconComponents(actor::TaskId task_id) {
  UpdateTaskListBubble(task_id);
  UpdateTaskNudge();
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  actor::ActorTask* task = actor_service_->GetTask(task_id);
  if (!task) {
    return;
  }
  UpdateTaskIconComponents(task_id);
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

  // TODO(crbug.com/469817191): Separate tasks that need attention from those
  // that are stopped.
  bool has_unprocessed_tasks = std::any_of(
      actor_task_list_bubble_rows_.begin(), actor_task_list_bubble_rows_.end(),
      [](const auto& pair) { return pair.second; });

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
    if (auto it = actor_task_list_bubble_rows_.find(task_id);
        it != actor_task_list_bubble_rows_.end()) {
      it->second = false;
    }
  } else {
    actor_task_list_bubble_rows_.erase(task_id);
  }
  UpdateTaskNudge();
}

void GlicActorTaskIconManager::UpdateTaskListBubble(actor::TaskId task_id) {
  const auto state =
      actor_service_->GetActorUiStateManager()->GetActorTaskState(task_id);
  if (!state.has_value()) {
    // If there is no value for the state, this means the task does not exist so
    // we should remove it.
    actor_task_list_bubble_rows_.erase(task_id);
  } else {
    const bool icon_v3_enabled =
        base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator);
    const bool requires_processing = RequiresTaskProcessing(state.value());

    if (icon_v3_enabled) {
      actor_task_list_bubble_rows_[task_id] = requires_processing;
    } else if (requires_processing) {
      // Old implementation does not use this field.
      actor_task_list_bubble_rows_[task_id] = false;
    }
  }
  task_list_bubble_change_callback_list_.Notify();
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

}  // namespace tabs
