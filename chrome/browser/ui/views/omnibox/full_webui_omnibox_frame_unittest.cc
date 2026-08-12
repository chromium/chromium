// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/omnibox/test_location_bar.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#endif  // USE_AURA

namespace {

class FullWebUIOmniboxFrameTest : public ChromeViewsTestBase {
 public:
  FullWebUIOmniboxFrameTest() = default;
  ~FullWebUIOmniboxFrameTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  TestLocationBar location_bar_;
  std::unique_ptr<views::Widget> widget_;
};

#if defined(USE_AURA)
TEST_F(FullWebUIOmniboxFrameTest, WindowTargeterInsets) {
  auto* frame = widget_->SetContentsView(
      std::make_unique<FullWebUIOmniboxFrame>(new views::View(), &location_bar_,
                                              /*forward_mouse_events=*/false));
  widget_->Show();

  aura::Window* window = widget_->GetNativeWindow();
  ASSERT_TRUE(window);
  ASSERT_TRUE(window->targeter());

  // In normal state (forward_mouse_events = false), targeter insets should
  // match frame's shadow insets (top, left, bottom, right), so bottom shadow
  // doesn't swallow events for bookmarks bar.
  gfx::Rect mouse_rect, touch_rect;
  window->targeter()->GetHitTestRects(window, &mouse_rect, &touch_rect);
  gfx::Rect expected_rect = window->bounds();
  expected_rect.Inset(frame->GetInsets());
  EXPECT_EQ(mouse_rect, expected_rect);

  // When forwarding mouse events (e.g. during double click window), top inset
  // should additionally exclude the location bar height, while left, right, and
  // bottom shadow insets remain active.
  frame->SetForwardMouseEvents(true);
  window->targeter()->GetHitTestRects(window, &mouse_rect, &touch_rect);
  int top_inset =
      frame->GetInsets().top() +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().top() +
      GetLayoutConstant(LayoutConstant::kLocationBarHeight);
  gfx::Insets expected_forwarding_insets = gfx::Insets::TLBR(
      top_inset, frame->GetInsets().left(), frame->GetInsets().bottom(),
      frame->GetInsets().right());
  expected_rect = window->bounds();
  expected_rect.Inset(expected_forwarding_insets);
  EXPECT_EQ(mouse_rect, expected_rect);

  // When forwarding is disabled again, insets should revert to shadow insets.
  frame->SetForwardMouseEvents(false);
  window->targeter()->GetHitTestRects(window, &mouse_rect, &touch_rect);
  expected_rect = window->bounds();
  expected_rect.Inset(frame->GetInsets());
  EXPECT_EQ(mouse_rect, expected_rect);
}

TEST_F(FullWebUIOmniboxFrameTest, SetElevationUpdatesTargeter) {
  auto* frame = widget_->SetContentsView(
      std::make_unique<FullWebUIOmniboxFrame>(new views::View(), &location_bar_,
                                              /*forward_mouse_events=*/false));
  widget_->Show();

  aura::Window* window = widget_->GetNativeWindow();
  ASSERT_TRUE(window);
  ASSERT_TRUE(window->targeter());

  frame->SetElevation(0);
  gfx::Rect mouse_rect, touch_rect;
  window->targeter()->GetHitTestRects(window, &mouse_rect, &touch_rect);
  gfx::Rect expected_rect = window->bounds();
  expected_rect.Inset(frame->GetInsets());
  EXPECT_EQ(mouse_rect, expected_rect);

  frame->SetElevation(RoundedOmniboxResultsFrame::kDefaultElevation);
  window->targeter()->GetHitTestRects(window, &mouse_rect, &touch_rect);
  expected_rect = window->bounds();
  expected_rect.Inset(frame->GetInsets());
  EXPECT_EQ(mouse_rect, expected_rect);
}

TEST_F(FullWebUIOmniboxFrameTest, UpdateWindowTargeterWithoutWidget) {
  auto unattached_frame = std::make_unique<FullWebUIOmniboxFrame>(
      new views::View(), &location_bar_, /*forward_mouse_events=*/false);

  // Calling SetElevation or SetForwardMouseEvents when frame is not attached to
  // a widget invokes UpdateWindowTargeter() where GetWidget() == nullptr.
  // Verify that it returns early without crashing.
  unattached_frame->SetElevation(0);
  unattached_frame->SetForwardMouseEvents(true);
}
#else  // !defined(USE_AURA)

TEST_F(FullWebUIOmniboxFrameTest, ViewTargeterHitTesting) {
  auto* contents = new views::View();
  auto* frame = widget_->SetContentsView(
      std::make_unique<FullWebUIOmniboxFrame>(contents, &location_bar_,
                                              /*forward_mouse_events=*/false));
  widget_->SetBounds(gfx::Rect(0, 0, 500, 500));
  widget_->Show();

  int top_inset =
      frame->GetInsets().top() +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().top() +
      GetLayoutConstant(LayoutConstant::kLocationBarHeight);

  // When forwarding is disabled (normal state), points in top area target
  // child contents rather than the frame itself.
  EXPECT_NE(frame->GetEventHandlerForPoint(gfx::Point(50, top_inset - 1)),
            frame);

  // When forwarding mouse events is enabled, points in top inset area target
  // the frame itself so it can handle/forward events to the location bar.
  frame->SetForwardMouseEvents(true);
  EXPECT_EQ(frame->GetEventHandlerForPoint(gfx::Point(50, top_inset - 1)),
            frame);

  // Points below top inset target child contents even when forwarding is
  // enabled.
  EXPECT_NE(frame->GetEventHandlerForPoint(gfx::Point(50, top_inset + 1)),
            frame);

  // When forwarding is disabled again, top area reverts to targeting child
  // contents.
  frame->SetForwardMouseEvents(false);
  EXPECT_NE(frame->GetEventHandlerForPoint(gfx::Point(50, top_inset - 1)),
            frame);
}

TEST_F(FullWebUIOmniboxFrameTest, OnMouseEventMarksHandled) {
  auto* frame = widget_->SetContentsView(
      std::make_unique<FullWebUIOmniboxFrame>(new views::View(), &location_bar_,
                                              /*forward_mouse_events=*/true));
  widget_->Show();

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::PointF(10, 10),
                       gfx::PointF(10, 10), base::TimeTicks::Now(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  frame->OnMouseEvent(&event);
  EXPECT_TRUE(event.handled());

  frame->SetForwardMouseEvents(false);
  ui::MouseEvent event2(ui::EventType::kMousePressed, gfx::PointF(10, 10),
                        gfx::PointF(10, 10), base::TimeTicks::Now(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  frame->OnMouseEvent(&event2);
  EXPECT_TRUE(event2.handled());
}

#endif  // USE_AURA

}  // namespace
