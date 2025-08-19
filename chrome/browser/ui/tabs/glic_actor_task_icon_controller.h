// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"

class BrowserWindowInterface;
class TabStripActionContainer;

namespace tabs {
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

  void OnStateUpdate(
      actor::ui::ActorUiStateManagerInterface::TaskIconUiState task_icon_state,
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView floaty_view);

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<TabStripActionContainer> tab_strip_action_container_;

  // Subscribe to updates from the ActorUiStateManager.
  void RegisterTaskIconStateCallback();

  // Get the current task icon and floaty state and update the UI. Called on
  // window creation to maintain state across multiple windows.
  void UpdateCurrentTaskIconUiState();

  std::vector<base::CallbackListSubscription>
      task_icon_state_change_callback_subscription_;

  ::ui::ScopedUnownedUserData<GlicActorTaskIconController> scoped_data_holder_;
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
