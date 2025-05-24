// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_SPLIT_TABS_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_TABS_TEST_SPLIT_TABS_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for split tabs tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class SplitTabsInteractiveTestMixin : public T {
 public:
  template <class... Args>
  explicit SplitTabsInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~SplitTabsInteractiveTestMixin() override = default;

  SplitTabsInteractiveTestMixin(const SplitTabsInteractiveTestMixin&) = delete;
  SplitTabsInteractiveTestMixin& operator=(
      const SplitTabsInteractiveTestMixin&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

  MultiContentsView* multi_contents_view() {
    return BrowserView::GetBrowserViewForBrowser(T::browser())
        ->multi_contents_view_for_testing();
  }

  auto EnterSplitView(int active_tab, int other_tab) {
    // MultiContentsView overrides Layout, causing an edge case where the
    // resize area gets set to visible but doesn't gain nonzero size until the
    // next layout pass. Use PollView and WaitForState to wait for a nonzero
    // size, rather than just visible = true.
    using ResizeAreaLoadObserver =
        views::test::PollingViewObserver<bool, MultiContentsResizeArea>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ResizeAreaLoadObserver,
                                        kResizeLoadObserver);

    auto result = T::Steps(
        T::SelectTab(kTabStripElementId, active_tab), T::Do([&, other_tab]() {
          T::browser()->tab_strip_model()->AddToNewSplit(
              {other_tab}, split_tabs::SplitTabVisualData());
        }),
        T::PollView(kResizeLoadObserver,
                    MultiContentsResizeArea::kMultiContentsResizeAreaElementId,
                    [](const MultiContentsResizeArea* resize_area) -> bool {
                      return resize_area->size().width() > 0 &&
                             resize_area->size().height() > 0;
                    }),
        T::WaitForState(kResizeLoadObserver, true));
    T::AddDescriptionPrefix(result, "EnterSplitView()");
    return result;
  }

  auto ExitSplitView(int index) {
    auto result = T::Steps(
        T::Check([index, this]() {
          return T::browser()
              ->tab_strip_model()
              ->GetSplitForTab(index)
              .has_value();
        }),
        T::Do([index, this]() {
          auto split_id =
              T::browser()->tab_strip_model()->GetSplitForTab(index);
          T::browser()->tab_strip_model()->RemoveSplit(split_id.value());
        }),
        T::WaitForHide(
            MultiContentsResizeArea::kMultiContentsResizeAreaElementId),
        T::Check([this]() {
          return multi_contents_view()->GetInactiveContentsView()->GetVisible();
        }));
    T::AddDescriptionPrefix(result, "ExitSplitView()");
    return result;
  }

  auto FocusInactiveTabInSplit() {
    auto result = T::Steps(T::MoveMouseTo(base::BindLambdaForTesting([this]() {
                             return multi_contents_view()
                                 ->GetInactiveContentsView()
                                 ->GetBoundsInScreen()
                                 .CenterPoint();
                           })),
                           T::ClickMouse());
    T::AddDescriptionPrefix(result, "FocusInactiveTabInSplit()");
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_UI_TABS_TEST_SPLIT_TABS_INTERACTIVE_TEST_MIXIN_H_
