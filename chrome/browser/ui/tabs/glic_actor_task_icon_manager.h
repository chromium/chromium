// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
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

class GlicActorTaskIconManager : public KeyedService {
 public:
  GlicActorTaskIconManager(Profile* profile,
                           actor::ActorKeyedService* actor_service,
                           glic::GlicWindowController& window_controller,
                           glic::Host& host);
  ~GlicActorTaskIconManager() override;

  // Called whenever floaty updates.
  void OnFloatyUpdate(glic::GlicWindowController::State floaty_state,
                      glic::mojom::CurrentView current_view);

  // TODO(crbug.com/437161973): Add necessary parameters.
  // Called whenever actor task state updates.
  void OnActorTaskStateUpdate();

  // Determines the state the task icon should be in.
  void UpdateTaskIcon(glic::GlicWindowController::State floaty_state,
                      glic::mojom::CurrentView current_view);

  // Register for this callback to get task icon state change notifications.
  using TaskIconStateChangeCallback = base::RepeatingCallback<void(
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView current_view,
      const ActorTaskIconState& actor_task_icon_state)>;
  base::CallbackListSubscription RegisterTaskIconStateChange(
      TaskIconStateChangeCallback callback);

  ActorTaskIconState GetCurrentActorTaskIconState() const;

  // KeyedService:
  void Shutdown() override;

 private:
  // Called once on startup.
  void RegisterSubscriptions();

  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  using TaskIconStateChangeCallbackList = base::RepeatingCallbackList<void(
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView current_view,
      const ActorTaskIconState& actor_task_icon_state)>;
  TaskIconStateChangeCallbackList task_icon_state_change_callback_list_;

  ActorTaskIconState current_actor_task_icon_state_;
  // Determines whether to suppress the task icon text.
  bool suppress_task_icon_text_ = false;
  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_service_;
  raw_ref<glic::GlicWindowController> window_controller_;
  raw_ref<glic::Host> host_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
