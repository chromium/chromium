// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/profiles/profile.h"

namespace {
bool ShouldDisplayInTaskListBubble(actor::ActorTask::State state) {
  return state == actor::ActorTask::State::kPausedByActor ||
         state == actor::ActorTask::State::kWaitingOnUser;
}
}  // namespace

namespace tabs {

using actor::ActorKeyedService;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using actor::ActorTask;
using glic::GlicWindowController;
using glic::Host;
using glic::mojom::CurrentView;

GlicActorTaskIconManager::GlicActorTaskIconManager(
    Profile* profile,
    actor::ActorKeyedService* actor_service,
    glic::GlicWindowController& window_controller)
    : profile_(profile),
      actor_service_(actor_service),
      window_controller_(window_controller) {
  CHECK(actor_service);
  RegisterSubscriptions();
}

GlicActorTaskIconManager::~GlicActorTaskIconManager() = default;

void GlicActorTaskIconManager::RegisterSubscriptions() {
  // Get the glic::GlicInstance associated with the task.
  glic::GlicInstance* instance =
      window_controller_->GetInstanceForTab(GetLastUpdatedTab());

  if (instance) {
    callback_subscriptions_.push_back(instance->RegisterStateChange(
        base::BindRepeating(&GlicActorTaskIconManager::OnInstanceStateChange,
                            base::Unretained(this))));
  }
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStateChange(base::BindRepeating(
              &GlicActorTaskIconManager::OnActorTaskStateUpdate,
              base::Unretained(this))));
  // TODO(crbug.com/458391262) revisit or cleanup implementation here for m144.
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStopped(
              base::BindRepeating(&GlicActorTaskIconManager::OnActorTaskStopped,
                                  base::Unretained(this))));
}

void GlicActorTaskIconManager::OnInstanceStateChange(bool is_showing,
                                                     CurrentView current_view) {
  UpdateTaskIcon(is_showing, current_view);
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  current_task_id_ = task_id;

  // TODO(crbug.com/444706814): Delete instance code once task icon path is
  // removed.
  glic::GlicInstance* instance = window_controller_->GetInstanceForTab(
      GetLastUpdatedTabForTaskId(task_id));

  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    UpdateTaskListBubble(task_id);
    UpdateTaskNudge();
  } else if (instance) {
    UpdateTaskIcon(instance->IsShowing(),
                   instance->host().GetPrimaryCurrentView());
  }
}

// TODO(crbug.com/458391262) revisit or cleanup implementation here for m144.
void GlicActorTaskIconManager::OnActorTaskStopped(
    actor::TaskId task_id,
    actor::ActorTask::State final_state,
    std::string task_title) {
  if (final_state == actor::ActorTask::State::kFinished) {
    has_unprocessed_completed_tasks_ = true;
  } else if (final_state == actor::ActorTask::State::kFailed) {
    has_unprocessed_failed_tasks_ = true;
  }
}

// TODO(crbug.com/458391262) revisit or cleanup implementation here for m144.
void GlicActorTaskIconManager::ClearStoppedTasks() {
  has_unprocessed_completed_tasks_ = false;
  has_unprocessed_failed_tasks_ = false;
  OnActorTaskStateUpdate(current_task_id_);
}

void GlicActorTaskIconManager::Shutdown() {}

void GlicActorTaskIconManager::UpdateTaskIcon(bool is_showing,
                                              CurrentView current_view) {
  auto active_tasks = actor_service_->GetActiveTasks();
  auto paused_or_yielded_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return (task.GetState() == actor::ActorTask::State::kPausedByActor ||
                task.GetState() == actor::ActorTask::State::kWaitingOnUser);
      });
  auto old_state = current_actor_task_icon_state_;
  // If there are no active tasks and no recently completed tasks, we can hide
  // the task icon.
  if (active_tasks.empty() && !has_unprocessed_completed_tasks_ &&
      !has_unprocessed_failed_tasks_) {
    current_actor_task_icon_state_ = {
        .is_visible = false,
        .text = ActorTaskIconState::Text::kDefault,
    };
    if (old_state != current_actor_task_icon_state_) {
      task_icon_state_change_callback_list_.Notify(
          is_showing, current_view, current_actor_task_icon_state_);
    }
    return;
  }

  // If the task isn't inactive, the task icon will always be visible.
  current_actor_task_icon_state_.is_visible = true;

  // Apply text state change.
  if (!paused_or_yielded_actor_tasks.empty() || has_unprocessed_failed_tasks_) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kNeedsAttention;
  } else if (has_unprocessed_completed_tasks_) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kCompleteTasks;
  } else {
    // If no tasks needing attention or completed, reset the icon.
    current_actor_task_icon_state_.text = ActorTaskIconState::Text::kDefault;
  }
  if (old_state != current_actor_task_icon_state_) {
    task_icon_state_change_callback_list_.Notify(
        is_showing, current_view, current_actor_task_icon_state_);
  }
}

void GlicActorTaskIconManager::UpdateTaskNudge() {
  auto active_tasks = actor_service_->GetActiveTasks();
  // TODO(b/440770955): Replace has_unprocessed_completed_tasks_ with a
  // snapshot (task title, state and tab handle) of the completed or failed
  // tasks for the pop-over.
  auto paused_or_yielded_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return (task.GetState() == actor::ActorTask::State::kPausedByActor ||
                task.GetState() == actor::ActorTask::State::kWaitingOnUser);
      });

  ActorTaskNudgeState old_state = current_actor_task_nudge_state_;
  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    if (!paused_or_yielded_actor_tasks.empty() &&
        !actor_task_list_bubble_rows_.empty()) {
      current_actor_task_nudge_state_.text =
          actor_task_list_bubble_rows_.size() > 1u
              ? ActorTaskNudgeState::Text::kMultipleTasksNeedAttention
              : ActorTaskNudgeState::Text::kNeedsAttention;
    } else {
      // If no tasks needing attention, hide the nudge.
      current_actor_task_nudge_state_.text =
          ActorTaskNudgeState::Text::kDefault;
    }
  }
  // TODO(crbug.com/458391262) revisit or cleanup implementation here for m144.
  else {
    if (!paused_or_yielded_actor_tasks.empty()) {
      current_actor_task_nudge_state_.text =
          ActorTaskNudgeState::Text::kNeedsAttention;
    } else if (has_unprocessed_completed_tasks_) {
      current_actor_task_nudge_state_.text =
          ActorTaskNudgeState::Text::kCompleteTasks;
    } else {
      // If no tasks needing attention or completed, hide the nudge.
      current_actor_task_nudge_state_.text =
          ActorTaskNudgeState::Text::kDefault;
    }
  }

  if (old_state != current_actor_task_nudge_state_) {
    task_nudge_state_change_callback_list_.Notify(
        current_actor_task_nudge_state_);
  }
}

void GlicActorTaskIconManager::RemoveRowFromTaskListBubble(
    actor::TaskId task_id) {
  actor_task_list_bubble_rows_.erase(task_id);
  UpdateTaskNudge();
}

void GlicActorTaskIconManager::UpdateTaskListBubble(actor::TaskId task_id) {
  if (actor::ActorTask* task = actor_service_->GetTask(task_id)) {
    if (ShouldDisplayInTaskListBubble(task->GetState())) {
      ActorTaskListBubbleRowState task_state = {.task_id = task_id,
                                                .title = task->title()};
      actor_task_list_bubble_rows_.insert({task_state.task_id, task_state});
      task_list_bubble_change_callback_list_.Notify(task_id);
      return;
    }
  }
  // Stopped ActorTasks will be cleared immediately so can safely remove.
  actor_task_list_bubble_rows_.erase(task_id);
}

base::CallbackListSubscription
GlicActorTaskIconManager::RegisterTaskIconStateChange(
    TaskIconStateChangeCallback callback) {
  return task_icon_state_change_callback_list_.Add(std::move(callback));
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

ActorTaskIconState GlicActorTaskIconManager::GetCurrentActorTaskIconState()
    const {
  return current_actor_task_icon_state_;
}

ActorTaskNudgeState GlicActorTaskIconManager::GetCurrentActorTaskNudgeState()
    const {
  return current_actor_task_nudge_state_;
}

// TODO(crbug.com/431015299): Clean up after redesign is launched.
raw_ptr<tabs::TabInterface> GlicActorTaskIconManager::GetLastUpdatedTab() {
  if (!current_task_id_ || !actor_service_->GetTask(current_task_id_)) {
    return nullptr;
  }
  actor::ActorTask* task = actor_service_->GetTask(current_task_id_);

  actor::ActorTask::TabHandleSet tabs = task->GetLastActedTabs();

  // TODO(crbug.com/441064175): Will need to be updated for multi-tab actuation.
  return tabs.empty() ? nullptr : tabs.begin()->Get();
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
