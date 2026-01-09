// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
namespace tabs {
namespace {
using actor::ActorKeyedService;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using actor::ActorTask;
using TaskState = actor::ActorTask::State;
using Text = ActorTaskNudgeState::Text;

bool RequiresTaskProcessing(TaskState state) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    return GlicActorTaskIconManager::RequiresAttention(state) ||
           state == TaskState::kFinished || state == TaskState::kFailed;
  } else {
    return GlicActorTaskIconManager::RequiresAttention(state);
  }
}

}  // namespace

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
        return (task.GetState() == TaskState::kPausedByActor ||
                task.GetState() == TaskState::kWaitingOnUser);
      });

  ActorTaskNudgeState old_state = current_actor_task_nudge_state_;

  bool needs_attention = false;
  bool tasks_complete = false;
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    for (const auto [task_id, requires_processing] :
         actor_task_list_bubble_rows_) {
      // Tasks that are processed will show the default nudge.
      if (!requires_processing) {
        continue;
      }

      const std::optional<TaskState> state =
          actor_service_->GetActorUiStateManager()->GetActorTaskState(task_id);

      // Tasks that have no state no longer exist and should not be processed.
      if (!state) {
        actor::ui::RecordTaskIconError(
            actor::ui::ActorUiTaskIconError::kNudgeTaskDoesntExist);
        continue;
      }

      if (tabs::GlicActorTaskIconManager::RequiresAttention(*state)) {
        // Needs attention prioritized over other text
        needs_attention = true;
        break;
      }

      if (*state == TaskState::kFinished || *state == TaskState::kFailed) {
        tasks_complete = true;
      }
    }
  } else {
    needs_attention = !paused_or_yielded_actor_tasks.empty() &&
                      !actor_task_list_bubble_rows_.empty();
  }

  current_actor_task_nudge_state_.text = needs_attention ? Text::kNeedsAttention
                                         : tasks_complete ? Text::kCompleteTasks
                                                          : Text::kDefault;

  // If either the state or number of tasks in the bubble changes, we want to
  // notify the nudge.
  if (old_state != current_actor_task_nudge_state_ ||
      stored_bubble_row_task_count_ != actor_task_list_bubble_rows_.size()) {
    stored_bubble_row_task_count_ = actor_task_list_bubble_rows_.size();
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
  if (!state.has_value() || state.value() == ActorTask::State::kCancelled) {
    // If there is no value for the state, this means the task does not exist so
    // we should remove it.
    // If the task was cancelled, it should also be removed from the bubble.
    actor_task_list_bubble_rows_.erase(task_id);
  } else {
    const bool icon_v3_enabled =
        base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator);
    const bool requires_processing = RequiresTaskProcessing(state.value());

    if (icon_v3_enabled) {
      actor_task_list_bubble_rows_[task_id] = requires_processing;
    }
    if (requires_processing) {
      if (!icon_v3_enabled) {
        // Old implementation does not use this field, but it needs to be set to
        // show the row in the bubble.
        actor_task_list_bubble_rows_[task_id] = false;
      }
      // Notify the bubble only if a task now requires processing. This callback
      // will open the task list bubble and make it active, in order to bring it
      // to the user's attention. This is also necessary for when a user
      // switches windows in order to show the bubble in the active window.
      task_list_bubble_change_callback_list_.Notify();
    }
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
size_t GlicActorTaskIconManager::GetNumActorTasksNeedProcessing() const {
  return std::ranges::count_if(
      actor_task_list_bubble_rows_,
      [](const auto& task) { return /*requires_processing=*/task.second; });
}

// static
bool GlicActorTaskIconManager::RequiresAttention(TaskState state) {
  return state == TaskState::kPausedByActor ||
         state == TaskState::kWaitingOnUser;
}

}  // namespace tabs
