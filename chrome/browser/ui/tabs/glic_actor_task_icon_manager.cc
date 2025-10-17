// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/profiles/profile.h"

namespace tabs {
namespace {

// TODO(crbug.com/438204230): Remove this condition.
bool IsRecentlyCompletedTask(const actor::ActorTask& task) {
  bool is_finished = (task.GetState() == actor::ActorTask::State::kFinished);
  bool is_not_expired =
      (base::Time::Now() - task.GetEndTime() <
       base::Seconds(
           features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()));
  return is_finished && is_not_expired;
}

}  // namespace

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
}

void GlicActorTaskIconManager::OnInstanceStateChange(bool is_showing,
                                                     CurrentView current_view) {
  UpdateTaskIcon(is_showing, current_view);
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  current_task_id_ = task_id;

  // TODO(crbug.com/446734119): Instead ActorTask should hold a glic
  // InstanceId and use that to retrieve the instance.
  glic::GlicInstance* instance = window_controller_->GetInstances().front();
  if (!instance) {
    return;
  }

  if (features::kGlicActorUiNudgeRedesign.Get()) {
    UpdateTaskNudge();
  } else {
    UpdateTaskIcon(instance->IsShowing(),
                   instance->host().GetPrimaryCurrentView());
  }
}

void GlicActorTaskIconManager::Shutdown() {}

void GlicActorTaskIconManager::UpdateTaskIcon(bool is_showing,
                                              CurrentView current_view) {
  auto active_tasks = actor_service_->GetActiveTasks();
  // TODO(crbug.com/431015299): Cache some of these values.
  auto completed_tasks =
      actor_service_->FindTaskIdsInInactive(&IsRecentlyCompletedTask);
  auto paused_by_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      });

  // If there are no active tasks and no recently completed tasks, we can hide
  // the task icon.
  if (active_tasks.empty() && completed_tasks.empty()) {
    current_actor_task_icon_state_ = {
        .is_visible = false,
        .text = ActorTaskIconState::Text::kDefault,
    };
    task_icon_state_change_callback_list_.Notify(
        is_showing, current_view, current_actor_task_icon_state_);
    return;
  }

  // If the task isn't inactive, the task icon will always be visible.
  current_actor_task_icon_state_.is_visible = true;

  // Apply text state change.
  if (!paused_by_actor_tasks.empty()) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kNeedsAttention;
  } else if (!completed_tasks.empty()) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kCompleteTasks;
  } else {
    // If no tasks needing attention or completed, reset the icon.
    current_actor_task_icon_state_.text = ActorTaskIconState::Text::kDefault;
  }

  task_icon_state_change_callback_list_.Notify(is_showing, current_view,
                                               current_actor_task_icon_state_);
}

void GlicActorTaskIconManager::UpdateTaskNudge() {
  auto active_tasks = actor_service_->GetActiveTasks();
  // TODO(crbug.com/431015299): Cache some of these values.
  auto completed_tasks =
      actor_service_->FindTaskIdsInInactive(&IsRecentlyCompletedTask);
  auto paused_by_actor_tasks =
      actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      });

  if (!paused_by_actor_tasks.empty()) {
    current_actor_task_nudge_state_.text =
        ActorTaskNudgeState::Text::kNeedsAttention;
  } else if (!completed_tasks.empty()) {
    current_actor_task_nudge_state_.text =
        ActorTaskNudgeState::Text::kCompleteTasks;
  } else {
    // If no tasks needing attention or completed, hide the nudge.
    current_actor_task_nudge_state_.text = ActorTaskNudgeState::Text::kDefault;
  }

  task_nudge_state_change_callback_list_.Notify(
      current_actor_task_nudge_state_);
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
