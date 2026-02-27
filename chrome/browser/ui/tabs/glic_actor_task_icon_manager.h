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
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace actor {
class ActorKeyedService;
}  // namespace actor

class Profile;

namespace tabs {

class GlicActorTaskIconManager : public KeyedService {
 public:
  GlicActorTaskIconManager(Profile* profile,
                           actor::ActorKeyedService* actor_service);
  ~GlicActorTaskIconManager() override;

  // Called whenever actor task state updates.
  void OnActorTaskStateUpdate(actor::TaskId task_id);

  // Called whenever updates are needed to the task icon components.
  void UpdateTaskIconComponents(actor::TaskId task_id);

  // Returns true if the task was paused by the actor or in an interrupt state
  // waiting for user action.
  static bool RequiresAttention(actor::ActorTask::State state);

  // Returns true if the task requires attention, was recently completed, or
  // failed.
  static bool RequiresTaskProcessing(actor::ActorTask::State state);

  // Register for this callback to get task nudge state change notifications.
  using TaskNudgeChangeCallback = base::RepeatingCallback<void(
      actor::ui::ActorTaskNudgeState actor_task_nudge_state)>;
  base::CallbackListSubscription RegisterTaskNudgeStateChange(
      TaskNudgeChangeCallback callback);

  // Register for this callback to get task state change notifications for the
  // bubble.
  using TaskListBubbleChangeCallback = base::RepeatingCallback<void()>;
  base::CallbackListSubscription RegisterTaskListBubbleStateChange(
      TaskListBubbleChangeCallback callback);

  actor::ui::ActorTaskNudgeState GetCurrentActorTaskNudgeState() const;
  size_t GetNumActorTasksNeedProcessing() const;
  const absl::flat_hash_map<actor::TaskId, bool>& actor_task_list_bubble_rows()
      const {
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

  // Determines the state the task nudge should be in.
  void UpdateTaskNudge();

  // Determines the state of a task to show in the task list bubble.
  void UpdateTaskListBubble(actor::TaskId task_id);

  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  using TaskNudgeChangeCallbackList = base::RepeatingCallbackList<void(
      actor::ui::ActorTaskNudgeState actor_task_nudge_text)>;
  TaskNudgeChangeCallbackList task_nudge_state_change_callback_list_;

  using TaskListBubbleChangeCallbackList = base::RepeatingCallbackList<void()>;
  TaskListBubbleChangeCallbackList task_list_bubble_change_callback_list_;

  actor::ui::ActorTaskNudgeState current_actor_task_nudge_state_;
  // TODO(b/482378011): stored_bubble_row_task_count_ be removed once nudge
  // fixes are enabled without any issues.
  size_t stored_bubble_row_task_count_ = 0;
  size_t stored_bubble_row_need_processing_task_count_ = 0;
  size_t stored_bubble_row_inactive_task_count_ = 0;

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_service_;

  // Map of tasks needing notifications. `requires_proccessing` tracks if this
  // row requires processing. A row is only processed when it has been clicked
  // on by the user. If the row does not need user attention it will not require
  // processing.
  absl::flat_hash_map<actor::TaskId, /* requires_processing */ bool>
      actor_task_list_bubble_rows_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
