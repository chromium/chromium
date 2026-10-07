// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
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

  void OnClicked();

  // PerformanceInterventionButtonControllerDelegate:
  void Show() override;
  void Hide() override;
  bool IsButtonShowing() const override;
  bool IsBubbleShowing() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  views::BubbleDialogModelHost* GetBubbleDialogModelHostForTesting() const {
    return bubble_dialog_model_host_;
  }
  PerformanceInterventionButtonController* controller_for_testing() const {
    return controller_.get();
  }

 private:
  void UpdateState();
  void CreateBubble();
  void OnButtonShown(ui::TrackedElement* element);

  raw_ptr<WebUIToolbarControlDelegate> delegate_;
  std::unique_ptr<PerformanceInterventionButtonController> controller_;
  bool should_be_shown_ = false;

  raw_ptr<views::BubbleDialogModelHost> bubble_dialog_model_host_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};

  // Subscription to observe when the performance intervention toolbar button
  // element is shown, allowing the bubble to be created and anchored once the
  // button becomes available in the UI.
  ui::ElementTracker::Subscription button_shown_subscription_;

};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PERFORMANCE_INTERVENTION_CONTROL_H_
