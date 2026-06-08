// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_

#include <memory>

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

namespace ui {
class ImageModel;
class LayerOwner;
}

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

  float GetCornerRadiusFor(ToolbarButton::Edge edge) const override;
  bool ShouldApplyCircularBackgroundShadow() const;
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

  // views::ViewObserver:
  void OnViewLayerBoundsSet(views::View* observed_view) override;

 protected:
  void UpdateColorsAndInsets() override;

 private:
  void OnButtonPress();
  void OnSidePanelAlignmentChanged();
  void OnShouldUpdateVisibility(bool should_show);
  void OnEligibilityChange(bool is_eligible);
  void MaybeUpdateVisibility();
  void UpdateDropShadowLayerBounds();
  ui::ImageModel GetButtonImage();

  BooleanPrefMember side_panel_alignment_;
  base::CallbackListSubscription should_update_visibility_subscription_;
  base::CallbackListSubscription eligibility_change_subscription_;
  base::CallbackListSubscription vertical_tabs_subscription_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  // Since the contextual tasks button is not a standard shape, it requires a
  // LayerOwner to paint the custom drop shadow.
  std::unique_ptr<ui::LayerOwner> drop_shadow_painted_layer_;

  base::ScopedObservation<
      contextual_tasks::ContextualTasksPanelController,
      contextual_tasks::ContextualTasksPanelController::Observer>
      panel_controller_observation_{this};

  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
