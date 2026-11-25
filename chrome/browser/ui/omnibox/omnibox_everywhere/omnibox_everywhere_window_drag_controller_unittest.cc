// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_window_drag_controller.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#endif

namespace omnibox_everywhere {
namespace {

using blink::WebInputEvent;
using blink::WebMouseEvent;

struct FakeDelegate : public OmniboxEverywhereWindowDragController::Delegate {
  views::Widget* GetWindowDragWidget() override { return widget; }
  bool IsPointInDraggableRegion(const gfx::Point& point) const override {
    return draggable_rect.Contains(point);
  }
  bool CanStartWindowDrag() const override { return can_start; }
  bool IsWindowDragging() const override { return is_dragging; }
  void OnWindowDragStarted() override { is_dragging = true; }
  void OnWindowDragEnded() override { is_dragging = false; }
  void ForwardMouseEventToRenderer(const WebMouseEvent& event) override {
    // Pass a copy back through the controller, as the real delegate does.
    const WebMouseEvent copy = event;
    if (!controller->PreHandleMouseEvent(copy)) {
      forwarded.push_back(copy);
    }
  }

  raw_ptr<OmniboxEverywhereWindowDragController> controller = nullptr;
  std::vector<WebMouseEvent> forwarded;
  raw_ptr<views::Widget> widget = nullptr;
  gfx::Rect draggable_rect{0, 0, 100, 100};
  bool can_start = true;
  bool is_dragging = false;
};

class OmniboxEverywhereWindowDragControllerTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(gfx::Rect(200, 200, 400, 300));
    delegate_.widget = widget_.get();
    delegate_.controller = &controller_;
  }

  void TearDown() override {
    controller_.Reset();
    delegate_.widget = nullptr;
    delegate_.controller = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  // Sends `type` at `point_in_widget`. Returns whether it was consumed.
  bool Send(WebInputEvent::Type type,
            const gfx::Point& point_in_widget,
            int modifiers,
            WebMouseEvent::Button button = WebMouseEvent::Button::kNoButton) {
    WebMouseEvent event(type, modifiers, base::TimeTicks::Now());
    event.button = button;
    event.SetPositionInWidget(point_in_widget.x(), point_in_widget.y());
    const gfx::Point origin = widget_->GetWindowBoundsInScreen().origin();
    event.SetPositionInScreen(origin.x() + point_in_widget.x(),
                              origin.y() + point_in_widget.y());
    return controller_.PreHandleMouseEvent(event);
  }

  bool Press(const gfx::Point& point, int extra_modifiers = 0) {
    return Send(WebInputEvent::Type::kMouseDown, point,
                WebInputEvent::kLeftButtonDown | extra_modifiers,
                WebMouseEvent::Button::kLeft);
  }
  bool MoveHeld(const gfx::Point& point) {
    return Send(WebInputEvent::Type::kMouseMove, point,
                WebInputEvent::kLeftButtonDown, WebMouseEvent::Button::kLeft);
  }
  bool Release(const gfx::Point& point) {
    return Send(WebInputEvent::Type::kMouseUp, point,
                WebInputEvent::kNoModifiers, WebMouseEvent::Button::kLeft);
  }

  static constexpr gfx::Point kDraggable{10, 10};
  static constexpr gfx::Point kNonDraggable{200, 200};

  FakeDelegate delegate_;
  OmniboxEverywhereWindowDragController controller_{delegate_};
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(OmniboxEverywhereWindowDragControllerTest, ClickSendsHeldPress) {
  // The press is held until it's known to be a click or a drag.
  EXPECT_TRUE(Press(kDraggable));
  EXPECT_TRUE(controller_.has_pending_drag_for_testing());

  // Moving below the drag threshold doesn't start a drag, and is hidden from
  // the renderer since it hasn't seen the press.
  EXPECT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(1, 0)));
  EXPECT_TRUE(controller_.has_pending_drag_for_testing());
  EXPECT_FALSE(delegate_.is_dragging);
  EXPECT_TRUE(delegate_.forwarded.empty());

  // Releasing sends the held press, then lets the release through.
  EXPECT_FALSE(Release(kDraggable));
  ASSERT_EQ(1u, delegate_.forwarded.size());
  EXPECT_EQ(WebInputEvent::Type::kMouseDown, delegate_.forwarded[0].GetType());
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       OtherButtonPressSendsHeldPress) {
  EXPECT_TRUE(Press(kDraggable));
  // Pressing another button means it isn't a drag, so the held press is sent
  // before the other press goes through.
  EXPECT_FALSE(
      Send(WebInputEvent::Type::kMouseDown, kDraggable,
           WebInputEvent::kLeftButtonDown | WebInputEvent::kRightButtonDown,
           WebMouseEvent::Button::kRight));
  ASSERT_EQ(1u, delegate_.forwarded.size());
  EXPECT_EQ(WebMouseEvent::Button::kLeft, delegate_.forwarded[0].button);
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
  // The left release then goes through too.
  EXPECT_FALSE(Release(kDraggable));
}

TEST_F(OmniboxEverywhereWindowDragControllerTest, PressesThatDoNotArm) {
  EXPECT_FALSE(Press(kNonDraggable));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());

  EXPECT_FALSE(Send(WebInputEvent::Type::kMouseDown, kDraggable,
                    WebInputEvent::kRightButtonDown,
                    WebMouseEvent::Button::kRight));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());

#if BUILDFLAG(IS_MAC)
  // Ctrl+click is a secondary click on macOS.
  EXPECT_FALSE(Press(kDraggable, WebInputEvent::kControlKey));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
#endif

  delegate_.can_start = false;
  EXPECT_FALSE(Press(kDraggable));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       MissedReleaseEndsPendingDrag) {
  Press(kDraggable);
  // A move with no button down means the release was missed.
  EXPECT_FALSE(Send(WebInputEvent::Type::kMouseMove, kDraggable,
                    WebInputEvent::kNoModifiers));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
  // The press is dropped, not sent.
  EXPECT_FALSE(Release(kDraggable));
  EXPECT_TRUE(delegate_.forwarded.empty());
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       LeftDragWithoutButtonStateKeepsPendingDrag) {
  Press(kDraggable);
  // On macOS, a left-drag move handled after the button was lifted has no
  // button-down state. It isn't a missed release.
  EXPECT_TRUE(Send(WebInputEvent::Type::kMouseMove,
                   kDraggable + gfx::Vector2d(1, 0),
                   WebInputEvent::kNoModifiers, WebMouseEvent::Button::kLeft));
  EXPECT_TRUE(controller_.has_pending_drag_for_testing());
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       ThresholdStartsDragAndDropsMoves) {
  Press(kDraggable);
  EXPECT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(50, 0)));
  EXPECT_FALSE(controller_.has_pending_drag_for_testing());
  EXPECT_TRUE(delegate_.is_dragging);

  // Moves are dropped while the drag is in progress.
  EXPECT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(60, 0)));

  // Resetting before the posted move loop runs cancels it and ends the drag.
  controller_.Reset();
  EXPECT_FALSE(delegate_.is_dragging);
  // The press is never sent to the renderer.
  EXPECT_TRUE(delegate_.forwarded.empty());
}

TEST_F(OmniboxEverywhereWindowDragControllerTest, DragHidesRelease) {
  Press(kDraggable);
  ASSERT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(50, 0)));
  // A release seen while dragging is hidden, since the renderer never saw the
  // press.
  EXPECT_TRUE(Release(kDraggable + gfx::Vector2d(50, 0)));
  EXPECT_FALSE(delegate_.is_dragging);
  EXPECT_TRUE(delegate_.forwarded.empty());

  // Later releases go through.
  EXPECT_FALSE(Release(kDraggable));
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       OtherButtonsHiddenWhileDragging) {
  Press(kDraggable);
  ASSERT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(50, 0)));

  // A right press during the drag doesn't reach the renderer or end the drag.
  EXPECT_TRUE(
      Send(WebInputEvent::Type::kMouseDown, kDraggable,
           WebInputEvent::kLeftButtonDown | WebInputEvent::kRightButtonDown,
           WebMouseEvent::Button::kRight));
  EXPECT_TRUE(Send(WebInputEvent::Type::kMouseUp, kDraggable,
                   WebInputEvent::kLeftButtonDown,
                   WebMouseEvent::Button::kRight));
  EXPECT_TRUE(delegate_.is_dragging);
}

TEST_F(OmniboxEverywhereWindowDragControllerTest,
       PostedMoveLoopRechecksPreconditions) {
  Press(kDraggable);
  ASSERT_TRUE(MoveHeld(kDraggable + gfx::Vector2d(50, 0)));
  ASSERT_TRUE(delegate_.is_dragging);

  // UI that blocks dragging opened before the posted move loop ran, so the
  // loop must not start, and the drag ends.
  delegate_.can_start = false;
  EXPECT_TRUE(base::test::RunUntil([&] { return !delegate_.is_dragging; }));
}

#if BUILDFLAG(IS_WIN)
// Ensure dragging suppresses data drags from the renderer side
// (crbug.com/576129731).
TEST_F(OmniboxEverywhereWindowDragControllerTest,
       PendingDragSuppressesRendererDataDrags) {
  auto is_suppressed = [] {
    return views::DesktopDragDropClientWin::
        IsDragInProgressOrSuppressedForTesting();
  };
  EXPECT_FALSE(is_suppressed());

  EXPECT_TRUE(Press(kDraggable));
  EXPECT_TRUE(is_suppressed());
  MoveHeld(kDraggable + gfx::Vector2d(1, 0));
  EXPECT_TRUE(is_suppressed());
  Release(kDraggable);
  EXPECT_FALSE(is_suppressed());

  // A second press replaces, rather than stacks, the previous suppression.
  Press(kDraggable);
  Press(kDraggable);
  EXPECT_TRUE(is_suppressed());
  Release(kDraggable);
  EXPECT_FALSE(is_suppressed());

  // Presses that don't arm a drag never suppress data drags.
  Press(kNonDraggable);
  EXPECT_FALSE(is_suppressed());

  Press(kDraggable);
  controller_.Reset();
  EXPECT_FALSE(is_suppressed());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace omnibox_everywhere
