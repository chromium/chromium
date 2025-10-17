// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_

#include <string>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/actor/task_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace actor {
class ActorKeyedService;
}  // namespace actor

class Profile;

namespace tabs {

struct ActorTaskIconState {
  enum class Text {
    // Default/no text.
    kDefault,
    // `Needs attention` text.
    kNeedsAttention,
    // `Complete Tasks` text.
    kCompleteTasks,
  };
  // Whether the task icon should be visible.
  bool is_visible = false;
  // The text that should be displayed, may change this to a string in the
  // future.
  Text text = Text::kDefault;

  bool operator==(const ActorTaskIconState& other) const {
    return is_visible == other.is_visible && text == other.text;
  }
};

struct ActorTaskNudgeState {
  enum class Text {
    // Default/no text.
    kDefault,
    // `Needs attention` text.
    kNeedsAttention,
    // `Complete Tasks` text.
    kCompleteTasks,
  };
  Text text = Text::kDefault;
};

class GlicActorTaskIconManager : public KeyedService {
 public:
  GlicActorTaskIconManager(Profile* profile,
                           actor::ActorKeyedService* actor_service,
                           glic::GlicWindowController& window_controller);
  ~GlicActorTaskIconManager() override;

  // Called whenever the instance visibility updates.
  void OnInstanceStateChange(bool is_showing,
                             glic::mojom::CurrentView current_view);

  // Called whenever actor task state updates.
  void OnActorTaskStateUpdate(actor::TaskId task_id);

  // TODO(crbug.com/431015299): Clean up after redesign is launched.
  // Determines the state the task icon should be in.
  void UpdateTaskIcon(bool is_showing, glic::mojom::CurrentView current_view);

  // Determines the state the task nudge should be in.
  void UpdateTaskNudge();

  // TODO(crbug.com/431015299): Clean up after redesign is launched.
  // Register for this callback to get task icon state change notifications.
  using TaskIconStateChangeCallback = base::RepeatingCallback<void(
      bool is_showing,
      glic::mojom::CurrentView current_view,
      const ActorTaskIconState& actor_task_icon_state)>;
  base::CallbackListSubscription RegisterTaskIconStateChange(
      TaskIconStateChangeCallback callback);

  // Register for this callback to get task nudge state change notifications.
  using TaskNudgeChangeCallback = base::RepeatingCallback<void(
      const ActorTaskNudgeState& actor_task_nudge_state)>;
  base::CallbackListSubscription RegisterTaskNudgeStateChange(
      TaskNudgeChangeCallback callback);

  ActorTaskIconState GetCurrentActorTaskIconState() const;
  ActorTaskNudgeState GetCurrentActorTaskNudgeState() const;

  raw_ptr<tabs::TabInterface> GetLastUpdatedTab();

  // KeyedService:
  void Shutdown() override;

 private:
  // Called once on startup.
  void RegisterSubscriptions();

  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  // TODO(crbug.com/431015299): Clean up after redesign is launched.
  using TaskIconStateChangeCallbackList = base::RepeatingCallbackList<void(
      bool is_showing,
      glic::mojom::CurrentView current_view,
      const ActorTaskIconState& actor_task_icon_state)>;
  TaskIconStateChangeCallbackList task_icon_state_change_callback_list_;

  using TaskNudgeChangeCallbackList = base::RepeatingCallbackList<void(
      const ActorTaskNudgeState& actor_task_nudge_text)>;
  TaskNudgeChangeCallbackList task_nudge_state_change_callback_list_;

  ActorTaskIconState current_actor_task_icon_state_;
  ActorTaskNudgeState current_actor_task_nudge_state_;

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_service_;
  raw_ref<glic::GlicWindowController> window_controller_;

  // TODO(mjenn): Update implementation for multi-tab actuation.
  actor::TaskId current_task_id_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
