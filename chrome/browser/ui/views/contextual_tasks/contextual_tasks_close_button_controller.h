// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CLOSE_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CLOSE_BUTTON_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace contextual_tasks {
class EntryPointEligibilityManager;
}  // namespace contextual_tasks

class BrowserWindowInterface;
class SidePanelEntry;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

// Controller to trigger whether `ContextualTasksCloseTabButton` should show.
class ContextualTasksCloseButtonController
    : public SidePanelEntryObserver,
      public contextual_tasks::ContextualTasksPanelController::Observer,
      public ImmersiveModeController::Observer {
 public:
  DECLARE_USER_DATA(ContextualTasksCloseButtonController);
  ContextualTasksCloseButtonController(
      BrowserWindowInterface* browser_window_interface,
      contextual_tasks::EntryPointEligibilityManager* eligibility_manager,
      contextual_tasks::ContextualTasksPanelController* panel_controller);
  ~ContextualTasksCloseButtonController() override;

  static ContextualTasksCloseButtonController* From(
      BrowserWindowInterface* browser_window_interface);

  using ShouldUpdateVisibilityCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterShouldUpdateButtonVisibility(
      ShouldUpdateVisibilityCallbackList::CallbackType callback);

  bool ShouldShowCloseButton();

  void MaybeNotifyVisibilityShouldChange();

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // contextual_tasks::ContextualTasksPanelController::Observer:
  void ExpandToFullTabStateChanged() override;
  void OnVerticalTabStripModeChanged(
      tabs::VerticalTabStripStateController* controller);

  // ImmersiveModeController::Observer:
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;

  void OnEligibilityChange(bool is_eligible);

  void MaybeCloseTabExpandSidePanel();

 private:
  bool IsVerticalTabOrIsImmersiveMode() const;

  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  ui::ScopedUnownedUserData<ContextualTasksCloseButtonController>
      scoped_unowned_user_data_;

  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      side_panel_entry_observation_{this};
  base::CallbackListSubscription eligibility_change_subscription_;
  base::CallbackListSubscription vertical_tab_subscription_;
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observation_{this};
  base::ScopedObservation<
      contextual_tasks::ContextualTasksPanelController,
      contextual_tasks::ContextualTasksPanelController::Observer>
      panel_controller_observation_{this};

  ShouldUpdateVisibilityCallbackList should_update_visibility_callbacks_;

  base::WeakPtrFactory<ContextualTasksCloseButtonController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CLOSE_BUTTON_CONTROLLER_H_
