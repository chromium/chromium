// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_BROWSER_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for split tabs tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class SplitViewBrowserTestMixin : public T {
 public:
  template <class... Args>
  explicit SplitViewBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~SplitViewBrowserTestMixin() override = default;

  SplitViewBrowserTestMixin(const SplitViewBrowserTestMixin&) = delete;
  SplitViewBrowserTestMixin& operator=(const SplitViewBrowserTestMixin&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                       {});
  }

  TabStripModel* tab_strip_model() { return T::browser()->tab_strip_model(); }

  MultiContentsView* multi_contents_view() {
    return BrowserView::GetBrowserViewForBrowser(T::browser())
        ->multi_contents_view();
  }

  MultiContentsDropTargetView* drop_target_view() {
    MultiContentsDropTargetView* view =
        BrowserElementsViews::From(T::browser())
            ->template GetViewAs<MultiContentsDropTargetView>(
                MultiContentsDropTargetView::kMultiContentsDropTargetElementId);

    CHECK(view);
    return view;
  }

  virtual const std::vector<base::test::FeatureRefAndParams>
  GetEnabledFeatures() {
    return {{features::kSideBySide, {}},
            {features::kSideBySideKeyboardShortcut, {}}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_BROWSER_TEST_MIXIN_H_
