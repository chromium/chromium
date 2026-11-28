// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class PerformanceInterventionButtonController;

namespace views {
class BubbleDialogModelHost;
class Widget;
}  // namespace views

class PerformanceInterventionButton
    : public ToolbarButton,
      public PerformanceInterventionButtonControllerDelegate,
      public views::WidgetObserver {
  METADATA_HEADER(PerformanceInterventionButton, ToolbarButton)

 public:
  explicit PerformanceInterventionButton(BrowserWindowInterface* browser);
  ~PerformanceInterventionButton() override;

  PerformanceInterventionButton(const PerformanceInterventionButton&) = delete;
  PerformanceInterventionButton& operator=(
      const PerformanceInterventionButton&) = delete;

  // PerformanceInterventionButtonControllerDelegate:
  void Show() override;
  void Hide() override;
  bool IsButtonShowing() const override;
  bool IsBubbleShowing() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  PerformanceInterventionButtonController* controller() {
    return controller_.get();
  }

  views::BubbleDialogModelHost* bubble_dialog_model_host() {
    return bubble_dialog_model_host_;
  }

 private:
  void OnClicked();
  void CreateBubble();

  // Updates the icon to the active or inactive color. The intervention button
  // should show the active color when it becomes visible and stay in the
  // active color until the user clicks on the button.
  void UpdateIconColor(bool is_active);

  std::unique_ptr<PerformanceInterventionButtonController> controller_;
  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::BubbleDialogModelHost> bubble_dialog_model_host_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
