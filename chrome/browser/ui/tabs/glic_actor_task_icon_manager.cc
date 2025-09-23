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
  callback_subscriptions_.push_back(
      window_controller_->RegisterFloatyStateChange(base::BindRepeating(
          &GlicActorTaskIconManager::OnFloatyUpdate, base::Unretained(this))));
  callback_subscriptions_.push_back(
      actor::ActorKeyedService::Get(profile_)
          ->GetActorUiStateManager()
          ->RegisterActorTaskStateChange(base::BindRepeating(
              &GlicActorTaskIconManager::OnActorTaskStateUpdate,
              base::Unretained(this))));
}

void GlicActorTaskIconManager::OnFloatyUpdate(
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView current_view) {
  UpdateTaskIcon(floaty_state, current_view);
}

void GlicActorTaskIconManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  // Reset suppression every time a new actor task state change occurs.
  suppress_task_icon_text_ = false;
  current_task_id_ = task_id;

  // Get the glic::GlicInstance associated with the task.
  glic::GlicInstance* instance =
      window_controller_->GetInstanceForTab(GetLastUpdatedTab());
  if (!instance) {
    return;
  }

  // TODO(crbug.com/445960367): Access all glic UI state through GlicInstance
  // instead.
  UpdateTaskIcon(window_controller_->state(),
                 instance->host().GetPrimaryCurrentView());
}

void GlicActorTaskIconManager::Shutdown() {}

void GlicActorTaskIconManager::UpdateTaskIcon(
    GlicWindowController::State floaty_state,
    CurrentView current_view) {
  auto active_tasks = actor_service_->GetActiveTasks();
  // TODO(crbug.com/431015299): Cache some of these values.
  auto completed_tasks = actor_service_->FindTaskIdsInInactive(
      base::BindRepeating(&IsRecentlyCompletedTask));
  auto paused_by_actor_tasks = actor_service_->FindTaskIdsInActive(
      base::BindRepeating([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      }));

  // If there are no active tasks and no recently completed tasks, we can hide
  // the task icon.
  if (active_tasks.empty() && completed_tasks.empty()) {
    current_actor_task_icon_state_ = {
        .is_visible = false,
        .text = ActorTaskIconState::Text::kDefault,
    };
    task_icon_state_change_callback_list_.Notify(
        floaty_state, current_view, current_actor_task_icon_state_);
    return;
  }

  // If the task isn't inactive, the task icon will always be visible.
  current_actor_task_icon_state_.is_visible = true;

  // If the text hasn't been suppressed, check if it should be suppressed.
  if (!suppress_task_icon_text_) {
    suppress_task_icon_text_ =
        (floaty_state == GlicWindowController::State::kOpen &&
         current_view == CurrentView::kActuation);
  }

  // Apply text state change.
  if (suppress_task_icon_text_) {
    current_actor_task_icon_state_.text = ActorTaskIconState::Text::kDefault;
  } else if (!paused_by_actor_tasks.empty()) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kNeedsAttention;
  } else if (!completed_tasks.empty()) {
    current_actor_task_icon_state_.text =
        ActorTaskIconState::Text::kCompleteTasks;
  }

  task_icon_state_change_callback_list_.Notify(floaty_state, current_view,
                                               current_actor_task_icon_state_);
}

base::CallbackListSubscription
GlicActorTaskIconManager::RegisterTaskIconStateChange(
    TaskIconStateChangeCallback callback) {
  return task_icon_state_change_callback_list_.Add(std::move(callback));
}

ActorTaskIconState GlicActorTaskIconManager::GetCurrentActorTaskIconState()
    const {
  return current_actor_task_icon_state_;
}

raw_ptr<tabs::TabInterface> GlicActorTaskIconManager::GetLastUpdatedTab() {
  if (!current_task_id_ || !actor_service_->GetTask(current_task_id_)) {
    return nullptr;
  }
  actor::ActorTask* task = actor_service_->GetTask(current_task_id_);

  actor::ActorTask::TabHandleSet tabs = task->GetLastActedTabs();

  // TODO(crbug.com/441064175): Will need to be updated for multi-tab actuation.
  DCHECK_LE(tabs.size(), 1ul);

  return tabs.empty() ? nullptr : tabs.begin()->Get();
}

}  // namespace tabs
