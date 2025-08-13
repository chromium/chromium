// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_window_controller.h"
#endif

class BrowserWindowInterface;
class TabStripActionContainer;

namespace tabs {
class GlicActorTaskIconController {
 public:
  explicit GlicActorTaskIconController(
      Profile* profile,
      TabStripActionContainer* tab_strip_action_container);
  GlicActorTaskIconController(const GlicActorTaskIconController&) = delete;
  GlicActorTaskIconController& operator=(
      const GlicActorTaskIconController& other) = delete;
  virtual ~GlicActorTaskIconController();

#if BUILDFLAG(ENABLE_GLIC)
  void OnStateUpdate(actor::ui::ActorUiStateManagerInterface::UiState,
                     glic::GlicWindowController::State floaty_state,
                     glic::mojom::CurrentView floaty_view);
#endif

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<TabStripActionContainer> tab_strip_action_container_;

  // Subscribe to updates from the ActorUiStateManager.
  void RegisterFloatyTaskStateCallback();

  // Get the current task and floaty state and update the UI. Called on window
  // creation to maintain state across multiple windows.
  void UpdateCurrentUiState();

  std::vector<base::CallbackListSubscription>
      floaty_task_state_change_callback_subscription_;
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
