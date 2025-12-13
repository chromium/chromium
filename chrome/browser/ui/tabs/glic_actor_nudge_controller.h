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

  void OnStateUpdate(actor::ui::ActorTaskNudgeState actor_task_nudge_state);

  // TODO(crbug.com/446871477): Add to actor/resources/common_resources post
  // M143.
  static constexpr std::u16string GetCheckTasksNudgeLabel() {
    return u"Check your tasks";
  }

 private:
  void OnStateUpdateImpl(actor::ui::ActorTaskNudgeState actor_task_nudge_state);

  // Subscribe to updates from the GlicActorTaskIconManager.
  void RegisterActorNudgeStateCallback();

  // Get the current actor nudge state and update the UI. Called on
  // window creation to maintain state across multiple windows.
  void UpdateCurrentActorNudgeState();

  // Only update the nudge label if it's already showing, otherwise retrigger
  // the nudge. Always shows the task list bubble after.
  void UpdateNudgeLabelOrRetrigger(std::u16string nudge_label_text);

  const raw_ptr<Profile> profile_;
  raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripActionContainer> tab_strip_action_container_;

  std::vector<base::CallbackListSubscription>
      actor_nudge_state_change_callback_subscription_;

  ::ui::ScopedUnownedUserData<GlicActorNudgeController> scoped_data_holder_;

  base::WeakPtrFactory<GlicActorNudgeController> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_
