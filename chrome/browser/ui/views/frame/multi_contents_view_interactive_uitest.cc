// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/split_tab_collection.h"
#include "chrome/browser/ui/tabs/split_tab_visual_data.h"
#include "chrome/browser/ui/tabs/test/split_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/views_test_utils.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

class MultiContentsViewUiTest
    : public SplitTabsInteractiveTestMixin<InteractiveBrowserTest> {
 protected:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  auto CreateTabsAndEnterSplitView() {
    auto result = Steps(
        AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
        CheckResult([=, this]() { return tab_strip_model()->count(); }, 2u),
        EnterSplitView(0, 1));
    AddDescriptionPrefix(result, "CreateTabsAndEnterSplitView()");
    return result;
  }

  auto CheckResizeKey(ui::KeyboardCode key_code,
                      base::RepeatingCallback<bool(double, double)> check) {
    // MultiContentsView overrides Layout, causing an edge case where resizes
    // don't take effect until the next layout pass. Use PollView and
    // WaitForState to wait for the expected layout pass to be completed.
    using MultiContentsViewLayoutObserver =
        views::test::PollingViewObserver<bool, MultiContentsView>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewLayoutObserver,
                                        kMultiContentsViewLayoutObserver);

    auto result = Steps(
        FocusElement(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId),
        SendKeyPress(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
            key_code),
        PollView(kMultiContentsViewLayoutObserver,
                 MultiContentsView::kMultiContentsViewElementId,
                 [check](const MultiContentsView* multi_contents_view) -> bool {
                   double start_width =
                       multi_contents_view->start_contents_view_for_testing()
                           ->size()
                           .width();
                   double end_width =
                       multi_contents_view->end_contents_view_for_testing()
                           ->size()
                           .width();
                   return check.Run(start_width, end_width);
                 }),
        WaitForState(kMultiContentsViewLayoutObserver, true));
    AddDescriptionPrefix(result, "CheckResizeKey()");
    return result;
  }

  auto CheckTabIsActive(int index) {
    return CheckResult([this]() { return tab_strip_model()->active_index(); },
                       index);
  }
};

// Check that MultiContentsView exists when the side by side flag is enabled
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ExistsWithFlag) {
  RunTestSequence(
      EnsurePresent(MultiContentsView::kMultiContentsViewElementId));
}

// Create a new split and exit the split view and ensure only 1 contents view is
// visible
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, EnterAndExitSplitViews) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), CheckTabIsActive(0), ExitSplitView(0),
      CheckTabIsActive(0),
      CheckResult([this]() { return tab_strip_model()->count(); }, 2u));
}

// Check that MultiContentsView changes its active view when inactive view is
// focused using mouse click.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesInactiveViewUsingMouseClick) {
  RunTestSequence(CreateTabsAndEnterSplitView(), CheckTabIsActive(0),
                  FocusInactiveTabInSplit(), CheckTabIsActive(1));
}

// Check that MultiContentsView changes its active view when inactive view is
// focused using keyboard.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesInactiveViewUsingKeyboard) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), CheckTabIsActive(0),
      // The second contents view should be next in the focus order after
      // the resize handle so send a TAB key event to move focus to inactive tab
      FocusElement(
          MultiContentsResizeHandle::kMultiContentsResizeHandleElementId),
      SendKeyPress(
          MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
          ui::VKEY_TAB),
      CheckTabIsActive(1));
}

// TODO(crbug.com/399212996): Flaky on linux_chromium_asan_rel_ng and
// chromium/ci/Linux Chromium OS ASan LSan Tests (1).
#if (defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_ResizesViaKeyboard DISABLED_ResizesViaKeyboard
#else
#define MAYBE_ResizesViaKeyboard ResizesViaKeyboard
#endif
// Check that the MultiContentsView resize area correctly resizes the start and
// end contents views via left and right key events.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, MAYBE_ResizesViaKeyboard) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), Check([&]() {
        double start_width = multi_contents_view()
                                 ->start_contents_view_for_testing()
                                 ->size()
                                 .width();
        double end_width = multi_contents_view()
                               ->end_contents_view_for_testing()
                               ->size()
                               .width();
        return start_width == end_width;
      }),
      CheckResizeKey(ui::VKEY_RIGHT, base::BindRepeating([](double start_width,
                                                            double end_width) {
                       return start_width > end_width;
                     })),
      CheckResizeKey(ui::VKEY_LEFT, base::BindRepeating([](double start_width,
                                                           double end_width) {
                       return start_width == end_width;
                     })),
      CheckResizeKey(ui::VKEY_LEFT, base::BindRepeating([](double start_width,
                                                           double end_width) {
                       return start_width > end_width;
                     })));
}

// Check that MultiContentsView only has insets on the contents views when in a
// split, verify this by checking that the sum of the contents views and resize
// area is less than the total width.
// TODO(crbug.com/397777917): Once this bug is resolved, if MultiContentsView is
// update to use interior margins then we should check whether those are set
// here instead of checking widths.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, InsetsOnlyInSplit) {
  RunTestSequence(
      Check([&]() {
        return multi_contents_view()
                   ->GetActiveContentsView()
                   ->bounds()
                   .width() == multi_contents_view()->bounds().width();
      }),
      CreateTabsAndEnterSplitView(), Check([&]() {
        int contents_and_resize_width =
            multi_contents_view()->GetActiveContentsView()->bounds().width() +
            multi_contents_view()->GetInactiveContentsView()->bounds().width() +
            multi_contents_view()->resize_area_for_testing()->bounds().width();
        return contents_and_resize_width <
               multi_contents_view()->bounds().width();
      }));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesMostRecentlyActiveTabInSplit) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), CheckTabIsActive(0),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      CheckTabIsActive(2),
      // Since tab 0 and 1 are part of a split view and tab 0 was the most
      // recently focused half of the split it should become the active tab, but
      // both tabs will be visible.
      SelectTab(kTabStripElementId, 1, InputType::kMouse, 0),
      CheckTabIsActive(0),
      // Select another tab in the split view and ensure the active index
      // doesn't change since it isn't the currently focused tab.
      SelectTab(kTabStripElementId, 1, InputType::kMouse, 0),
      CheckTabIsActive(0));
}
