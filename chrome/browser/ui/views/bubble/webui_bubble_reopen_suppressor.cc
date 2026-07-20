// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"

#include "ui/views/mouse_constants.h"
#include "ui/views/widget/widget.h"

WebUIBubbleReopenSuppressor::WebUIBubbleReopenSuppressor() = default;

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

bool WebUIBubbleReopenSuppressor::ShouldSuppress() const {
  if (IsShowing()) {
    return true;
  }

  if (!last_close_time_) {
    return false;
  }

  base::TimeDelta threshold =
      suppression_threshold_.value_or(views::kMinimumTimeBetweenButtonClicks);
  return (base::TimeTicks::Now() - *last_close_time_) < threshold;
}

void WebUIBubbleReopenSuppressor::SetSuppressionThresholdForTesting(  // IN-TEST
    base::TimeDelta threshold) {
  suppression_threshold_ = threshold;
}

void WebUIBubbleReopenSuppressor::OnWidgetDestroying(views::Widget* widget) {
  last_close_time_ = base::TimeTicks::Now();
  observation_.Reset();
}

void WebUIBubbleReopenSuppressor::CloseForTesting() {
  last_close_time_ = base::TimeTicks::Now();
  observation_.Reset();
}
