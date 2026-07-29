// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_performance_intervention_control.h"

#include <memory>
#include <utility>

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

WebUIPerformanceInterventionControl::WebUIPerformanceInterventionControl(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUIPerformanceInterventionControl::~WebUIPerformanceInterventionControl() {
  if (bubble_dialog_model_host_) {
    PerformanceInterventionBubble::CloseBubble(bubble_dialog_model_host_);
  }
}

void WebUIPerformanceInterventionControl::Init() {
  controller_ = std::make_unique<PerformanceInterventionButtonController>(
      this, delegate_->GetBrowser());
}

void WebUIPerformanceInterventionControl::Show() {
  should_be_shown_ = true;
  is_active_ = true;
  UpdateState();
  delegate_->OnPreferredSizeChanged();

  ui::TrackedElement* button_element =
      BrowserElements::From(delegate_->GetBrowser())
          ->GetElement(kToolbarPerformanceInterventionButtonElementId);
  if (button_element) {
    CreateBubble();
  } else {
    button_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            kToolbarPerformanceInterventionButtonElementId,
            views::ElementTrackerViews::GetContextForView(delegate_->GetView()),
            base::BindRepeating(
                &WebUIPerformanceInterventionControl::OnButtonShown,
                base::Unretained(this)));
  }
}

void WebUIPerformanceInterventionControl::Hide() {
  should_be_shown_ = false;
  is_active_ = false;
  suppress_button_clicked_ = false;
  UpdateState();
  delegate_->OnPreferredSizeChanged();
  button_shown_subscription_ = {};
  if (bubble_dialog_model_host_) {
    PerformanceInterventionBubble::CloseBubble(bubble_dialog_model_host_);
  }
}

bool WebUIPerformanceInterventionControl::IsButtonShowing() const {
  return should_be_shown_;
}

bool WebUIPerformanceInterventionControl::IsBubbleShowing() const {
  return bubble_dialog_model_host_ != nullptr;
}

void WebUIPerformanceInterventionControl::OnWidgetDestroying(
    views::Widget* widget) {
  PerformanceInterventionBubble::RecordCloseReason(widget->closed_reason());

  bubble_dialog_model_host_ = nullptr;
  scoped_widget_observation_.Reset();
  is_active_ = false;
  UpdateState();
}

void WebUIPerformanceInterventionControl::OnClicked(bool is_mouse_interaction) {
  if (!controller_) {
    return;
  }

  const bool was_active = is_active_ || IsBubbleShowing();
  is_active_ = false;
  UpdateState();

  const bool suppress =
      suppress_button_clicked_ || reopen_suppressor_.ShouldSuppress();
  suppress_button_clicked_ = false;

  if (IsBubbleShowing()) {
    PerformanceInterventionBubble::CloseBubble(bubble_dialog_model_host_);
    return;
  }

  if (was_active || suppress) {
    return;
  }

  is_active_ = true;
  UpdateState();
  CreateBubble();
  RecordInterventionToolbarButtonClicked();
}

void WebUIPerformanceInterventionControl::OnMousePressed() {
  suppress_button_clicked_ =
      reopen_suppressor_.ShouldSuppress() || is_active_ || IsBubbleShowing();
}

void WebUIPerformanceInterventionControl::UpdateState() {
  auto state =
      toolbar_ui_api::mojom::PerformanceInterventionControlState::New();
  state->should_be_shown = should_be_shown_;
  state->is_active = is_active_;
  delegate_->OnPerformanceInterventionControlStateChanged(std::move(state));
}

void WebUIPerformanceInterventionControl::CreateBubble() {
  if (bubble_dialog_model_host_) {
    return;
  }
  CHECK(delegate_->GetView());

  ui::TrackedElement* button_element =
      BrowserElements::From(delegate_->GetBrowser())
          ->GetElement(kToolbarPerformanceInterventionButtonElementId);
  if (!button_element) {
    return;
  }

  button_shown_subscription_ = {};

  bubble_dialog_model_host_ = PerformanceInterventionBubble::CreateBubble(
      views::BubbleAnchor(button_element), controller_.get());
  reopen_suppressor_.Observe(bubble_dialog_model_host_->GetWidget());
  scoped_widget_observation_.Observe(bubble_dialog_model_host_->GetWidget());
}

void WebUIPerformanceInterventionControl::OnButtonShown(
    ui::TrackedElement* element) {
  button_shown_subscription_ = {};
  CreateBubble();
}

void WebUIPerformanceInterventionControl::
    SetSuppressionThresholdForTesting(  // IN-TEST
        base::TimeDelta threshold) {
  reopen_suppressor_.SetSuppressionThresholdForTesting(threshold);  // IN-TEST
}
