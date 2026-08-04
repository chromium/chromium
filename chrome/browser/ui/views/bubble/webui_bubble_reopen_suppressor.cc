// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"

#include "ui/views/mouse_constants.h"
#include "ui/views/widget/widget.h"

WebUIBubbleReopenSuppressor::WebUIBubbleReopenSuppressor()
    : suppression_threshold_(views::kMinimumTimeBetweenButtonClicks) {}

WebUIBubbleReopenSuppressor::~WebUIBubbleReopenSuppressor() = default;

void WebUIBubbleReopenSuppressor::Observe(views::Widget* widget) {
  observation_.Reset();
  if (widget) {
    observation_.Observe(widget);
  }
}

bool WebUIBubbleReopenSuppressor::IsShowing() const {
  return observation_.IsObserving();
}

views::Widget* WebUIBubbleReopenSuppressor::GetWidget() {
  return observation_.GetSource();
}

void WebUIBubbleReopenSuppressor::Close(views::Widget::ClosedReason reason) {
  if (observation_.IsObserving()) {
    observation_.GetSource()->CloseWithReason(reason);
  }
}

void WebUIBubbleReopenSuppressor::OnMousePressed(
    bool extra_suppress_condition) {
  suppress_next_bubble_show_ = extra_suppress_condition || ShouldSuppress();
}

bool WebUIBubbleReopenSuppressor::ShouldSuppressBubbleShow(
    bool is_pointer_interaction) {
  if (is_pointer_interaction) {
    bool suppress = suppress_next_bubble_show_;
    suppress_next_bubble_show_ = false;
    return suppress;
  }

  suppress_next_bubble_show_ = false;
  return IsShowing();
}

bool WebUIBubbleReopenSuppressor::ShouldSuppress() const {
  if (IsShowing()) {
    return true;
  }

  if (!last_close_time_) {
    return false;
  }

  return (base::TimeTicks::Now() - *last_close_time_) < suppression_threshold_;
}

void WebUIBubbleReopenSuppressor::OnWidgetDestroying(views::Widget* widget) {
  last_close_time_ = base::TimeTicks::Now();
  observation_.Reset();
}

void WebUIBubbleReopenSuppressor::CloseForTesting() {
  last_close_time_ = base::TimeTicks::Now();
  observation_.Reset();
}

void WebUIBubbleReopenSuppressor::SetSuppressionThresholdForTesting(  // IN-TEST
    base::TimeDelta threshold) {
  suppression_threshold_ = threshold;
}
