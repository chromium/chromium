// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;
class BrowserWindowInterface;
class TabStripActionContainer;

namespace tabs {

// Controller that handles Glic Actor notification/nudge handling.
// TODO(crbug.com/431015299): Move GlicNudgeController logic into this
// controller in order to coordinate nudge behavior between Glic and Glic Actor.
class GlicActorNudgeController {
 public:
  explicit GlicActorNudgeController(
      BrowserWindowInterface* browser,
      TabStripActionContainer* tab_strip_action_container);
  GlicActorNudgeController(const GlicActorNudgeController&) = delete;
  GlicActorNudgeController& operator=(const GlicActorNudgeController& other) =
      delete;
  virtual ~GlicActorNudgeController();

  DECLARE_USER_DATA(GlicActorNudgeController);
  static GlicActorNudgeController* From(BrowserWindowInterface* browser);

  // Update the nudge for the given state. Will also conditionally show the
  // bubble on UI update based on show_bubble.
  void OnStateUpdate(bool show_bubble,
                     actor::ui::ActorTaskNudgeState actor_task_nudge_state);

  // Get the current actor nudge state and update the UI. Called when
  // TabStripActionContainer is added to the native widget.
  void UpdateCurrentActorNudgeState();

 private:
  // Subscribe to updates from the GlicActorTaskIconManager.
  void RegisterActorNudgeStateCallback();

  // Only update the nudge label if it's already showing, otherwise retrigger
  // the nudge. Shows the task list bubble after if show_bubble is true.
  void UpdateNudgeLabelOrRetrigger(std::u16string nudge_label_text,
                                   bool show_bubble);

  // Close the task list bubble if it is visible.
  void CloseBubble();

  // Called when the task list bubble's visibility state changes.
  void OnBubbleVisibilityChange(bool is_bubble_open);

  const raw_ptr<Profile> profile_;
  raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripActionContainer> tab_strip_action_container_;

  std::vector<base::CallbackListSubscription>
      actor_nudge_state_change_callback_subscription_;
  std::vector<base::CallbackListSubscription>
      bubble_visibility_change_subscription_;

  ::ui::ScopedUnownedUserData<GlicActorNudgeController> scoped_data_holder_;

  base::WeakPtrFactory<GlicActorNudgeController> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_
