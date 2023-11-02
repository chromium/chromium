// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/accessibility_checker.h"

#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget.h"

void RunAccessibilityChecks(views::Widget* widget) {
  RunAccessibilityPaintChecks(widget);
}

AccessibilityChecker::AccessibilityChecker() = default;

AccessibilityChecker::~AccessibilityChecker() {
  DCHECK(!scoped_observations_.IsObservingAnySource());
}

void AccessibilityChecker::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  ChromeViewsDelegate::OnBeforeWidgetInit(params, delegate);
  views::Widget* widget = delegate->AsWidget();
  if (widget)
    scoped_observations_.AddObservation(widget);
}

void AccessibilityChecker::OnWidgetDestroying(views::Widget* widget) {
  scoped_observations_.RemoveObservation(widget);
}

void AccessibilityChecker::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  // Test widget for accessibility errors both as it becomes visible or hidden,
  // in order to catch more errors. For example, to catch errors in the download
  // shelf we must check the browser window as it is hidden, because the shelf
  // is not visible when the browser window first appears.
  RunAccessibilityPaintChecks(widget);
}
