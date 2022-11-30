// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_
#define CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_

#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "ui/views/widget/widget_observer.h"

// Runs UI accessibility checks on |widget|.
void RunAccessibilityChecks(views::Widget* widget);

// Observe the creation of all widgets and ensure their view subtrees are
// checked for accessibility violations when they become visible or hidden.
//
// Accessibility violations will add a gtest failure.
class AccessibilityChecker : public ChromeViewsDelegate,
                             public views::WidgetObserver {
 public:
  AccessibilityChecker();

  AccessibilityChecker(const AccessibilityChecker&) = delete;
  AccessibilityChecker& operator=(const AccessibilityChecker&) = delete;

  ~AccessibilityChecker() override;

  // ChromeViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 private:
  base::ScopedMultiSourceObservation<views::Widget, WidgetObserver>
      scoped_observations_{this};
};

#endif  // CHROME_TEST_VIEWS_ACCESSIBILITY_CHECKER_H_
