// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for split tabs tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class SplitViewInteractiveTestMixin : public SplitViewBrowserTestMixin<T> {
 public:
  template <class... Args>
  explicit SplitViewInteractiveTestMixin(Args&&... args)
      : SplitViewBrowserTestMixin<T>(std::forward<Args>(args)...) {}

  auto EnterSplitView(int active_tab,
                      std::optional<int> other_tab = std::nullopt,
                      double ratio = 0.5) {
    // MultiContentsView overrides Layout, causing an edge case where the
    // resize area gets set to visible but doesn't gain nonzero size until the
    // next layout pass. Use PollView and WaitForState to wait for a nonzero
    // size, rather than just visible = true.
    using ResizeAreaLoadObserver =
        views::test::PollingViewObserver<bool, MultiContentsResizeArea>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ResizeAreaLoadObserver,
                                        kResizeLoadObserver);

    auto result = SplitViewBrowserTestMixin<T>::Steps(
        SplitViewBrowserTestMixin<T>::SelectTab(kTabStripElementId, active_tab),
        SplitViewBrowserTestMixin<T>::Do([&, other_tab, ratio]() {
          if (other_tab.has_value()) {
            split_tabs::SplitTabVisualData visual_data;
            visual_data.set_split_ratio(ratio);
            SplitViewBrowserTestMixin<T>::browser()
                ->tab_strip_model()
                ->AddToNewSplit(
                    {other_tab.value()}, visual_data,
                    split_tabs::SplitTabCreatedSource::kToolbarButton);
          } else {
            chrome::NewSplitTab(
                SplitViewBrowserTestMixin<T>::browser(),
                split_tabs::SplitTabCreatedSource::kToolbarButton);
          }
        }),
        SplitViewBrowserTestMixin<T>::PollView(
            kResizeLoadObserver,
            MultiContentsResizeArea::kMultiContentsResizeAreaElementId,
            [](const MultiContentsResizeArea* resize_area) -> bool {
              return resize_area->size().width() > 0 &&
                     resize_area->size().height() > 0;
            }),
        SplitViewBrowserTestMixin<T>::WaitForState(kResizeLoadObserver, true));
    SplitViewBrowserTestMixin<T>::AddDescriptionPrefix(result,
                                                       "EnterSplitView()");
    return result;
  }

  auto ExitSplitView(int index) {
    auto result = SplitViewBrowserTestMixin<T>::Steps(
        SplitViewBrowserTestMixin<T>::Check([index, this]() {
          return SplitViewBrowserTestMixin<T>::browser()
              ->tab_strip_model()
              ->GetSplitForTab(index)
              .has_value();
        }),
        SplitViewBrowserTestMixin<T>::Do([index, this]() {
          auto split_id = SplitViewBrowserTestMixin<T>::browser()
                              ->tab_strip_model()
                              ->GetSplitForTab(index);
          SplitViewBrowserTestMixin<T>::browser()
              ->tab_strip_model()
              ->RemoveSplit(split_id.value());
        }),
        SplitViewBrowserTestMixin<T>::WaitForHide(
            MultiContentsResizeArea::kMultiContentsResizeAreaElementId),
        SplitViewBrowserTestMixin<T>::Check([this]() {
          return SplitViewBrowserTestMixin<T>::multi_contents_view()
              ->GetInactiveContentsView()
              ->GetVisible();
        }));
    SplitViewBrowserTestMixin<T>::AddDescriptionPrefix(result,
                                                       "ExitSplitView()");
    return result;
  }

  auto FocusInactiveTabInSplit() {
    auto result = SplitViewBrowserTestMixin<T>::Steps(
        SplitViewBrowserTestMixin<T>::MoveMouseTo(
            base::BindLambdaForTesting([this]() {
              return SplitViewBrowserTestMixin<T>::multi_contents_view()
                  ->GetInactiveContentsView()
                  ->GetBoundsInScreen()
                  .CenterPoint();
            })),
        SplitViewBrowserTestMixin<T>::ClickMouse());
    SplitViewBrowserTestMixin<T>::AddDescriptionPrefix(
        result, "FocusInactiveTabInSplit()");
    return result;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_SPLIT_VIEW_INTERACTIVE_TEST_MIXIN_H_
