// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_

#include <concepts>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"

// Template to be used as a mixin class for vertical tabs tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class VerticalTabsBrowserTestMixin : public T {
 public:
  template <class... Args>
  explicit VerticalTabsBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~VerticalTabsBrowserTestMixin() override = default;

  VerticalTabsBrowserTestMixin(const VerticalTabsBrowserTestMixin&) = delete;
  VerticalTabsBrowserTestMixin& operator=(const VerticalTabsBrowserTestMixin&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    EnterVerticalTabsMode();
  }

  TabStripModel* tab_strip_model() { return T::browser()->tab_strip_model(); }

  tabs::VerticalTabStripStateController* vertical_tab_strip_state_controller() {
    return T::browser()
        ->browser_window_features()
        ->vertical_tab_strip_state_controller();
  }

  void EnterVerticalTabsMode() {
    T::browser()->profile()->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled,
                                                    true);
  }

  tabs_api::TabStripService* tab_strip_service() {
    return T::browser()
        ->GetFeatures()
        .tab_strip_service_feature()
        ->GetTabStripService();
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_
