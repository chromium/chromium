// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/screen.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace {

// TODO(crbug.com/414590951): Tab DnD tests not working on Mac.
// TODO(crbug.com/500937645): Re-enable the test on Windows.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                                    kBrowserCountPoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kDragStatePoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kWaitFinishedPoller);

class MultiContentsViewTabDragEntrypointsUiTest
    : public SplitViewInteractiveTestMixin<
          TabStripInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  MultiContentsViewTabDragEntrypointsUiTest() = default;
  ~MultiContentsViewTabDragEntrypointsUiTest() override = default;

  gfx::Point GetPointForDropSide(MultiContentsDropTargetView::DropSide side) {
    const gfx::Rect bounds = GetBrowserView().GetBoundsInScreen();
    switch (side) {
      case MultiContentsDropTargetView::DropSide::START:
        return gfx::Point(bounds.left_center().x() + 10,
                          bounds.left_center().y());
      case MultiContentsDropTargetView::DropSide::END:
        return gfx::Point(bounds.right_center().x() - 10,
                          bounds.right_center().y());
      case MultiContentsDropTargetView::DropSide::BOTTOM:
      default:
        NOTREACHED();
    }
  }

  BrowserView& GetBrowserView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    EXPECT_TRUE(browser_view != nullptr);
    return *browser_view;
  }

  MultiContentsDropTargetView* GetDropTargetView(BrowserView& browser_view) {
    return views::AsViewClass<MultiContentsDropTargetView>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            MultiContentsDropTargetView::kMultiContentsDropTargetElementId,
            browser_view.GetElementContext()));
  }

  base::RepeatingCallback<size_t()> GetBrowserCount() {
    return base::BindRepeating(
        []() { return GlobalBrowserCollection::GetInstance()->GetSize(); });
  }

  base::RepeatingCallback<bool()> GetDragActive() {
    return base::BindRepeating([]() { return TabDragController::IsActive(); });
  }

  auto NameTabViewAt(std::string_view name, int index) {
    return NameView(name, base::BindLambdaForTesting([this, index]() {
                      return GetBrowserView().tab_strip_view()->GetTabAnchorViewAt(
                          index);
                    }));
  }

  auto WaitTime(base::TimeDelta timeout) {
    return Steps(
        Do([this]() { last_wait_start_ = base::TimeTicks::Now(); }),
        PollState(kWaitFinishedPoller,
                  [this, timeout]() {
                    return !last_wait_start_.is_null() &&
                           (base::TimeTicks::Now() - last_wait_start_ >=
                            timeout);
                  }),
        WaitForState(kWaitFinishedPoller, true),
        Do([this]() { last_wait_start_ = base::TimeTicks(); }));
  }

 private:
  base::TimeTicks last_wait_start_;
};


class MultiContentsViewTabDragEntrypointsUiParamTest
    : public MultiContentsViewTabDragEntrypointsUiTest,
      public testing::WithParamInterface<
          MultiContentsDropTargetView::DropSide> {
 public:
  MultiContentsViewTabDragEntrypointsUiParamTest() = default;
  ~MultiContentsViewTabDragEntrypointsUiParamTest() override = default;
};

IN_PROC_BROWSER_TEST_P(MultiContentsViewTabDragEntrypointsUiParamTest,
                       DragAndDrop) {
  // TODO(crbug.com/448651072): Remove when Weston support is added.
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "Weston's implementation of tab dragging is incompatible "
                    "with creating a split view.";
  }
#endif

  const auto drop_side = GetParam();

  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL(), 1),
      AddInstrumentedTab(kSecondTab, chrome::ChromeUINewTabURLAsGURL(), 2),
      WaitForActiveTabChange(2),
      NameTabViewAt("Tab to drag", 1),
      MoveMouseTo("Tab to drag"),
      DragMouseTo(kBrowserViewElementId, CenterPoint(), /*release=*/false),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2u),
      MoveMouseTo(base::BindLambdaForTesting(
          [&]() { return GetPointForDropSide(drop_side); })),
      InAnyContext(WaitForShow(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId)),
      InAnyContext(CheckView(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId,
          base::BindOnce(
              [](MultiContentsDropTargetView::DropSide side,
                 MultiContentsDropTargetView* view) {
                return view->side() == side;
              },
              drop_side))),
      ReleaseMouse(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false),
      CheckResult(
          [this]() {
            return browser()->tab_strip_model()->GetActiveTab()->IsSplit();
          },
          true));
}

IN_PROC_BROWSER_TEST_P(MultiContentsViewTabDragEntrypointsUiParamTest,
                       ShowAndHideDropTarget) {
  // TODO(crbug.com/448651072): Remove when Weston support is added.
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "Weston's implementation of tab dragging is incompatible "
                    "with creating a split view.";
  }
#endif

  const auto drop_side = GetParam();

  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL(), 1),
      AddInstrumentedTab(kSecondTab, chrome::ChromeUINewTabURLAsGURL(), 2),
      WaitForActiveTabChange(2),
      NameTabViewAt("Tab to drag", 1),
      MoveMouseTo("Tab to drag"),
      DragMouseTo(kBrowserViewElementId, CenterPoint(), /*release=*/false),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2u),
      MoveMouseTo(base::BindLambdaForTesting(
          [&]() { return GetPointForDropSide(drop_side); })),
      InAnyContext(WaitForShow(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId)),
      InAnyContext(CheckView(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId,
          base::BindOnce(
              [](MultiContentsDropTargetView::DropSide side,
                 MultiContentsDropTargetView* view) {
                return view->side() == side;
              },
              drop_side))),
      MoveMouseTo(kBrowserViewElementId),
      InAnyContext(WaitForHide(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId)),
      ReleaseMouse(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false));
}

IN_PROC_BROWSER_TEST_P(MultiContentsViewTabDragEntrypointsUiParamTest,
                       DragAndDropDisabledForChromePage) {
  // TODO(crbug.com/448651072): Remove when Weston support is added.
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "Weston's implementation of tab dragging is incompatible "
                    "with creating a split view.";
  }
#endif

  const auto drop_side = GetParam();

  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2),
      NameTabViewAt("Tab to drag", 1),
      MoveMouseTo("Tab to drag"),
      DragMouseTo(kBrowserViewElementId, CenterPoint(), /*release=*/false),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2u),
      MoveMouseTo(base::BindLambdaForTesting(
          [&]() { return GetPointForDropSide(drop_side); })),
      WaitTime(base::Milliseconds(500)),
      Check([this]() {
        return !GetDropTargetView(GetBrowserView())->GetVisible();
      }),
      ReleaseMouse(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false),
      CheckResult(
          [this]() {
            return browser()->tab_strip_model()->GetActiveTab()->IsSplit();
          },
          false));
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewTabDragEntrypointsUiTest,
                       DragAndDropDisabled) {
  // TODO(crbug.com/448651072): Remove when Weston support is added.
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "Weston's implementation of tab dragging is incompatible "
                    "with creating a split view.";
  }
#endif

#if BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See crbug.com/477426026.";
  }
#endif

  // Disable drag and drop.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSplitViewDragAndDropEnabled, false);

  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL(), 1),
      AddInstrumentedTab(kSecondTab, chrome::ChromeUINewTabURLAsGURL(), 2),
      WaitForActiveTabChange(2),
      NameTabViewAt("Tab to drag", 1),
      MoveMouseTo("Tab to drag"),
      DragMouseTo(kBrowserViewElementId, CenterPoint(), /*release=*/false),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2u),
      MoveMouseTo(base::BindLambdaForTesting([&]() {
        return GetPointForDropSide(MultiContentsDropTargetView::DropSide::START);
      })),
      WaitTime(base::Milliseconds(500)),
      ReleaseMouse(),
      Check([this]() {
        return !GetDropTargetView(GetBrowserView())->GetVisible();
      }),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiContentsViewTabDragEntrypointsUiParamTest,
    ::testing::Values(MultiContentsDropTargetView::DropSide::START,
                      MultiContentsDropTargetView::DropSide::END));
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)

}  // namespace
