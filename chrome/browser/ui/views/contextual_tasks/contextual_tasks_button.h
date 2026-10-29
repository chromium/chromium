// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;
class PinnedToolbarButtonStatusIndicator;

namespace ui {
class ImageModel;
class Layer;
class LayerOwner;
}  // namespace ui

class ContextualTasksButton
    : public ToolbarButton,
      public contextual_tasks::ContextualTasksPanelController::Observer,
      public ImmersiveModeController::Observer {
  METADATA_HEADER(ContextualTasksButton, ToolbarButton)

 public:
  explicit ContextualTasksButton(
      BrowserWindowInterface* browser_window_interface);
  ~ContextualTasksButton() override;

  static constexpr int kShadowOutset = 12;

  contextual_tasks::EphemeralButtonShape GetShape() const;
  float GetCornerRadiusFor(ToolbarButton::Edge edge) const override;
  bool IsHighlighted() const;
  bool ShouldShowCircleHighlight() const;
  ui::Layer* GetDropShadowLayerForTesting() const;
  PinnedToolbarButtonStatusIndicator* GetStatusIndicatorForTesting() const {
    return status_indicator_;
  }

  // ToolbarButton:
  void Layout(PassKey) override;
  void UpdateIcon() override;

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

  // views::View:
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;

 protected:
  bool ShouldShowMenu() override;
  void UpdateColorsAndInsets() override;

 private:
  void OnButtonPress();
  void OnShouldUpdatePosition();
  void OnShouldUpdateVisibility(bool should_show);
  void OnEligibilityChange(bool is_eligible);
  void UpdateContextMenu();
  void MaybeUpdateVisibility();
  void UpdateHighlight();
  void UpdateTooltipText();
  void RecordShownDuration(std::string_view end_reason);
  void MaybeToggleFeaturePromo();
  void MaybeShowFeaturePromo();
  void UpdateDropShadow(bool force_paint = false, float initial_opacity = 1.0f);
  void UpdateDropShadowLayerBounds();
  void UpdateStatusIndicator();
  void AnimateShow();
  void ClearDropShadow();
  bool IsPanelOpen() const;
  ui::ImageModel GetButtonImage();

  base::CallbackListSubscription should_update_visibility_subscription_;
  base::CallbackListSubscription should_update_position_subscription_;
  base::CallbackListSubscription eligibility_change_subscription_;
  base::CallbackListSubscription vertical_tabs_subscription_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  raw_ptr<PinnedToolbarButtonStatusIndicator> status_indicator_ = nullptr;

  std::optional<Button::ScopedAnchorHighlight> anchor_highlight_;

  // Since the contextual tasks button is not a standard shape, it requires a
  // LayerOwner to paint the custom drop shadow.
  std::unique_ptr<ui::LayerOwner> drop_shadow_painted_layer_;

  // Timestamp when the ephemeral button most recently became visible.
  base::TimeTicks shown_time_ticks_;

  // Tracks whether the contextual tasks side panel was open during the last
  // visibility update, so the ephemeral IPH can be triggered when the panel
  // closes while the button remains visible.
  bool was_panel_open_ = false;

  base::ScopedObservation<
      contextual_tasks::ContextualTasksPanelController,
      contextual_tasks::ContextualTasksPanelController::Observer>
      panel_controller_observation_{this};

  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observation_{this};

  base::WeakPtrFactory<ContextualTasksButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
