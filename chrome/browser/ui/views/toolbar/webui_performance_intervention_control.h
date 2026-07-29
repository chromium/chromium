// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget_observer.h"

class PerformanceInterventionButtonController;
class WebUIToolbarControlDelegate;

namespace views {
class BubbleDialogModelHost;
class Widget;
}  // namespace views

// WebUIPerformanceInterventionControl implements C++-side functionality for the
// WebUI-based implementation of the performance intervention button in the
// toolbar.
class WebUIPerformanceInterventionControl
    : public PerformanceInterventionButtonControllerDelegate,
      public views::WidgetObserver {
 public:
  explicit WebUIPerformanceInterventionControl(
      WebUIToolbarControlDelegate* delegate);
  WebUIPerformanceInterventionControl(
      const WebUIPerformanceInterventionControl&) = delete;
  WebUIPerformanceInterventionControl& operator=(
      const WebUIPerformanceInterventionControl&) = delete;
  ~WebUIPerformanceInterventionControl() override;

  void Init();

  void OnClicked(bool is_mouse_interaction);
  void OnMousePressed();

  // PerformanceInterventionButtonControllerDelegate:
  void Show() override;
  void Hide() override;
  bool IsButtonShowing() const override;
  bool IsBubbleShowing() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void SetSuppressionThresholdForTesting(base::TimeDelta threshold);
  views::BubbleDialogModelHost* GetBubbleDialogModelHostForTesting() const {
    return bubble_dialog_model_host_;
  }

 private:
  void UpdateState();
  void CreateBubble();
  void OnButtonShown(ui::TrackedElement* element);

  raw_ptr<WebUIToolbarControlDelegate> delegate_;
  std::unique_ptr<PerformanceInterventionButtonController> controller_;
  bool should_be_shown_ = false;

  // Boolean that keeps track if the intervention button icon should be shown
  // in the active color. The intervention button should show the active color
  // when it becomes visible and stay in the active color until the user clicks
  // on the button.
  bool is_active_ = true;

  raw_ptr<views::BubbleDialogModelHost> bubble_dialog_model_host_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};

  // Subscription to observe when the performance intervention toolbar button
  // element is shown, allowing the bubble to be created and anchored once the
  // button becomes available in the UI.
  ui::ElementTracker::Subscription button_shown_subscription_;

  // Helper to prevent mouse clicks from immediately reopening a bubble that was
  // just closed.
  WebUIBubbleReopenSuppressor reopen_suppressor_;
  bool suppress_button_clicked_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_
