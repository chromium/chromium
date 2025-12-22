// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_

#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/common/actor/task_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace actor {
class ActorKeyedService;
}  // namespace actor

class Profile;

namespace tabs {

struct ActorTaskListBubbleRowState {
  actor::TaskId task_id;
  std::string title;
  // If this row requires processing. A row is only processed when it has been
  // clicked on by the user. If the row does not need user attention it will not
  // require processing.
  bool requires_processing;
};

class GlicActorTaskIconManager : public KeyedService {
 public:
  GlicActorTaskIconManager(Profile* profile,
                           actor::ActorKeyedService* actor_service);
  ~GlicActorTaskIconManager() override;

  // Called whenever actor task state updates.
  void OnActorTaskStateUpdate(actor::TaskId task_id);

  // Called whenever an actor task is completed.
  void OnActorTaskStopped(actor::TaskId task_id,
                          actor::ActorTask::State final_state,
                          std::string task_title);

  // Determines the state the task nudge should be in.
  void UpdateTaskNudge();

  // Determines the state of a task to show in the task list bubble.
  void UpdateTaskListBubble(actor::TaskId task_id);

  // Register for this callback to get task nudge state change notifications.
  using TaskNudgeChangeCallback = base::RepeatingCallback<void(
      actor::ui::ActorTaskNudgeState actor_task_nudge_state)>;
  base::CallbackListSubscription RegisterTaskNudgeStateChange(
      TaskNudgeChangeCallback callback);

  // Register for this callback to get task state change notifications for the
  // bubble.
  using TaskListBubbleChangeCallback =
      base::RepeatingCallback<void(actor::TaskId task_id)>;
  base::CallbackListSubscription RegisterTaskListBubbleStateChange(
      TaskListBubbleChangeCallback callback);

  actor::ui::ActorTaskNudgeState GetCurrentActorTaskNudgeState() const;

  raw_ptr<tabs::TabInterface> GetLastUpdatedTabForTaskId(actor::TaskId task_id);

  void ClearStoppedTasks();

  std::map<actor::TaskId, ActorTaskListBubbleRowState>
  GetActorTaskListBubbleRows() const {
    return actor_task_list_bubble_rows_;
  }

  // Callback to process a row in the task list bubble when it is clicked.
  // The nudge should be visible until all task rows have been processed.
  void ProcessRowInTaskListBubble(actor::TaskId task_id);

  // KeyedService:
  void Shutdown() override;

 private:
  // Called once on startup.
  void RegisterSubscriptions();

  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  using TaskNudgeChangeCallbackList = base::RepeatingCallbackList<void(
      actor::ui::ActorTaskNudgeState actor_task_nudge_text)>;
  TaskNudgeChangeCallbackList task_nudge_state_change_callback_list_;

  using TaskListBubbleChangeCallbackList =
      base::RepeatingCallbackList<void(actor::TaskId task_id)>;
  TaskListBubbleChangeCallbackList task_list_bubble_change_callback_list_;

  actor::ui::ActorTaskNudgeState current_actor_task_nudge_state_;

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_service_;

  // TODO(mjenn): Update implementation for multi-tab actuation.
  actor::TaskId current_task_id_;

  // TODO(b/440770955): Replace complete task lists (complete + fail) with a
  // snapshot (task title, state and tab handle) of the completed or failed
  // tasks for the pop-over.
  bool has_unprocessed_completed_tasks_ = false;
  // Whether there is an unprocessed failed task.
  bool has_unprocessed_failed_tasks_ = false;

  // Map of tasks needing notifications.
  std::map<actor::TaskId, ActorTaskListBubbleRowState>
      actor_task_list_bubble_rows_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
