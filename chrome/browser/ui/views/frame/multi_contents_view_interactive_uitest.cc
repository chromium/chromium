// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/clamped_math.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/test/split_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/base_event_utils.h"
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

  auto CheckResizeValues(base::RepeatingCallback<bool(double, double)> check) {
    // MultiContentsView overrides Layout, causing an edge case where resizes
    // don't take effect until the next layout pass. Use PollView and
    // WaitForState to wait for the expected layout pass to be completed.
    using MultiContentsViewLayoutObserver =
        views::test::PollingViewObserver<bool, MultiContentsView>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewLayoutObserver,
                                        kMultiContentsViewLayoutObserver);

    auto result = Steps(
        PollView(kMultiContentsViewLayoutObserver,
                 MultiContentsView::kMultiContentsViewElementId,
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
        WaitForState(kMultiContentsViewLayoutObserver, true));
    AddDescriptionPrefix(result, "CheckResizeValues()");
    return result;
  }

  // Perform a check on the contents view sizes following a direct resize call
  auto CheckResize(int resize_amount,
                   base::RepeatingCallback<bool(double, double)> check) {
    auto result = Steps(Do([resize_amount, this]() {
                          multi_contents_view()->OnResize(resize_amount, true);
                        }),
                        CheckResizeValues(check));
    AddDescriptionPrefix(result, "CheckResize()");
    return result;
  }

  // Perform a check on the contents view sizes following a keyboard-triggered
  // resize
  auto CheckResizeKey(ui::KeyboardCode key_code,
                      base::RepeatingCallback<bool(double, double)> check) {
    auto result = Steps(
        FocusElement(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId),
        SendKeyPress(
            MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
            key_code),
        CheckResizeValues(check));
    AddDescriptionPrefix(result, "CheckResizeKey()");
    return result;
  }

  auto ResizeWindow(int width) {
    auto result = Steps(Do([width, this]() {
      BrowserView::GetBrowserViewForBrowser(browser())->SetContentsSize(
          gfx::Size(width, 1000));
    }));
    AddDescriptionPrefix(result, "ResizeWindow()");
    return result;
  }

  auto SetMinWidth(int width) {
    auto result = Steps(Do([width, this]() {
      multi_contents_view()->SetMinWidthForTesting(width);
    }));
    AddDescriptionPrefix(result, "SetMinWidth()");
    return result;
  }

  auto CheckTabIsActive(int index) {
    return CheckResult([this]() { return tab_strip_model()->active_index(); },
                       index);
  }

  auto CheckActiveContentsHasFocus() {
    return CheckView(
        MultiContentsView::kMultiContentsViewElementId,
        [](MultiContentsView* multi_contents_view) -> bool {
          return multi_contents_view->GetActiveContentsView()->HasFocus();
        });
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
                  FocusInactiveTabInSplit(), CheckTabIsActive(1),
                  CheckActiveContentsHasFocus());
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
      CheckTabIsActive(1), CheckActiveContentsHasFocus());
}

// Check focus for the MultiContentView when in split view
IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ActiveContentsViewHasFocus) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      FocusWebContents(kNewTab),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      FocusWebContents(kSecondTab),
      CheckResult([this]() { return tab_strip_model()->count(); }, 3u),
      EnterSplitView(2, 0), CheckTabIsActive(2), CheckActiveContentsHasFocus());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest, ResizesToMinWidth) {
  RunTestSequence(
      CreateTabsAndEnterSplitView(), ResizeWindow(1000),
      // Artificially lower min width so that testing on smaller devices does
      // not affect results.
      SetMinWidth(60),
      CheckResize(10000,
                  base::BindRepeating([](double start_width, double end_width) {
                    // On large window, uses flat min width.
                    return end_width ==
                           60 - MultiContentsView::contents_inset_for_testing();
                  })));
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
                       (60 - MultiContentsView::contents_inset_for_testing()) &&
                   end_width > 0;
          })));
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
      PollView(kMultiContentsViewSwapObserver,
               MultiContentsView::kMultiContentsViewElementId,
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
      WaitForState(kMultiContentsViewSwapObserver, true), CheckTabIsActive(1),
      CheckActiveContentsHasFocus());
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
      PollView(kMultiContentsViewSwapObserver,
               MultiContentsView::kMultiContentsViewElementId,
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
      WaitForState(kMultiContentsViewSwapObserver, true), CheckTabIsActive(1),
      CheckActiveContentsHasFocus());
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
      WaitForShow(kContentsSeparatorViewElementId),
      WaitForShow(kRightAlignedSidePanelSeparatorViewElementId),
      WaitForHide(kLeftAlignedSidePanelSeparatorViewElementId),
      WaitForShow(kSidePanelRoundedCornerViewElementId),
      // Open split view.
      CreateTabsAndEnterSplitView(),
      // Verify no contents separators are visible.
      WaitForHide(kContentsSeparatorViewElementId),
      WaitForHide(kRightAlignedSidePanelSeparatorViewElementId),
      WaitForHide(kLeftAlignedSidePanelSeparatorViewElementId),
      WaitForHide(kSidePanelRoundedCornerViewElementId));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewUiTest,
                       MiniToolbarShownForInactiveContents) {
  RunTestSequence(
      // Open split view.
      CreateTabsAndEnterSplitView(), CheckTabIsActive(0),
      // Verify the mini toolbar is only visible for the inactive contents.
      Check([&]() {
        return !multi_contents_view()
                    ->mini_toolbar_for_testing(0)
                    ->GetVisible();
      }),
      Check([&]() {
        return multi_contents_view()->mini_toolbar_for_testing(1)->GetVisible();
      }),
      // Focus inactive contents and verify active tab.
      FocusInactiveTabInSplit(), CheckTabIsActive(1),
      // Verify the mini toolbar is only visile for the newly inactive contents.
      Check([&]() {
        return multi_contents_view()->mini_toolbar_for_testing(0)->GetVisible();
      }),
      Check([&]() {
        return !multi_contents_view()
                    ->mini_toolbar_for_testing(1)
                    ->GetVisible();
      }));
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
        RunSubsequence(WaitForShow(kMultiContentsViewDropTargetElementId)),
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
                    AddStep(
                        mouse_moves,
                        WithView(MultiContentsView::kMultiContentsViewElementId,
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
            WaitForShow(kMultiContentsViewDropTargetElementId)));
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
      CheckTabIsActive(0),
      // Drag an href element to the drop target area. The drop
      // target should be shown.
      MoveMouseTo(kNewTab, DeepQuery{"#title1"}),
      DragMouseToWithoutWait(MultiContentsView::kMultiContentsViewElementId,
                             PointForDropTarget()),
      WaitForDropTargetVisible());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewDragEntrypointsUiTest,
                       DoesNotShowDropTargetOnNonURLDragged) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GetURL("/button.html"), 0),
      CheckTabIsActive(0),
      // Dragging a non-url to the drop target area should have no
      // effect.
      MoveMouseTo(kNewTab, DeepQuery{"#button"}),
      DragMouseToWithoutWait(MultiContentsView::kMultiContentsViewElementId,
                             PointForDropTarget()),
      WaitForHide(kMultiContentsViewDropTargetElementId));
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
      CheckTabIsActive(0), WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kBookmarkButtonId, bookmark_title),
      MoveMouseTo(kBookmarkButtonId),
      DragMouseToWithoutWait(MultiContentsView::kMultiContentsViewElementId,
                             PointForDropTarget()),
      WaitForDropTargetVisible());
}
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
