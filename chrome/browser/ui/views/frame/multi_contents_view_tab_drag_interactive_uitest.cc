// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/test/split_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/screen.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace {

// TODO(crbug.com/414590951): Tab DnD tests not working on ChromeOS and Mac.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

// TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
#define SKIP_FOR_WAYLAND()                                                \
  if (!ui::OzonePlatform::GetInstance()                                   \
           ->GetPlatformProperties()                                      \
           .supports_split_view_drag_and_drop) {                          \
    GTEST_SKIP() << "Skipping DnD test on Wayland (crbug.com/425715421)"; \
  }
#else
#define SKIP_FOR_WAYLAND()
#endif

MultiContentsDropTargetView* GetDropTargetView(BrowserView& browser_view) {
  return views::AsViewClass<MultiContentsDropTargetView>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          MultiContentsDropTargetView::kMultiContentsDropTargetElementId,
          browser_view.GetElementContext()));
}

// TODO(crbug.com/40249472): All of the helpers below are used as a workaround
// for limitations in Windows' drag and drop handling, where it locks into a
// message loop when drag starts. The helpers below workaround this by polling,
// without using any nested run loops for waiting.
//
// Due to the nature of dragging, each step must be executed as a callback
// within the previous step.
// This is a helper that allows running a set of closures, where each
// closure is expected to execute the next one. This is not functionally
// required, but makes the syntax a lot cleaner.
template <size_t kIndex, typename... Steps>
void RunDragStep(std::tuple<Steps...> steps, base::OnceClosure on_complete) {
  if constexpr (kIndex < sizeof...(Steps)) {
    auto step_to_run = std::move(std::get<kIndex>(steps));
    auto next_step = base::BindOnce(&RunDragStep<kIndex + 1, Steps...>,
                                    std::move(steps), std::move(on_complete));
    std::move(step_to_run).Run(std::move(next_step));
  } else {
    std::move(on_complete).Run();
  }
}

template <typename... Steps>
void DragSequence(Steps... steps) {
  RunDragStep<0, Steps...>(std::make_tuple(std::move(steps)...),
                           base::DoNothing());
}

// Polling within a drag loop is complicated: a typical `base::RunLoop` created
// within a drag loop will hang.
// This function works around this limitation by posting tasks to poll an
// arbitrary condition, then executing a callback once met.
void Poll(base::RepeatingCallback<bool()> condition,
          base::OnceClosure callback) {
  if (!condition.Run()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Poll, std::move(condition), std::move(callback)),
        base::Milliseconds(1));
    return;
  }
  std::move(callback).Run();
}

// A helper for observing the end of a tab dragging session.
// This should be created before drag loop is started.
class QuitTabDraggingObserver {
 public:
  explicit QuitTabDraggingObserver(TabStrip* tab_strip) {
    tab_strip->GetDragContext()->SetDragControllerCallbackForTesting(
        base::BindOnce(&QuitTabDraggingObserver::OnDragControllerSet,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  QuitTabDraggingObserver(const QuitTabDraggingObserver&) = delete;
  QuitTabDraggingObserver& operator=(const QuitTabDraggingObserver&) = delete;
  ~QuitTabDraggingObserver() = default;

  void Wait() & {
    timeout_warning_timer_.Start(
        FROM_HERE, TestTimeouts::action_max_timeout(),
        base::BindLambdaForTesting([]() {
          LOG(ERROR)
              << "QuitTabDraggingObserver::Wait() is taking a long time. "
                 "If this test times out, please check the comment for "
                 "QuitTabDraggingObserver to see if it should be using "
                 "BrowserChangeWaiter instead.";
          LOG(ERROR) << "Note: you might be using QuitTabDraggingObserver via "
                        "DragTabAndNotify() or "
                        "DragToDetachGroupAndNotify().";
        }));
    run_loop_.Run();
    timeout_warning_timer_.Stop();
  }

 private:
  void OnDragControllerSet(TabDragController* controller) {
    controller->SetDragLoopDoneCallbackForTesting(base::BindOnce(
        &QuitTabDraggingObserver::Quit, weak_ptr_factory_.GetWeakPtr()));
  }

  void Quit() { run_loop_.QuitWhenIdle(); }

  base::RunLoop run_loop_;
  base::OneShotTimer timeout_warning_timer_;

  base::WeakPtrFactory<QuitTabDraggingObserver> weak_ptr_factory_{this};
};

class MultiContentsViewTabDragEntrypointsUiTest
    : public SplitTabsInteractiveTestMixin<
          TabStripInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  using DragStep = base::OnceCallback<void(base::OnceClosure)>;

  // Moves the mouse to the tab header for the given index, then presses the
  // mouse button down.
  void SelectTabAt(int index) {
    EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
        ui_test_utils::GetCenterInScreenCoordinates(
            GetBrowserView().tabstrip()->tab_at(index))));
    EXPECT_TRUE(ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                   ui_controls::DOWN));
  }

  BrowserView& GetBrowserView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    EXPECT_TRUE(browser_view != nullptr);
    return *browser_view;
  }

  // Waits for two browser windows to exist, then runs a callback.
  DragStep WaitForDetachedWindow() {
    return base::BindOnce([](base::OnceClosure callback) {
      Poll(base::BindRepeating(
               []() { return BrowserList::GetInstance()->size() == 2u; }),
           std::move(callback));
    });
  }

  // Returns a `DragStep` that waits for the multi contents drop target to be
  // shown.
  DragStep WaitForDropTargetShowing() {
    return WaitForDropTargetVisibility(true);
  }

  // Returns a `DragStep` that waits for the multi contents drop target to be
  // hidden.
  DragStep WaitForDropTargetHidden() {
    return WaitForDropTargetVisibility(false);
  }

  // Returns a `DragStep` that moves the mouse to a point.
  DragStep MoveMouse(gfx::Point point) {
    return base::BindOnce(
        [](gfx::Point point, base::OnceClosure callback) {
          ui_controls::SendMouseMoveNotifyWhenDone(point.x(), point.y(),
                                                   std::move(callback));
        },
        point);
  }

  // Returns a `DragStep` that releases the left mouse button.
  DragStep ReleaseMouse() {
    return base::BindOnce([](base::OnceClosure callback) {
      ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);
      std::move(callback).Run();
    });
  }

  // Performs a check on the `MultiContentsDropTargetView` to ensure it is
  // showing on the correct side of the browser view.
  DragStep CheckDropSide(MultiContentsDropTargetView::DropSide side) {
    return base::BindOnce(
        [](BrowserView& browser_view,
           MultiContentsDropTargetView::DropSide side,
           base::OnceClosure callback) {
          EXPECT_EQ(GetDropTargetView(browser_view)->side(), side);
          std::move(callback).Run();
        },
        std::ref(GetBrowserView()), side);
  }

 private:
  DragStep WaitForDropTargetVisibility(bool visible) {
    return base::BindOnce(
        [](BrowserView& browser_view, bool visible,
           base::OnceClosure callback) {
          Poll(base::BindRepeating(
                   [](BrowserView& browser_view, bool visible) {
                     return GetDropTargetView(browser_view)->GetVisible() ==
                            visible;
                   },
                   std::ref(browser_view), visible),
               std::move(callback));
        },
        std::ref(GetBrowserView()), visible);
  }
};

class MultiContentsViewTabDragEntrypointsUiParamTest
    : public MultiContentsViewTabDragEntrypointsUiTest,
      public testing::WithParamInterface<
          MultiContentsDropTargetView::DropSide> {
 public:
  MultiContentsViewTabDragEntrypointsUiParamTest() = default;
  ~MultiContentsViewTabDragEntrypointsUiParamTest() override = default;

  gfx::Point GetPointForDropSide(MultiContentsDropTargetView::DropSide side) {
    const gfx::Rect bounds = GetBrowserView().GetBoundsInScreen();
    switch (side) {
      case MultiContentsDropTargetView::DropSide::START:
        return gfx::Point(bounds.left_center().x() + 10,
                          bounds.left_center().y());
      case MultiContentsDropTargetView::DropSide::END:
        return gfx::Point(bounds.right_center().x() - 10,
                          bounds.right_center().y());
    }
  }
};

IN_PROC_BROWSER_TEST_P(MultiContentsViewTabDragEntrypointsUiParamTest,
                       DragAndDrop) {
  SKIP_FOR_WAYLAND();

  BrowserView& browser_view = GetBrowserView();
  TabStrip* tab_strip = browser_view.tabstrip();
  const auto drop_side = GetParam();

  QuitTabDraggingObserver observer(tab_strip);
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2), Do([&]() {
        SelectTabAt(1);
        DragSequence(
            MoveMouse(
                ui_test_utils::GetCenterInScreenCoordinates(&browser_view)),
            WaitForDetachedWindow(), MoveMouse(GetPointForDropSide(drop_side)),
            WaitForDropTargetShowing(), CheckDropSide(drop_side),
            ReleaseMouse());
        observer.Wait();
      }),
      CheckResult(
          [this]() {
            return browser()->tab_strip_model()->GetActiveTab()->IsSplit();
          },
          true));
}

IN_PROC_BROWSER_TEST_P(MultiContentsViewTabDragEntrypointsUiParamTest,
                       ShowAndHideDropTarget) {
  SKIP_FOR_WAYLAND();

  BrowserView& browser_view = GetBrowserView();
  TabStrip* tab_strip = browser_view.tabstrip();
  const auto drop_side = GetParam();

  QuitTabDraggingObserver observer(tab_strip);
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 1),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUISettingsURL), 2),
      WaitForActiveTabChange(2), Do([&]() {
        SelectTabAt(1);
        DragSequence(
            MoveMouse(
                ui_test_utils::GetCenterInScreenCoordinates(&browser_view)),
            WaitForDetachedWindow(), MoveMouse(GetPointForDropSide(drop_side)),
            WaitForDropTargetShowing(), CheckDropSide(drop_side),
            MoveMouse(
                ui_test_utils::GetCenterInScreenCoordinates(&browser_view)),
            WaitForDropTargetHidden(), ReleaseMouse());
        observer.Wait();
      }));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiContentsViewTabDragEntrypointsUiParamTest,
    ::testing::Values(MultiContentsDropTargetView::DropSide::START,
                      MultiContentsDropTargetView::DropSide::END));
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)

}  // namespace
