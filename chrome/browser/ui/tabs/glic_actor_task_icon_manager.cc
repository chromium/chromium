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
  // TODO(crbug.com/458391262) revisit or cleanup implementation here for m144.
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStopped(
              base::BindRepeating(&GlicActorTaskIconManager::OnActorTaskStopped,
                                  base::Unretained(this))));
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  current_task_id_ = task_id;
  UpdateTaskListBubble(task_id);
  UpdateTaskNudge();
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

  current_actor_task_nudge_state_.task_list_size =
      actor_task_list_bubble_rows_.size();
  if (!paused_or_yielded_actor_tasks.empty() &&
      !actor_task_list_bubble_rows_.empty()) {
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
