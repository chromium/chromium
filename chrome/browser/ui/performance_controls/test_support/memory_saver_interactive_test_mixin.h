// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for memory saver tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class MemorySaverInteractiveTestMixin : public MemorySaverBrowserTestMixin<T> {
 public:
  template <class... Args>
  explicit MemorySaverInteractiveTestMixin(Args&&... args)
      : MemorySaverBrowserTestMixin<T>(std::forward<Args>(args)...) {}

  ~MemorySaverInteractiveTestMixin() override = default;

  MemorySaverInteractiveTestMixin(const MemorySaverInteractiveTestMixin&) =
      delete;
  MemorySaverInteractiveTestMixin& operator=(
      const MemorySaverInteractiveTestMixin&) = delete;

  auto CheckTabIsDiscarded(int tab_index, bool is_discarded) {
    return T::Check([=, this]() {
      return MemorySaverBrowserTestMixin<T>::IsTabDiscarded(tab_index) ==
             is_discarded;
    });
  }

  auto TryDiscardTab(int tab_index) {
    return T::Do([=, this]() {
      MemorySaverBrowserTestMixin<T>::TryDiscardTabAt(tab_index);
    });
  }

  auto ForceRefreshMemoryMetrics() {
    return T::Do([=, this]() {
      MemorySaverBrowserTestMixin<T>::ForceRefreshMemoryMetricsAndWait();
    });
  }

  // Attempts to discard the tab at `tab_index` and navigates to that
  // tab and waits for it to reload
  auto DiscardAndReloadTab(int tab_index,
                           const ui::ElementIdentifier& contents_id) {
    return T::Steps(  // This has to be done on a fresh message loop to prevent
                      // a tab being discarded while it is notifying its
                      // observers
        TryDiscardTab(tab_index), T::SelectTab(kTabStripElementId, tab_index),
        T::WaitForShow(contents_id));
  }
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_INTERACTIVE_TEST_MIXIN_H_
