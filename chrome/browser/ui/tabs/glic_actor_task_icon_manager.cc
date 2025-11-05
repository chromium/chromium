// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/profiles/profile.h"

namespace tabs {

using actor::ActorKeyedService;
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
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskCompleted(base::BindRepeating(
              [](GlicActorTaskIconManager* icon_manager, actor::TaskId task_id,
                 actor::ActorTask::State final_state, std::string title) {
                // We have a wrapper function because the header can't depend
                // on actor::ActorTask.
                icon_manager->OnActorTaskCompleted(
                    task_id, final_state == actor::ActorTask::State::kFinished);
              },
              base::Unretained(this))));
}

void GlicActorTaskIconManager::OnInstanceStateChange(bool is_showing,
                                                     CurrentView current_view) {
  UpdateTaskIcon(is_showing, current_view);
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  current_task_id_ = task_id;

  // TODO(crbug.com/446734119): Instead ActorTask should hold a glic
  // InstanceId and use that to retrieve the instance.
  std::vector<glic::GlicInstance*> instances =
      window_controller_->GetInstances();
  if (instances.empty()) {
    return;
  }
  glic::GlicInstance* instance = instances.front();
  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    UpdateTaskNudge();
  } else {
    UpdateTaskIcon(instance->IsShowing(),
                   instance->host().GetPrimaryCurrentView());
  }
}

void GlicActorTaskIconManager::OnActorTaskCompleted(actor::TaskId task_id,
                                                    bool success) {
  if (!success) {
    return;
  }
  has_unprocessed_completed_tasks_ = true;
}

void GlicActorTaskIconManager::ClearCompletedTasks() {
  has_unprocessed_completed_tasks_ = false;
  OnActorTaskStateUpdate(current_task_id_);
}

void GlicActorTaskIconManager::Shutdown() {}

void GlicActorTaskIconManager::UpdateTaskIcon(bool is_showing,
                                              CurrentView current_view) {
  auto active_tasks = actor_service_->GetActiveTasks();

  // TODO(b/440770955): Replace has_unprocessed_completed_tasks_ with a
  // snapshot (task title, state and tab handle) of the completed or failed
  // tasks for the pop-over.
  bool has_recently_completed_tasks = has_unprocessed_completed_tasks_;
  auto paused_or_yielded_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return (task.GetState() == ActorTask::State::kPausedByActor ||
                task.GetState() == ActorTask::State::kWaitingOnUser);
      });
  auto old_state = current_actor_task_icon_state_;
  // If there are no active tasks and no recently completed tasks, we can hide
  // the task icon.
  if (active_tasks.empty() && !has_recently_completed_tasks) {
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
  if (!paused_or_yielded_actor_tasks.empty()) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kNeedsAttention;
  } else if (has_recently_completed_tasks) {
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
  bool has_recently_completed_tasks = has_unprocessed_completed_tasks_;
  auto paused_or_yielded_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return (task.GetState() == ActorTask::State::kPausedByActor ||
                task.GetState() == ActorTask::State::kWaitingOnUser);
      });

  ActorTaskNudgeState old_state = current_actor_task_nudge_state_;
  if (!paused_or_yielded_actor_tasks.empty()) {
    current_actor_task_nudge_state_.text =
        ActorTaskNudgeState::Text::kNeedsAttention;
  } else if (has_recently_completed_tasks) {
    current_actor_task_nudge_state_.text =
        ActorTaskNudgeState::Text::kCompleteTasks;
  } else {
    // If no tasks needing attention or completed, hide the nudge.
    current_actor_task_nudge_state_.text = ActorTaskNudgeState::Text::kDefault;
  }

  if (old_state != current_actor_task_nudge_state_) {
    task_nudge_state_change_callback_list_.Notify(
        current_actor_task_nudge_state_);
  }
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

ActorTaskIconState GlicActorTaskIconManager::GetCurrentActorTaskIconState()
    const {
  return current_actor_task_icon_state_;
}

ActorTaskNudgeState GlicActorTaskIconManager::GetCurrentActorTaskNudgeState()
    const {
  return current_actor_task_nudge_state_;
}

raw_ptr<tabs::TabInterface> GlicActorTaskIconManager::GetLastUpdatedTab() {
  if (!current_task_id_ || !actor_service_->GetTask(current_task_id_)) {
    return nullptr;
  }
  actor::ActorTask* task = actor_service_->GetTask(current_task_id_);

  actor::ActorTask::TabHandleSet tabs = task->GetLastActedTabs();

  // TODO(crbug.com/441064175): Will need to be updated for multi-tab actuation.
  return tabs.empty() ? nullptr : tabs.begin()->Get();
}

}  // namespace tabs
