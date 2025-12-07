// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"

class BrowserWindowInterface;
class TabStripActionContainer;

namespace tabs {
struct ActorTaskIconState;

class GlicActorTaskIconController {
 public:
  DECLARE_USER_DATA(GlicActorTaskIconController);
  explicit GlicActorTaskIconController(
      BrowserWindowInterface* browser,
      TabStripActionContainer* tab_strip_action_container);
  static GlicActorTaskIconController* From(BrowserWindowInterface* browser);
  GlicActorTaskIconController(const GlicActorTaskIconController&) = delete;
  GlicActorTaskIconController& operator=(
      const GlicActorTaskIconController& other) = delete;
  virtual ~GlicActorTaskIconController();

  void OnStateUpdate(bool is_showing,
                     glic::mojom::CurrentView current_view,
                     const ActorTaskIconState& actor_task_icon_state);

 private:
  // Subscribe to updates from the GlicActorTaskIconManager.
  void RegisterTaskIconStateCallback();

  // Get the current task icon and instance state and update the UI. Called on
  // window creation to maintain state across multiple windows.
  void UpdateCurrentTaskIconUiState();

  const raw_ptr<Profile> profile_;
  raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripActionContainer> tab_strip_action_container_;

  std::vector<base::CallbackListSubscription>
      task_icon_state_change_callback_subscription_;

  ui::ScopedUnownedUserData<GlicActorTaskIconController> scoped_data_holder_;
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
