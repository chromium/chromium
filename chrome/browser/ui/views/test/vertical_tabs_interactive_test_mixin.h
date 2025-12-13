// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for vertical tabs tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class VerticalTabsInteractiveTestMixin
    : public VerticalTabsBrowserTestMixin<T> {
 public:
  template <class... Args>
  explicit VerticalTabsInteractiveTestMixin(Args&&... args)
      : VerticalTabsBrowserTestMixin<T>(std::forward<Args>(args)...) {}

  auto EnterVerticalTabsMode() {
    auto result = VerticalTabsBrowserTestMixin<T>::Steps(
        VerticalTabsBrowserTestMixin<T>::Do([this]() {
          VerticalTabsBrowserTestMixin<T>::EnterVerticalTabsMode();
        }),
        VerticalTabsBrowserTestMixin<T>::WaitForShow(
            kVerticalTabStripRegionElementId));
    VerticalTabsBrowserTestMixin<T>::AddDescriptionPrefix(
        result, "EnterVerticalTabsMode()");
    return result;
  }

  auto ExitVerticalTabsMode() {
    auto result = VerticalTabsBrowserTestMixin<T>::Steps(
        VerticalTabsBrowserTestMixin<T>::Do([this]() {
          VerticalTabsBrowserTestMixin<T>::ExitVerticalTabsMode();
        }),
        VerticalTabsBrowserTestMixin<T>::WaitForHide(
            kVerticalTabStripRegionElementId));
    VerticalTabsBrowserTestMixin<T>::AddDescriptionPrefix(
        result, "ExitVerticalTabsMode()");
    return result;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_INTERACTIVE_TEST_MIXIN_H_
