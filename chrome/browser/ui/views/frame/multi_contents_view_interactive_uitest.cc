// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/ranges.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/split_tab_highlight_controller.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "chrome/browser/ui/views/frame/multi_contents_background_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/views_test_utils.h"

namespace {
class ViewBoundsChangedObserver : public views::ViewObserver,
                                  public ui::test::StateObserver<int> {
 public:
  explicit ViewBoundsChangedObserver(views::View* view) {
    CHECK(view);
    observation_.Observe(view);
  }

  // ui::test::StateObserver:
  int GetStateObserverInitialState() const override {
    return bounds_changed_count_;
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    bounds_changed_count_++;
    OnStateObserverStateChanged(bounds_changed_count_);
  }

  void OnViewIsDeleting(views::View* view) override { observation_.Reset(); }

 private:
  raw_ptr<Browser> browser_;
  int bounds_changed_count_ = 0;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
}  // namespace

class MultiContentsViewUiTest
    : public SplitViewInteractiveTestMixin<
          TabStripInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  GURL GetTestUrl() { return embedded_test_server()->GetURL("/title1.html"); }

  auto CreateTabsAndEnterSplitView() {
    auto result = Steps(
        AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
        CheckResult([=, this]() { return tab_strip_model()->count(); }, 2u),
        EnterSplitView(0, 1));
    AddDescriptionPrefix(result, "CreateTabsAndEnterSplitView()");
    return result;
  }

  // MultiContentsView overrides Layout, causing an edge case where resizes
  // don't take effect until the next layout pass. Use PollView and
  // WaitForState to wait for the expected layout pass to be completed.
  using MultiContentsViewLayoutObserver =
      views::test::PollingViewObserver<bool, MultiContentsView>;

  auto CheckResizeValues(
      base::RepeatingCallback<bool(double, double)> check,
      ui::test::StateIdentifier<MultiContentsViewLayoutObserver> observer_id) {
    auto result = Steps(
        PollView(observer_id, kMultiContentsViewElementId,
                 [check](const MultiContentsView* multi_contents_view) -> bool {
                   double start_width =
                       multi_contents_view->start_contents_view_for_testing()
                           ->parent()
                           ->size()
                           .width();
                   double end_width =
                       multi_contents_view->end_contents_view_for_testing()
                           ->parent()
                           ->size()
                           .width();
                   return check.Run(start_width, end_width);
                 }),
        WaitForState(observer_id, true));
    AddDescriptionPrefix(result, "CheckResizeValues()");
    return result;
  }

  // Perform a check on the contents view sizes following a direct resize call
  auto CheckResize(int resize_amount,
                   base::RepeatingCallback<bool(double, double)> check) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewLayoutObserver,
                                        kMultiContentsViewLayoutObserver);
    auto result =
        Steps(Do([resize_amount, this]() {
                multi_contents_view()->OnResize(resize_amount, true);
              }),
              CheckResizeValues(check, kMultiContentsViewLayoutObserver));
    AddDescriptionPrefix(result, "CheckResize()");
    return result;
  }

  auto CheckResizeWithId(
      int resize_amount,
      base::RepeatingCallback<bool(double, double)> check,
      ui::test::StateIdentifier<MultiContentsViewLayoutObserver> observer_id) {
    auto result = Steps(Do([resize_amount, this]() {
                          multi_contents_view()->OnResize(resize_amount, true);
                        }),
                        CheckResizeValues(check, observer_id));
    AddDescriptionPrefix(result, "CheckResizeWithId()");
    return result;
  }

  // Perform a check on the contents view sizes following a keyboard-triggered
  // resize
  auto CheckResizeKey(ui::KeyboardCode key_code,
                      base::RepeatingCallback<bool(double, double)> check) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewLayoutObserver,
                                        kMultiContentsViewLayoutObserver);
    auto result = Steps(
        FocusElement(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId),
        SendKeyPress(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
            key_code),
        CheckResizeValues(check, kMultiContentsViewLayoutObserver));
    AddDescriptionPrefix(result, "CheckResizeKey()");
    return result;
  }

  auto ResizeWindow(int width) {
    auto result = Steps(Do([width, this]() {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      auto bounds = browser_view->bounds();
      bounds.set_width(width);
      bounds.set_height(1000);
      browser_view->SetBounds(bounds);
    }));
    AddDescriptionPrefix(result, "ResizeWindow()");
    return result;
  }

  auto ResizeContents(int width) {
    auto result = Steps(Do([width, this]() {
      BrowserView::GetBrowserViewForBrowser(browser())->SetContentsSize(
          gfx::Size(width, 1000));
    }));
    AddDescriptionPrefix(result, "ResizeContents()");
    return result;
  }

  auto SetMinWidth(int width) {
    auto result = Steps(Do([width, this]() {
      multi_contents_view()->set_min_contents_width_for_testing(width);
    }));
    AddDescriptionPrefix(result, "SetMinWidth()");
    return result;
  }

  auto CheckActiveContentsHasFocus() {
    return CheckView(
        kMultiContentsViewElementId,
        [](MultiContentsView* multi_contents_view) -> bool {
          return multi_contents_view->GetActiveContentsView()->HasFocus();
        });
  }
};

// Check that MultiContentsView exists when the side by side flag is enabled
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ExistsWithFlag) {
  RunTestSequence(EnsurePresent(kMultiContentsViewElementId));
}

// Check that resizing the browser window in split view correctly resizes
// both content panes.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ResizesInSplitView) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewLayoutObserver,
                                      kLayoutObserver);

  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
      CheckResult([=, this]() { return tab_strip_model()->count(); }, 2u),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1, /*ratio=*/0.75),
      ResizeContents(500),

      // Set the contents size to 600.
      ResizeContents(600),
      // Check that the active contents width is 600.
      CheckResizeValues(
          base::BindRepeating([](double start_width, double end_width) {
            return base::IsApproximatelyEqual(start_width, 600.0, 2.0);
          }),
          kLayoutObserver),
      // The inactive tab loses some sizing, so activate it before validating
      // width. It should be size to 200, in line with the 0.75 split ratio.
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      CheckResizeValues(
          base::BindRepeating([](double start_width, double end_width) {
            return base::IsApproximatelyEqual(end_width, 200.0, 2.0);
          }),
          kLayoutObserver));
}

// Create a new split and exit the split view and ensure only 1 contents view is
// visible
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, EnterAndExitSplitViews) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      ExitSplitView(0), WaitForActiveTabChange(0),
      CheckResult([this]() { return tab_strip_model()->count(); }, 2u));
}

// Tests switching tabs with split views. This also adds coverage to ensuring
// that there isn't any unnecessary re-layout during tab switching.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, TabSwitchWithSplitView) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ViewBoundsChangedObserver,
                                      kActiveContentsViewBoundsChangedObserver);
  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2),
      SelectTab(kTabStripElementId, 0, InputType::kMouse),
      // Check if there is just one resizing event that happens when switching
      // between a split view to a regular tab.
      WaitForActiveTabChange(0),
      ObserveState(kActiveContentsViewBoundsChangedObserver,
                   multi_contents_view()->GetActiveContentsView()),
      SelectTab(kTabStripElementId, 2, InputType::kMouse),
      WaitForActiveTabChange(2),
      CheckState(kActiveContentsViewBoundsChangedObserver, 1),
      StopObservingState(kActiveContentsViewBoundsChangedObserver));
}

// Check that MultiContentsView changes its active view when inactive view is
// focused using mouse click.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesInactiveViewUsingMouseClick) {
  RunTestSequence(CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
                  FocusInactiveTabInSplit(), WaitForActiveTabChange(1),
                  CheckActiveContentsHasFocus());
}

// Check that MultiContentsView changes its active view when inactive view is
// focused using keyboard.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesInactiveViewUsingKeyboard) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      // The second contents view should be next in the focus order after
      // the resize handle so send a TAB key event to move focus to inactive tab
      FocusElement(
          MultiContentsResizeHandle::kMultiContentsResizeHandleElementId),
      SendKeyPress(
          MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
          ui::VKEY_TAB),
      WaitForActiveTabChange(1), CheckActiveContentsHasFocus());
}

// Check that MultiContentsView changes its active view when the tab shortcut
// is used and the active view has focus.
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ActivatesInactiveViewUsingAccelerator) {
  const int kControlCommandModifier =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      FocusWebContents(kNewTab),
      SendAccelerator(kBrowserViewElementId,
                      ui::Accelerator(ui::VKEY_2, kControlCommandModifier)),
      WaitForActiveTabChange(1), CheckActiveContentsHasFocus());
}

// Check focus for the MultiContentView when in split view
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ActiveContentsViewHasFocus) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      FocusWebContents(kNewTab),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      FocusWebContents(kSecondTab),
      CheckResult([this]() { return tab_strip_model()->count(); }, 3u),
      EnterSplitView(2, 0), WaitForActiveTabChange(2),
      CheckActiveContentsHasFocus());
}

// Split view active tab change while browser window doesn't have focus. This
// is used to simulate tab switching scenarios using Tab Search
// TODO(https://crbug.com/422941990): Flaky (times out) on Linux and Windows
// debug bots.
#if !defined(NDEBUG) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX))
#define MAYBE_TabChangeInSplitViewWithInactiveBrowserWindow \
  DISABLED_TabChangeInSplitViewWithInactiveBrowserWindow
#else
#define MAYBE_TabChangeInSplitViewWithInactiveBrowserWindow \
  TabChangeInSplitViewWithInactiveBrowserWindow
#endif
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MAYBE_TabChangeInSplitViewWithInactiveBrowserWindow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  RunTestSequence(
      InstrumentTab(kFirstTab, 0),
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kFirstTab),
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      FocusWebContents(kNewTab),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      FocusWebContents(kSecondTab),
      CheckResult([this]() { return tab_strip_model()->count(); }, 3u),
      EnterSplitView(2, 0), WaitForActiveTabChange(2),
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      WaitForHide(kTabSearchBubbleElementId), WaitForActiveTabChange(1),
      CheckActiveContentsHasFocus());
}

// Switch to the not last used tab inside a split view from a not split tab
// while the browser is inactive. This is used to simulate tab switching
// scenarios using Tab Search
// TODO(https://crbug.com/422941990): Flaky (times out) on Linux and Windows
// debug bots.
#if !defined(NDEBUG) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX))
#define MAYBE_SwitchToSplitViewWithInactiveBrowserWindow \
  DISABLED_SwitchToSplitViewWithInactiveBrowserWindow
#else
#define MAYBE_SwitchToSplitViewWithInactiveBrowserWindow \
  SwitchToSplitViewWithInactiveBrowserWindow
#endif
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MAYBE_SwitchToSplitViewWithInactiveBrowserWindow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  RunTestSequence(
      InstrumentTab(kFirstTab, 0),
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kFirstTab),
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      FocusWebContents(kNewTab),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      FocusWebContents(kSecondTab),
      CheckResult([this]() { return tab_strip_model()->count(); }, 3u),
      EnterSplitView(2, 0), WaitForActiveTabChange(2),
      // Switch from the split view to a regular tab
      SelectTab(kTabStripElementId, 0, InputType::kMouse),
      WaitForActiveTabChange(0), FocusWebContents(kNewTab),
      // Launch the tab search bubble using the tab search button
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      // Switch from a regular tab directly to an inactive tab, which is on
      // the left side of a split with the TabSearch bubble dialog opened.
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      WaitForHide(kTabSearchBubbleElementId), WaitForActiveTabChange(1),
      CheckActiveContentsHasFocus(),
      // Switch out of the split view back to the regular tab
      SelectTab(kTabStripElementId, 0, InputType::kMouse),
      WaitForActiveTabChange(0), FocusWebContents(kNewTab),
      // Launch the tab search bubble using the tab search button
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      // Switch from a regular tab directly to an inactive tab, which is on
      // the right side of a split with the TabSearch bubble dialog opened.
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(2); }),
      WaitForHide(kTabSearchBubbleElementId), WaitForActiveTabChange(2),
      CheckActiveContentsHasFocus());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ResizesToMinWidth) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), ResizeWindow(1000),
      // Artificially lower min width so that testing on smaller devices does
      // not affect results.
      SetMinWidth(60),
      CheckResize(
          10000, base::BindRepeating([](double start_width, double end_width) {
            // On large window, uses flat min width.
            return end_width == 60 - MultiContentsView::kSplitViewContentInset;
          })));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ResizesToSnapPointWidth) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      MultiContentsViewLayoutObserver,
      kMultiContentsViewLayoutInitialResizeObserver);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      MultiContentsViewLayoutObserver,
      kMultiContentsViewLayoutSnapResizeObserver);

  RunTestSequence(
      CreateTabsAndEnterSplitView(), ResizeWindow(1000),
      // Resize outside of the snap point width
      CheckResizeWithId(
          100, base::BindRepeating([](double start_width, double end_width) {
            // Rounding differences mean this width is only changed by 199
            // instead of 200.
            return start_width == end_width + 199;
          }),
          kMultiContentsViewLayoutInitialResizeObserver),
      // Resize back to within the snap point margin and snap back to 50% width
      CheckResizeWithId(
          -96, base::BindRepeating([](double start_width, double end_width) {
            // On large window, uses snap point width.
            return end_width == start_width;
          }),
          kMultiContentsViewLayoutSnapResizeObserver));
}

// TODO(crbug.com/399212996): Flaky on linux_chromium_asan_rel_ng, linux-rel
// and linux-chromeos-rel.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_ResizesToMinWidthPercentage DISABLED_ResizesToMinWidthPercentage
#else
#define MAYBE_ResizesToMinWidthPercentage ResizesToMinWidthPercentage
#endif
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MAYBE_ResizesToMinWidthPercentage) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), ResizeWindow(500), SetMinWidth(60),
      CheckResize(
          10000, base::BindRepeating([](double start_width, double end_width) {
            // On small window, uses percentage of window size vs. flat width
            // for min. Don't check exact number to avoid rounding issues.
            return end_width <
                       (60 - MultiContentsView::kSplitViewContentInset) &&
                   end_width > 0;
          })));
}

// TODO(crbug.com/399212996): Flaky on Linux, ChromeOS and Win-ASAN.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
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
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2),
      // Since tab 0 and 1 are part of a split view and tab 0 was the most
      // recently focused half of the split it should become the active tab, but
      // both tabs will be visible.
      SelectTab(kTabStripElementId, 1, InputType::kMouse, 0),
      WaitForActiveTabChange(0),
      // Select another tab in the split view and ensure the active index
      // doesn't change since it isn't the currently focused tab.
      SelectTab(kTabStripElementId, 1, InputType::kMouse, 0),
      WaitForActiveTabChange(0));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ResizeMouseDoubleClickSwapsSplitViews) {
  using MultiContentsViewSwapObserver =
      views::test::PollingViewObserver<bool, MultiContentsView>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewSwapObserver,
                                      kMultiContentsViewSwapObserver);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  RunTestSequence(
      // Create a split view with and verify web contents are as expected and
      // the active index is correct.
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, GURL(chrome::kChromeUINewTabURL)),
      CreateTabsAndEnterSplitView(), Check([&]() {
        return multi_contents_view()
                   ->start_contents_view_for_testing()
                   ->GetWebContents()
                   ->GetVisibleURL() == GURL(chrome::kChromeUISettingsURL);
      }),
      Check([&]() {
        return multi_contents_view()
                   ->end_contents_view_for_testing()
                   ->GetWebContents()
                   ->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL);
      }),
      CheckResult([this]() { return tab_strip_model()->active_index(); }, 0),
      // Simulate a double click on the resize area to trigger the split tabs to
      // swap.
      Do([&]() {
        auto* resize_area = multi_contents_view()->resize_area_for_testing();
        gfx::Point center(resize_area->width() / 2, resize_area->height() / 2);
        ui::MouseEvent press_event(
            ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
        ui::MouseEvent release_event(ui::EventType::kMouseReleased, center,
                                     center, ui::EventTimeForNow(),
                                     ui::EF_LEFT_MOUSE_BUTTON,
                                     ui::EF_LEFT_MOUSE_BUTTON);
        press_event.SetClickCount(2);
        release_event.SetClickCount(2);
        resize_area->OnMousePressed(press_event);
        resize_area->OnMouseReleased(release_event);
      }),
      // Verify the web contents in the split have swapped and the active index
      // is correct.
      PollView(kMultiContentsViewSwapObserver, kMultiContentsViewElementId,
               [&](const MultiContentsView* multi_contents_view) -> bool {
                 bool first_web_contents_set =
                     multi_contents_view->start_contents_view_for_testing()
                         ->GetWebContents()
                         ->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL);
                 bool second_web_contents_set =
                     multi_contents_view->end_contents_view_for_testing()
                         ->GetWebContents()
                         ->GetVisibleURL() ==
                     GURL(chrome::kChromeUISettingsURL);
                 return first_web_contents_set && second_web_contents_set;
               }),
      WaitForState(kMultiContentsViewSwapObserver, true),
      WaitForActiveTabChange(1), CheckActiveContentsHasFocus());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ResizeGestureDoubleTapSwapsSplitViews) {
  using MultiContentsViewSwapObserver =
      views::test::PollingViewObserver<bool, MultiContentsView>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewSwapObserver,
                                      kMultiContentsViewSwapObserver);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  RunTestSequence(
      // Create a split view with and verify web contents are as expected and
      // the active index is correct.
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, GURL(chrome::kChromeUINewTabURL)),
      CreateTabsAndEnterSplitView(), Check([&]() {
        return multi_contents_view()
                   ->start_contents_view_for_testing()
                   ->GetWebContents()
                   ->GetVisibleURL() == GURL(chrome::kChromeUISettingsURL);
      }),
      Check([&]() {
        return multi_contents_view()
                   ->end_contents_view_for_testing()
                   ->GetWebContents()
                   ->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL);
      }),
      CheckResult([this]() { return tab_strip_model()->active_index(); }, 0),
      // Simulate a double press gesture event on the resize area to trigger the
      // split tabs to swap.
      Do([&]() {
        auto* resize_area = multi_contents_view()->resize_area_for_testing();
        gfx::Point center(resize_area->width() / 2, resize_area->height() / 2);
        ui::GestureEventDetails details(ui::EventType::kGestureTap);
        details.set_tap_count(2);
        ui::GestureEvent gesture_event(center.x(), center.y(), ui::EF_NONE,
                                       ui::EventTimeForNow(), details);
        resize_area->OnGestureEvent(&gesture_event);
      }),
      // Verify the web contents in the split have swapped and the active index
      // is correct.
      PollView(kMultiContentsViewSwapObserver, kMultiContentsViewElementId,
               [&](const MultiContentsView* multi_contents_view) -> bool {
                 bool first_web_contents_set =
                     multi_contents_view->start_contents_view_for_testing()
                         ->GetWebContents()
                         ->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL);
                 bool second_web_contents_set =
                     multi_contents_view->end_contents_view_for_testing()
                         ->GetWebContents()
                         ->GetVisibleURL() ==
                     GURL(chrome::kChromeUISettingsURL);
                 return first_web_contents_set && second_web_contents_set;
               }),
      WaitForState(kMultiContentsViewSwapObserver, true),
      WaitForActiveTabChange(1), CheckActiveContentsHasFocus());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       ContentsDividersHiddenInSplitView) {
  RunTestSequence(
      // Open the bookmarks side panel.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
      WaitForShow(kSidePanelElementId),
      // Verify expected contents separators are visible. Note, only one side
      // panel separator should be visible and the side panel is right aligned
      // by default.
      WaitForShow(kContentsSeparatorTopEdgeElementId),
      WaitForShow(kContentsSeparatorTrailingEdgeElementId),
      WaitForHide(kContentsSeparatorLeadingEdgeElementId),
      WaitForShow(kContentsSeparatorTrailingTopCornerElementId),
      WaitForHide(kContentsSeparatorLeadingTopCornerElementId),
      // Open split view.
      CreateTabsAndEnterSplitView(),
      // Verify no contents separators are visible.
      WaitForHide(kContentsSeparatorTopEdgeElementId),
      WaitForHide(kContentsSeparatorTrailingEdgeElementId),
      WaitForHide(kContentsSeparatorLeadingEdgeElementId),
      WaitForHide(kContentsSeparatorTrailingTopCornerElementId),
      WaitForHide(kContentsSeparatorLeadingTopCornerElementId));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, RoundedCornersForSplitView) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      // Ensure the contents web views have rounded corners.
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return !multi_contents_view->GetActiveContentsView()
                              ->GetBackgroundRadii()
                              .IsEmpty() &&
                         !multi_contents_view->GetInactiveContentsView()
                              ->GetBackgroundRadii()
                              .IsEmpty();
                }),
      // Add a regular tab to the tab strip
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2),
      // Ensure the active contents web view doesn't have rounded corners.
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return multi_contents_view->GetActiveContentsView()
                      ->GetBackgroundRadii()
                      .IsEmpty();
                }));
}
#endif

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, BackgroundVisibility) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      // Ensure the background is visible when in sidebyside view
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return multi_contents_view->background_view_for_testing()
                      ->GetVisible();
                }),
      // Add a regular tab to the tab strip
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2),
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return !multi_contents_view->background_view_for_testing()
                              ->GetVisible();
                }));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MiniToolbarVisibilityForContents) {
  bool visible_on_active_contents =
      features::kSideBySideMiniToolbarActiveConfiguration.Get() !=
      features::MiniToolbarActiveConfiguration::Hide;
  RunTestSequence(
      // Open split view.
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      // Verify the mini toolbar is visible for the inactive contents.
      Check([&]() {
        return multi_contents_view()
                   ->mini_toolbar_for_testing(0)
                   ->GetVisible() == visible_on_active_contents;
      }),
      // Verify the mini toolbar visibility on active contents.
      Check([&]() {
        return multi_contents_view()->mini_toolbar_for_testing(1)->GetVisible();
      }),
      // Focus inactive contents and verify active tab.
      FocusInactiveTabInSplit(), WaitForActiveTabChange(1),
      // Verify the mini toolbar is only visible on the newly inactive contents.
      Check([&]() {
        return multi_contents_view()->mini_toolbar_for_testing(0)->GetVisible();
      }),
      // Verify the mini toolbar visibility on the newly active contents.
      Check([&]() {
        return multi_contents_view()
                   ->mini_toolbar_for_testing(1)
                   ->GetVisible() == visible_on_active_contents;
      }));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MiniToolbarHidesNewTabPageDomain) {
  RunTestSequence(
      // Open split view and navigate the first tab to the NTP.
      CreateTabsAndEnterSplitView(), WaitForActiveTabChange(0),
      NavigateWebContents(kNewTab, GURL("chrome://newtab")),
      // Focus inactive contents and verify inactive tab doesn't show the
      // domain.
      FocusInactiveTabInSplit(), WaitForActiveTabChange(1),
      // Verify the mini toolbar visibility on inactive contents.
      Check([&]() {
        return multi_contents_view()->mini_toolbar_for_testing(0)->GetVisible();
      }),
      CheckResult(
          [&]() {
            return multi_contents_view()
                ->mini_toolbar_for_testing(0)
                ->domain_label_for_testing()
                ->GetText();
          },
          u""));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, KeyboardShortcutCreatesSplit) {
  ui::Accelerator accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_NEW_SPLIT_TAB, &accelerator));
  RunTestSequence(
      CheckResult(
          [&]() {
            return browser()->tab_strip_model()->GetActiveTab()->IsSplit();
          },
          false),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      SendAccelerator(kBrowserViewElementId, accelerator),
      CheckResult(
          [&]() {
            return browser()->tab_strip_model()->GetActiveTab()->IsSplit();
          },
          true),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // Pressing the accelerator again shouldn't do anything since the active
      // tab is already in a split
      SendAccelerator(kBrowserViewElementId, accelerator),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2));
}

using ContentsViewOutlineHighlightObserver =
    views::test::PollingViewObserver<bool, MultiContentsView>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ContentsViewOutlineHighlightObserver,
                                    kContentsViewOutlineHighlightObserver);

class MultiContentsViewOutlineHighlightUiTest : public MultiContentsViewUiTest {
 public:
  MultiContentsViewOutlineHighlightUiTest() = default;

  auto SimulateTriggeringPermissionPrompt(bool show_prompt) {
    return Do([this, show_prompt]() {
      split_tabs::SplitTabHighlightController* const
          split_tab_highlight_controller =
              browser()
                  ->browser_window_features()
                  ->split_tab_highlight_controller();
      if (show_prompt) {
        split_tab_highlight_controller->OnPermissionPromptShown();
      } else {
        split_tab_highlight_controller->OnPermissionPromptHidden();
      }
    });
  }

  auto CheckOutlineHighlightState(size_t content_container_index,
                                  bool is_highlighted) {
    auto result = Steps(
        PollView(kContentsViewOutlineHighlightObserver,
                 kMultiContentsViewElementId,
                 [=](const MultiContentsView* multi_contents_view) -> bool {
                   auto contents_container_views =
                       multi_contents_view->contents_container_views();
                   EXPECT_LT(content_container_index,
                             contents_container_views.size());
                   return contents_container_views[content_container_index]
                       ->contents_outline_view()
                       ->is_highlighted();
                 }),
        WaitForState(kContentsViewOutlineHighlightObserver, is_highlighted),
        StopObservingState(kContentsViewOutlineHighlightObserver));
    AddDescriptionPrefix(result, "CheckOutlineHighlightState()");
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(MultiContentsViewOutlineHighlightUiTest,
                       ShowHighlightOnOmniboxDropDownOpen) {
  RunTestSequence(
      // Create a split view
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      FocusElement(kNewTab), CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false),
      // Focus omnibox and verify the start tab has an outline
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"query"),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      // Move focus to the end tab and verify start tab outline is hidden
      FocusInactiveTabInSplit(), CheckOutlineHighlightState(0, false),
      // Focus omnibox and confirm that highlight is shown in the end tab
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"query"),
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, true));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewOutlineHighlightUiTest,
                       HighlightUpdatesForMultipleSplitTabs) {
  RunTestSequence(
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false),
      // Create a split tab and verify highlight is shown for start tab
      AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"query"),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      SendKeyPress(kOmniboxElementId, ui::VKEY_ESCAPE),
      // Create a second split tab
      AddInstrumentedTab(kThirdTab, GetTestUrl()),
      AddInstrumentedTab(kFourthTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 2), EnterSplitView(2, 3),
      // Focus on omnibox and verify highlight is shown for start tab
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"query"),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      // Remove focus from the omnibox split
      FocusElement(kThirdTab), CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewOutlineHighlightUiTest,
                       HighlightShowsForPermissionPrompt) {
  RunTestSequence(
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false),
      // Create a split tab and simulate the permission prompt is shown
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      FocusElement(kNewTab), CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false),
      SimulateTriggeringPermissionPrompt(true),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      // Simulate the permission prompt closed
      SimulateTriggeringPermissionPrompt(false),
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewOutlineHighlightUiTest,
                       CoordinateHighlightShowReasons) {
  RunTestSequence(
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false),
      // Create a split tab and focus the omnibox
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"query"),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      // Trigger the permission prompt while focusing the omnibox should
      // continue showing the scrim.
      SimulateTriggeringPermissionPrompt(true),
      CheckOutlineHighlightState(0, true),
      // Removing focus from the omnibox should still have the scrim continue to
      // show because the permission prompt is still showing.
      FocusElement(kNewTab), CheckOutlineHighlightState(0, true),
      // The scrim should hide after the prompt is closed because there is no
      // longer any reason to continue showing the scrim.
      SimulateTriggeringPermissionPrompt(false),
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewOutlineHighlightUiTest,
                       HighlightShowsForPageInfoBubble) {
  RunTestSequence(
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false), InstrumentTab(kNewTab),
      NavigateWebContents(kNewTab, GURL(chrome::kChromeUISettingsURL)),
      AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      FocusElement(kNewTab), CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false), PressButton(kLocationIconElementId),
      CheckOutlineHighlightState(0, true), CheckOutlineHighlightState(1, false),
      // Clicking the location icon again should close the page info bubble and
      // hide the scrim.
      MoveMouseTo(kLocationIconElementId), ClickMouse(),
      CheckOutlineHighlightState(0, false),
      CheckOutlineHighlightState(1, false));
}

// TODO(crbug.com/414590951): There's limited support for testing drag and drop
// on various platforms. These should be re-enabled as support is added.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)

gfx::Point PointForDropTargetFromView(views::View* view) {
  return view->GetBoundsInScreen().right_center() - gfx::Vector2d(10, 0);
}

class MultiContentsViewDragEntrypointsUiTest : public MultiContentsViewUiTest {
 public:
  using MultiContentsViewUiTest::MultiContentsViewUiTest;

  void SetUp() override {
    http_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.InitializeAndListen());
    MultiContentsViewUiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    http_server_.StartAcceptingConnections();
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  GURL GetURL(std::string path) { return http_server_.GetURL(path); }

  auto PointForDropTarget() const {
    return base::BindLambdaForTesting(
        [](views::View* view) { return PointForDropTargetFromView(view); });
  }

  // The standard DragMouseTo verb waits for the mouse to reach the
  // destination. This version does not, since the mouse position sometimes
  // doesn't get reported immediately (see `WaitForDropTargetVisible`).
  auto DragMouseToWithoutWait(
      ElementSpecifier target_view,
      base::RepeatingCallback<gfx::Point(views::View*)> pos) {
    return WithView(target_view, [pos = std::move(pos)](views::View* view) {
      base::RunLoop press_loop(base::RunLoop::Type::kNestableTasksAllowed);
      EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
          ui_controls::MouseButton::LEFT, ui_controls::MouseButtonState::DOWN,
          press_loop.QuitClosure()));
      press_loop.Run();
      gfx::Point target_location = std::move(pos).Run(view);

      EXPECT_TRUE(
          ui_controls::SendMouseMove(target_location.x(), target_location.y()));
    });
  }

  auto WaitForDropTargetVisible() {
    // This method waits for the drop target to be visible, but also sends
    // periodic mouse movement events while waiting. The mouse movements are
    // needed to deflake this test on some Mac platforms: in the normal case,
    // the initial mouse movement initiates a drag session, which later
    // receives "drag updated" events from the OS. However, for some of the
    // flakes, these updates are never sent by the OS. Manually generating
    // the events seems to fix this.
    // We really only need one event timed to execute after the drag session
    // starts; an alternative approach would be to add observation to the
    // Mac DnD client. Until then, periodic events does the trick.
    //
    // Note, both branches of AnyOf end with WaitForShow to ensure that the
    // only way this step terminates successfully is if the view is shown.
    return AnyOf(
        RunSubsequence(WaitForShow(
            MultiContentsDropTargetView::kMultiContentsDropTargetElementId)),
        RunSubsequence(
            Steps(
                // Programmatically generate a list of mouse movement steps.
                []() {
                  constexpr int kMouseMovements = 20;
                  constexpr base::TimeDelta kMovementDelay =
                      base::Milliseconds(250);
                  MultiStep mouse_moves;
                  // Jitter applied to the mouse move destination to ensure it
                  // changes between each step.
                  int jitter = 3;
                  for (int mouse_move_events = 0;
                       mouse_move_events < kMouseMovements;
                       ++mouse_move_events) {
                    jitter *= -1;
                    AddStep(mouse_moves, Do([kMovementDelay] {
                              base::PlatformThread::Sleep(kMovementDelay);
                            }));
                    AddStep(mouse_moves,
                            WithView(kMultiContentsViewElementId,
                                     [jitter](views::View* view) {
                                       gfx::Point target =
                                           PointForDropTargetFromView(view);
                                       EXPECT_TRUE(ui_controls::SendMouseMove(
                                           target.x() + jitter, target.y()));
                                     }));
                  }
                  return mouse_moves;
                }()),
            // This branch also waits for visibility to prevent it from exiting
            // prematurely.
            WaitForShow(MultiContentsDropTargetView::
                            kMultiContentsDropTargetElementId)));
  }

 private:
  net::EmbeddedTestServer http_server_;
};

// TODO(crbug.com/414590951): This test has been flaky on some MacOS versions,
// and DnD testing isn't well-supported for other platforms.
IN_PROC_BROWSER_TEST_F(MultiContentsViewDragEntrypointsUiTest,
                       DISABLED_ShowsDropTargetOnLinkDragged) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GetURL("/links.html"), 0),
      WaitForActiveTabChange(0),
      // Drag an href element to the drop target area. The drop
      // target should be shown.
      MoveMouseTo(kNewTab, DeepQuery{"#title1"}),
      DragMouseToWithoutWait(kMultiContentsViewElementId, PointForDropTarget()),
      WaitForDropTargetVisible());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDragEntrypointsUiTest,
                       DISABLED_BackgroundVisibleWhenDropTargetShown) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GetURL("/links.html"), 0),
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return !multi_contents_view->background_view_for_testing()
                              ->GetVisible();
                }),
      WaitForActiveTabChange(0),
      // Drag an href element to the drop target area. The drop
      // target should be shown.
      MoveMouseTo(kNewTab, DeepQuery{"#title1"}),
      DragMouseToWithoutWait(kMultiContentsViewElementId, PointForDropTarget()),
      WaitForDropTargetVisible(),
      CheckView(kMultiContentsViewElementId,
                [](MultiContentsView* multi_contents_view) -> bool {
                  return multi_contents_view->background_view_for_testing()
                      ->GetVisible();
                }));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDragEntrypointsUiTest,
                       DoesNotShowDropTargetOnNonURLDragged) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GetURL("/button.html"), 0),
      WaitForActiveTabChange(0),
      // Dragging a non-url to the drop target area should have no
      // effect.
      MoveMouseTo(kNewTab, DeepQuery{"#button"}),
      DragMouseToWithoutWait(kMultiContentsViewElementId, PointForDropTarget()),
      WaitForHide(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId));
}

class MultiContentsViewBookmarkDragEntrypointsUiTest
    : public MultiContentsViewDragEntrypointsUiTest {
 public:
  using MultiContentsViewDragEntrypointsUiTest::
      MultiContentsViewDragEntrypointsUiTest;

  void SetUpOnMainThread() override {
    MultiContentsViewDragEntrypointsUiTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
  }

  // Names the bookmark bar button for the given bookmark folder.
  auto NameBookmarkButton(std::string assigned_name,
                          std::u16string node_title) {
    return NameViewRelative(
        kBookmarkBarElementId, assigned_name,
        base::BindLambdaForTesting([=](views::View* view) -> views::View* {
          auto* const bookmark_bar = views::AsViewClass<BookmarkBarView>(view);
          CHECK(bookmark_bar);
          for (views::View* child : bookmark_bar->children()) {
            auto* bookmark_button = views::AsViewClass<BookmarkButton>(child);
            if (bookmark_button && bookmark_button->GetText() == node_title) {
              return bookmark_button;
            }
          }
          NOTREACHED() << "Bookmark button with title " << node_title
                       << " not found.";
        }));
  }
};

// TODO(crbug.com/414590951): This test has been flaky on some MacOS versions,
// and DnD testing isn't well-supported for other platforms.
IN_PROC_BROWSER_TEST_F(MultiContentsViewBookmarkDragEntrypointsUiTest,
                       DISABLED_ShowsDropTargetOnBookmarkedLinkDragged) {
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const std::u16string bookmark_title = u"Bookmark";
  model->AddNewURL(model->bookmark_bar_node(), 0, u"Bookmark",
                   GetURL("/links.html"));

  const std::string kBookmarkButtonId = "bookmark_button";
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
      WaitForActiveTabChange(0), WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kBookmarkButtonId, bookmark_title),
      MoveMouseTo(kBookmarkButtonId),
      DragMouseToWithoutWait(kMultiContentsViewElementId, PointForDropTarget()),
      WaitForDropTargetVisible());
}
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
