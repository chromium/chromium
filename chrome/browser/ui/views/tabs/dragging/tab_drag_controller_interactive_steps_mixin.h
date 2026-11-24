// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_STEPS_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_STEPS_MIXIN_H_

#include <concepts>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller_interactive_uitest.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/range/range.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                               kTabDragBrowserCountState);
DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                               kTabDragActiveState);
DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                               kTabDragWindowLoopActiveState);
DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                               kTabDragAttachedToTargetState);
DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                               kTabDragNewBrowserActiveState);

// Mixin with Kombucha (`RunTestSequence`) steps for in-strip, detach, and
// cross-window tab drags in `TabDragControllerTest`-based tests.
template <typename T>
  requires(std::derived_from<T, TabDragControllerTest>)
class TabDragControllerInteractiveStepsMixin : public T {
 public:
  template <class... Args>
  explicit TabDragControllerInteractiveStepsMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~TabDragControllerInteractiveStepsMixin() override = default;
  TabDragControllerInteractiveStepsMixin(
      const TabDragControllerInteractiveStepsMixin&) = delete;
  TabDragControllerInteractiveStepsMixin& operator=(
      const TabDragControllerInteractiveStepsMixin&) = delete;

 protected:
  using T::Check;
  using T::CheckResult;
  using T::ClickMouse;
  using T::Do;
  using T::GetAttachedDragWindow;
  using T::IsDraggingTabsIn;
  using T::IsTabDraggingInfoCleared;
  using T::IsTabDraggingInfoSet;
  using T::IsWindowDragLoopActive;
  using T::MoveMouseTo;
  using T::NameView;
  using T::PollState;
  using T::PollStateUntil;
  using T::ReleaseMouse;
  using T::Steps;
  using T::StopAnimating;
  using T::WaitForState;
  using typename T::MultiStep;
  using typename T::RelativePositionSpecifier;
  using typename T::StepBuilder;

  // Starts the state pollers that the steps below wait on. If `attach_target`
  // is set, also tracks whether the drag has attached to `attach_target`.
  auto StartDragStatePollers(TabStrip* attach_target = nullptr) {
    auto steps = Steps(
        PollState(
            kTabDragBrowserCountState,
            []() { return GlobalBrowserCollection::GetInstance()->GetSize(); }),
        PollState(kTabDragActiveState,
                  []() { return TabDragController::IsActive(); }),
        PollState(kTabDragWindowLoopActiveState,
                  [this]() { return IsWindowDragLoopActive(); }));
    if (attach_target) {
      T::AddStep(steps, PollState(kTabDragAttachedToTargetState,
                                  [this, attach_target]() {
                                    return IsDraggingTabsIn(attach_target);
                                  }));
    }
    return steps;
  }

  // Waits until there are `count` browser windows.
  auto WaitForBrowserCount(size_t count) {
    return WaitForState(kTabDragBrowserCountState, count);
  }

  // --- Pointer actions ---
  //
  // `Press*()` presses the pointer, `Move*()` moves the pressed pointer, and
  // `ReleaseDrag()` / `CancelDragWithEsc()` end the drag. Steps named
  // `*UntilDetached()` / `*UntilAttached()` also wait for that state.

  // Presses the pointer on `tab_strip->tab_at(index)` at `pos` (center by
  // default).
  auto PressTab(TabStrip* tab_strip,
                int index,
                RelativePositionSpecifier pos = typename T::CenterPoint()) {
    static constexpr char kPressedTab[] = "PressedTab";
    return Steps(
        NameTab(kPressedTab, tab_strip, index),
        MoveMouseTo(kPressedTab, std::move(pos)).SetMustRemainVisible(false),
        ClickMouse(ui_controls::LEFT, /*release=*/false));
  }

  // Moves the pressed pointer to `tab_strip->tab_at(index)` at `pos` (center by
  // default).
  MultiStep MoveToTab(
      TabStrip* tab_strip,
      int index,
      RelativePositionSpecifier pos = typename T::CenterPoint()) {
    static constexpr char kTargetTab[] = "TargetTab";
    return Steps(
        NameTab(kTargetTab, tab_strip, index),
        MoveMouseTo(kTargetTab, std::move(pos)).SetMustRemainVisible(false));
  }
  MultiStep MoveToTab(TabStrip* tab_strip, int index, gfx::Vector2d offset) {
    return MoveToTab(tab_strip, index, CenterPointWithOffset(offset));
  }

  // Drags `tab_strip->tab_at(from)` onto `tab_strip->tab_at(to)` at `to_pos`
  // (or `+ to_offset`) and releases.
  MultiStep DragTabInStripAndRelease(
      TabStrip* tab_strip,
      int from,
      int to,
      RelativePositionSpecifier to_pos = typename T::CenterPoint()) {
    return Steps(PressTab(tab_strip, from),
                 MoveToTab(tab_strip, to, std::move(to_pos)),
                 ReleaseMouse(ui_controls::LEFT),
                 Do([this, tab_strip]() { StopAnimating(tab_strip); }));
  }
  MultiStep DragTabInStripAndRelease(TabStrip* tab_strip,
                                     int from,
                                     int to,
                                     gfx::Vector2d to_offset) {
    return DragTabInStripAndRelease(tab_strip, from, to,
                                    CenterPointWithOffset(to_offset));
  }

  // Returns a position halfway between the left edge and center of the target
  // element (matching `GetLeftCenterInScreenCoordinates`).
  static base::RepeatingCallback<gfx::Point(ui::TrackedElement*)>
  LeftCenterPoint() {
    return base::BindLambdaForTesting([](ui::TrackedElement* el) {
      const gfx::Rect bounds = el->GetScreenBounds();
      return bounds.CenterPoint() - gfx::Vector2d(bounds.width() / 4, 0);
    });
  }

  // Returns a position halfway between the center and right edge of the target
  // element (matching `GetRightCenterInScreenCoordinates`).
  static base::RepeatingCallback<gfx::Point(ui::TrackedElement*)>
  RightCenterPoint() {
    return base::BindLambdaForTesting([](ui::TrackedElement* el) {
      const gfx::Rect bounds = el->GetScreenBounds();
      return bounds.CenterPoint() + gfx::Vector2d(bounds.width() / 4, 0);
    });
  }

  // Presses the pointer on `tab_strip->group_header(group)`.
  auto PressGroupHeader(TabStrip* tab_strip, tab_groups::TabGroupId group) {
    static constexpr char kPressedGroupHeader[] = "PressedGroupHeader";
    return Steps(NameGroupHeader(kPressedGroupHeader, tab_strip, group),
                 MoveMouseTo(kPressedGroupHeader).SetMustRemainVisible(false),
                 ClickMouse(ui_controls::LEFT, /*release=*/false));
  }

  // Moves the pressed pointer straight down from
  // `tab_strip->group_header(group)` until the group detaches into a window
  // move loop.
  auto MoveGroupHeaderOutUntilDetached(TabStrip* tab_strip,
                                       tab_groups::TabGroupId group) {
    static constexpr char kDetachingGroupHeader[] = "DetachingGroupHeader";
    return Steps(
        NameGroupHeader(kDetachingGroupHeader, tab_strip, group),
        MoveMouseTo(kDetachingGroupHeader, PointBelowTabStrip(tab_strip))
            .SetMustRemainVisible(false),
        WaitForState(kTabDragWindowLoopActiveState, true));
  }

  // Moves the pressed pointer straight down from `tab_strip->tab_at(index)`
  // until the tab detaches into a window move loop.
  auto MoveTabOutUntilDetached(TabStrip* tab_strip, int index) {
    static constexpr char kDetachingTab[] = "DetachingTab";
    return Steps(NameTab(kDetachingTab, tab_strip, index),
                 MoveMouseTo(kDetachingTab, PointBelowTabStrip(tab_strip))
                     .SetMustRemainVisible(false),
                 WaitForState(kTabDragWindowLoopActiveState, true));
  }

  // Moves the pressed pointer straight down from the center of `tab_strip`
  // until the dragged tabs detach into a window move loop.
  auto MoveOutOfTabStripUntilDetached(TabStrip* tab_strip) {
    static constexpr char kDetachingTabStrip[] = "DetachingTabStrip";
    return Steps(Do([this, tab_strip]() { StopAnimating(tab_strip); }),
                 NameView(kDetachingTabStrip, tab_strip),
                 MoveMouseTo(kDetachingTabStrip, PointBelowTabStrip(tab_strip))
                     .SetMustRemainVisible(false),
                 WaitForState(kTabDragWindowLoopActiveState, true));
  }

  // Moves the dragged window until the pointer is over `target`, then waits
  // for the drag to attach to `target` and for the move loop to exit.
  //
  // `source` is the tab strip the drag came from. Its window can overlap
  // `target`'s on small bot screens, so the drop point is chosen to its right.
  // This is the original window for a single-tab drag, or the dragged window
  // itself when all of its tabs are dragged.
  // TODO(crbug.com/571898137): Fix the window overlap in
  // `CreateAnotherBrowserAndResize()` and target `target` directly.
  auto MoveOverTabStripUntilAttached(TabStrip* target, TabStrip* source) {
    static constexpr char kTargetTabStrip[] = "TargetTabStrip";
    return Steps(
        Do([this, target]() { StopAnimating(target); }),
        NameView(kTargetTabStrip, target),
        MoveMouseTo(kTargetTabStrip,
                    base::BindLambdaForTesting(
                        [target, source](ui::TrackedElement* el) {
                          if (!test::PlatformSupportsScreenCoordinates()) {
                            return el->GetScreenBounds().CenterPoint() +
                                   gfx::Vector2d(target->width() / 2 - 50, 0);
                          }
                          const gfx::Rect target_bounds =
                              target->GetBoundsInScreen();
                          const gfx::Point point = test::DragPointInTabStrip(
                              target_bounds,
                              source->GetWidget()->GetWindowBoundsInScreen(),
                              target_bounds.right() - 50);
                          EXPECT_TRUE(target_bounds.Contains(point));
                          return point;
                        }))
            .SetMustRemainVisible(false),
        WaitForState(kTabDragAttachedToTargetState, true),
        WaitForState(kTabDragWindowLoopActiveState, false),
        Do([this, target]() { StopAnimating(target); }));
  }

  // Releases the pointer and waits for the drag to end.
  //
  // On Windows the release is sent asynchronously, because a synchronous
  // release that ends a window move loop can hang or DCHECK in
  // `MessagePumpForUI`. This bypasses Kombucha's mouse tracking, so the
  // sequence must not press the mouse again afterwards.
  // TODO(crbug.com/582409410): Use `ReleaseMouse()` on all platforms once the
  // Windows nested run loop timer leak is fixed.
  auto ReleaseDrag() {
#if BUILDFLAG(IS_WIN)
    return Steps(WaitForState(kTabDragActiveState, true), Do([]() {
                   ASSERT_TRUE(ui_controls::SendMouseEvents(ui_controls::LEFT,
                                                            ui_controls::UP));
                 }),
                 WaitForState(kTabDragActiveState, false));
#else
    return Steps(WaitForState(kTabDragActiveState, true),
                 ReleaseMouse(ui_controls::LEFT),
                 WaitForState(kTabDragActiveState, false));
#endif
  }

  // Presses Escape and waits for the drag to end.
  //
  // This uses `ui_controls` rather than Kombucha's `SendKeyPress()` because
  // the key must go to whichever window currently hosts the drag, which may be
  // a temporary window created mid-drag that has no named element (matching
  // `common/tab_drag_interactive_uitest.cc`).
  auto CancelDragWithEsc() {
    return Steps(WaitForState(kTabDragActiveState, true), Do([this]() {
                   ASSERT_TRUE(TabDragController::IsActive());
                   const gfx::NativeWindow window = GetAttachedDragWindow();
                   ASSERT_TRUE(window);
                   ASSERT_TRUE(ui_controls::SendKeyPress(
                       window, ui::VKEY_ESCAPE, false, false, false, false));
                 }),
                 WaitForState(kTabDragActiveState, false));
  }

  // Waits until the browser that is not in `existing` is active.
  auto WaitForNewBrowserActive(std::set<BrowserWindowInterface*> existing) {
    return PollStateUntil(
        kTabDragNewBrowserActiveState,
        [existing]() {
          BrowserWindowInterface* const browser =
              ui_test_utils::GetBrowserNotInSet(existing);
          return browser && browser->GetWindow()->IsActive();
        },
        true);
  }

  // --- Checks ---

  // Checks that `browser` has tabs in the order `expected` (see
  // `test::IDString()`).
  static auto CheckTabOrder(BrowserWindowInterface* browser,
                            std::string expected) {
    return CheckResult(
        [browser]() { return test::IDString(browser->GetTabStripModel()); },
        expected, "CheckTabOrder()");
  }

  // Checks that `group` in `group_model` spans `expected`.
  static auto CheckTabGroupRange(TabGroupModel* group_model,
                                 tab_groups::TabGroupId group,
                                 gfx::Range expected) {
    return CheckResult(
        [group_model, group]() {
          return group_model->GetTabGroup(group)->ListTabs();
        },
        expected, "CheckTabGroupRange()");
  }

  // Waits for the drag to detach from `source` into a new, temporary window,
  // and checks that only that window has a drag session.
  auto CheckDetachedIntoNewWindow(BrowserWindowInterface* source,
                                  BrowserWindowInterface* other) {
    return Steps(WaitForState(kTabDragBrowserCountState, 3u),
                 Do([this, source, other]() {
                   TabStrip* const source_tab_strip =
                       test::GetTabStripForBrowser(source);
                   EXPECT_FALSE(test::IsDragSessionActive(source_tab_strip));
                   EXPECT_FALSE(test::IsDragSessionActive(
                       test::GetTabStripForBrowser(other)));
                   EXPECT_TRUE(TabDragController::IsActive());
                   EXPECT_TRUE(IsTabDraggingInfoCleared(source_tab_strip));

                   BrowserWindowInterface* const new_browser =
                       ui_test_utils::GetBrowserNotInSet({source, other});
                   ASSERT_TRUE(new_browser);
                   TabStrip* const new_tab_strip =
                       test::GetTabStripForBrowser(new_browser);
                   EXPECT_TRUE(test::IsDragSessionActive(new_tab_strip));
                   EXPECT_TRUE(IsTabDraggingInfoSet(new_tab_strip));
                 }));
  }

  // Checks that the drag is attached to `target` and has left `source`.
  auto CheckAttachedTo(BrowserWindowInterface* target,
                       BrowserWindowInterface* source) {
    return Do([this, target, source]() {
      TabStrip* const target_tab_strip = test::GetTabStripForBrowser(target);
      TabStrip* const source_tab_strip = test::GetTabStripForBrowser(source);
      EXPECT_TRUE(test::IsDragSessionActive(target_tab_strip));
      EXPECT_FALSE(test::IsDragSessionActive(source_tab_strip));
      EXPECT_TRUE(TabDragController::IsActive());
      EXPECT_FALSE(test::GetIsDragged(source));
      EXPECT_TRUE(IsTabDraggingInfoCleared(source_tab_strip));
      EXPECT_TRUE(IsTabDraggingInfoSet(target_tab_strip));
    });
  }

  // Checks that the tab in the window detached from `source` kept the width
  // it had before detaching. Only meaningful if both windows are the same
  // type.
  static auto CheckDetachedTabWidth(BrowserWindowInterface* source,
                                    int expected_width) {
    return Do([source, expected_width]() {
      BrowserWindowInterface* const detached =
          ui_test_utils::GetBrowserNotInSet({source});
      ASSERT_TRUE(detached);
      if (source->GetType() == detached->GetType()) {
        EXPECT_EQ(expected_width,
                  test::GetTabStripForBrowser(detached)->tab_at(0)->width());
      }
    });
  }

  // Checks that no drag is in progress, and that no browser window is still
  // marked as dragged or holding capture.
  static auto CheckDragEnded() {
    return Do([]() {
      EXPECT_FALSE(TabDragController::IsActive());
      GlobalBrowserCollection::GetInstance()->ForEach(
          [](BrowserWindowInterface* browser) {
            TabStrip* const tab_strip = test::GetTabStripForBrowser(browser);
            if (tab_strip) {
              EXPECT_FALSE(test::IsDragSessionActive(tab_strip));
              EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
            }
            EXPECT_FALSE(test::GetIsDragged(browser));
            return true;
          });
    });
  }

  // Checks that no browser window ended up maximized.
  static auto CheckNoBrowserMaximized() {
    return Do([]() {
      GlobalBrowserCollection::GetInstance()->ForEach(
          [](BrowserWindowInterface* browser) {
            EXPECT_FALSE(browser->GetWindow()->IsMaximized());
            return true;
          });
    });
  }

  // Runs `check` on the browser that isn't in `existing`.
  static StepBuilder CheckNewBrowser(
      std::set<BrowserWindowInterface*> existing,
      base::OnceCallback<void(BrowserWindowInterface*)> check) {
    return Do(base::BindOnce(
        [](const std::set<BrowserWindowInterface*>& existing,
           base::OnceCallback<void(BrowserWindowInterface*)> check) {
          BrowserWindowInterface* const new_browser =
              ui_test_utils::GetBrowserNotInSet(existing);
          ASSERT_TRUE(new_browser);
          std::move(check).Run(new_browser);
        },
        std::move(existing), std::move(check)));
  }

  // Runs `check` on the browser that isn't `browser()`.
  StepBuilder CheckNewBrowser(
      base::OnceCallback<void(BrowserWindowInterface*)> check) {
    return CheckNewBrowser({T::browser()}, std::move(check));
  }

  // Drags `source`'s tab at `index` out into a new window and releases it.
  // Also checks that the tab kept its width.
  MultiStep DragTabOutAndRelease(BrowserWindowInterface* source, int index) {
    TabStrip* const tab_strip = test::GetTabStripForBrowser(source);
    return Steps(
        PressTab(tab_strip, index), MoveTabOutUntilDetached(tab_strip, index),
        CheckDetachedTabWidth(source, tab_strip->tab_at(index)->width()),
        ReleaseDrag());
  }

 private:
  // Names `tab_strip->tab_at(index)` as `name` once animations have settled.
  // `index` is resolved when the step runs, not when the sequence is built.
  MultiStep NameTab(std::string_view name, TabStrip* tab_strip, int index) {
    return Steps(
        Do([this, tab_strip]() { StopAnimating(tab_strip); }),
        NameView(name, base::BindLambdaForTesting([tab_strip, index]() {
                   return static_cast<views::View*>(tab_strip->tab_at(index));
                 })));
  }

  // Names `tab_strip->group_header(group)` as `name` once animations have
  // settled.
  MultiStep NameGroupHeader(std::string_view name,
                            TabStrip* tab_strip,
                            tab_groups::TabGroupId group) {
    return Steps(
        Do([this, tab_strip]() { StopAnimating(tab_strip); }),
        NameView(name, base::BindLambdaForTesting([tab_strip, group]() {
                   return static_cast<views::View*>(
                       tab_strip->group_header(group));
                 })));
  }

  // Returns a position at the center of the target element plus `offset`.
  static base::RepeatingCallback<gfx::Point(ui::TrackedElement*)>
  CenterPointWithOffset(gfx::Vector2d offset) {
    return base::BindLambdaForTesting([offset](ui::TrackedElement* el) {
      return el->GetScreenBounds().CenterPoint() + offset;
    });
  }

  // Returns a position directly below the center of the target element, far
  // enough down to detach from `tab_strip`.
  static base::RepeatingCallback<gfx::Point(ui::TrackedElement*)>
  PointBelowTabStrip(TabStrip* tab_strip) {
    return base::BindLambdaForTesting([tab_strip](ui::TrackedElement* el) {
      return el->GetScreenBounds().CenterPoint() +
             gfx::Vector2d(0, test::GetDetachY(tab_strip));
    });
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_STEPS_MIXIN_H_
