// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_

#include <concepts>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                       {});
  }

  void SetUpOnMainThread() override {
    EnterVerticalTabsMode();
    T::SetUpOnMainThread();
  }

  TabStripModel* tab_strip_model() { return T::browser()->tab_strip_model(); }

  tabs::VerticalTabStripStateController* vertical_tab_strip_state_controller() {
    return tabs::VerticalTabStripStateController::From(T::browser());
  }

  VerticalTabStripController* vertical_tab_strip_controller() {
    VerticalTabStripRegionView* const region_view =
        T::browser()
            ->GetBrowserView()
            .vertical_tab_strip_region_view_for_testing();
    return region_view ? region_view->GetVerticalTabStripController() : nullptr;
  }

  void EnterVerticalTabsMode() {
    T::browser()->profile()->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled,
                                                    true);
    T::RunScheduledLayouts();
  }

  void ExitVerticalTabsMode() {
    T::browser()->profile()->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled,
                                                    false);
    T::RunScheduledLayouts();
  }

  tabs_api::TabStripService* tab_strip_service() {
    return T::browser()
        ->GetFeatures()
        .tab_strip_service_feature()
        ->GetTabStripService();
  }

  virtual const std::vector<base::test::FeatureRefAndParams>
  GetEnabledFeatures() {
    return {{tabs::kVerticalTabs, {}}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_VERTICAL_TABS_BROWSER_TEST_MIXIN_H_
