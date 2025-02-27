// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
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

class MultiContentsViewUiTest : public InteractiveBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  MultiContentsView* multi_contents_view() {
    return browser_view()->multi_contents_view_for_testing();
  }

  auto EnterSplitView() {
    // MultiContentsView overrides Layout, causing an edge case where the
    // resize area gets set to visible but doesn't gain nonzero size until the
    // next layout pass. Use PollView and WaitForState to wait for a nonzero
    // size, rather than just visible = true.
    using ResizeAreaLoadObserver =
        views::test::PollingViewObserver<bool, MultiContentsResizeArea>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ResizeAreaLoadObserver,
                                        kResizeLoadObserver);

    auto result = Steps(
        AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
        Check([=, this]() { return tab_strip_model()->count() == 2u; }),
        Do([&]() {
          content::WebContents* inactive_contents =
              tab_strip_model()->GetWebContentsAt(1);
          multi_contents_view()->SetWebContents(inactive_contents, false);
        }),
        PollView(kResizeLoadObserver,
                 MultiContentsResizeArea::kMultiContentsResizeAreaElementId,
                 [](const MultiContentsResizeArea* resize_area) -> bool {
                   return resize_area->size().width() > 0 &&
                          resize_area->size().height() > 0;
                 }),
        WaitForState(kResizeLoadObserver, true));
    AddDescriptionPrefix(result, "EnterSplitView()");
    return result;
  }

  auto FocusResizeHandle() {
    using FocusObserver =
        views::test::PollingViewObserver<bool, MultiContentsResizeHandle>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FocusObserver, kFocusObserver);

    auto result = Steps(
        WithView(MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
                 [](MultiContentsResizeHandle* resize_handle) {
                   resize_handle->RequestFocus();
                 }),
        PollView(kFocusObserver,
                 MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
                 [](const MultiContentsResizeHandle* resize_handle) -> bool {
                   return resize_handle->HasFocus();
                 }),
        WaitForState(kFocusObserver, true));
    AddDescriptionPrefix(result, "FocusResizeHandle()");
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
        FocusResizeHandle(), Do([this, key_code]() {
          ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
              browser(), key_code, false, false, false, false));
        }),
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that MultiContentsView exists when the side by side flag is enabled
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ExistsWithFlag) {
  RunTestSequence(
      EnsurePresent(MultiContentsView::kMultiContentsViewElementId));
}

// Check that MultiContentsView executes its callback on inactive view mouse
// down.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ActivatesInactiveView) {
  RunTestSequence(
      EnterSplitView(),
      Check([=, this]() { return tab_strip_model()->active_index() == 0; }),
      Do([&]() {
        // Simulate a mouse click event on the inactive contents, which should
        // trigger the activation callback.
        content::SimulateMouseClick(
            multi_contents_view()->GetInactiveContentsView()->GetWebContents(),
            0, blink::WebPointerProperties::Button::kLeft);
      }),
      Check([&]() { return tab_strip_model()->active_index() == 1; }));
}

// TODO(crbug.com/399212996): Flaky on linux_chromium_asan_rel_ng.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ResizesViaKeyboard DISABLED_ResizesViaKeyboard
#else
#define MAYBE_ResizesViaKeyboard ResizesViaKeyboard
#endif
// Check that the MultiContentsView resize area correctly resizes the start and
// end contents views via left and right key events.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, MAYBE_ResizesViaKeyboard) {
  RunTestSequence(
      EnterSplitView(), Check([&]() {
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
