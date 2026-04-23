// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_member.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;

class ContextualTasksButton
    : public ToolbarButton,
      public contextual_tasks::ContextualTasksPanelController::Observer,
      public ImmersiveModeController::Observer {
  METADATA_HEADER(ContextualTasksButton, ToolbarButton)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContextualTasksToolbarButton);
  explicit ContextualTasksButton(
      BrowserWindowInterface* browser_window_interface);
  ~ContextualTasksButton() override;

  // contextual_tasks::ContextualTasksPanelController::Observer:
  void OnSurfaceStateChanged(
      contextual_tasks::ContextualTasksPanelHost::SurfaceState state,
      contextual_tasks::ContextualTasksPanelHost::StateChangeReason reason)
      override;
  void OnControllerDestroyed() override;

  // ImmersiveModeController::Observer:
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void OnImmersiveModeControllerDestroyed() override;

 protected:
  void UpdateColorsAndInsets() override;

 private:
  void OnButtonPress();
  void OnPinStateChanged();
  void OnSidePanelAlignmentChanged();
  void OnShouldUpdateVisibility(bool should_show);
  void OnEligibilityChange(bool is_eligible);
  bool ShouldApplyCircularBackgroundShadow() const;
  void MaybeUpdateVisibility();

  BooleanPrefMember pin_state_;
  BooleanPrefMember side_panel_alignment_;
  base::CallbackListSubscription should_update_visibility_subscription_;
  base::CallbackListSubscription eligibility_change_subscription_;
  base::CallbackListSubscription vertical_tabs_subscription_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  base::ScopedObservation<
      contextual_tasks::ContextualTasksPanelController,
      contextual_tasks::ContextualTasksPanelController::Observer>
      panel_controller_observation_{this};

  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
