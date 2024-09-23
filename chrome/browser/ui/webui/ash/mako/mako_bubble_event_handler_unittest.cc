// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_event_handler.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

using testing::_;
using testing::VariantWith;

class DelegateForTest : public ash::MakoBubbleEventHandler::Delegate {
 public:
  virtual ~DelegateForTest() = default;

  const std::optional<SkRegion> GetDraggableRegion() override {
    SkRegion draggable_region;
    draggable_region.op(SkIRect::MakeLTRB(100, 100, 200, 200),
                        SkRegion::kUnion_Op);
    return draggable_region;
  }

  const gfx::Rect GetWidgetBoundsInScreen() override {
    return gfx::Rect(
        /*x=*/50,
        /*y=*/50,
        /*width=*/300,
        /*height=*/300);
  }

  void SetWidgetBoundsConstrained(const gfx::Rect bounds) override {
    widget_bounds_updated = true;
    widget_bounds = bounds;
  }

  void SetCursor(const ui::Cursor& cursor) override {}

  bool IsDraggingEnabled() override { return true; }

  bool IsResizingEnabled() override { return true; }

  bool widget_bounds_updated = false;
  gfx::Rect widget_bounds;
};

enum EventLocation {
  DRAGGABLE_REGION,
  TOP,
  BOTTOM,
  LEFT,
  RIGHT,
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT,
  OTHER
};

gfx::PointF MakeLocation(EventLocation location) {
  switch (location) {
    case EventLocation::DRAGGABLE_REGION:
      return gfx::PointF(150, 150);
    case EventLocation::TOP:
      return gfx::PointF(150, 2);
    case EventLocation::BOTTOM:
      return gfx::PointF(150, 298);
    case EventLocation::LEFT:
      return gfx::PointF(2, 150);
    case EventLocation::RIGHT:
      return gfx::PointF(298, 150);
    case EventLocation::TOP_LEFT:
      return gfx::PointF(2, 2);
    case EventLocation::TOP_RIGHT:
      return gfx::PointF(298, 2);
    case EventLocation::BOTTOM_LEFT:
      return gfx::PointF(2, 298);
    case EventLocation::BOTTOM_RIGHT:
      return gfx::PointF(298, 298);
    case EventLocation::OTHER:
      return gfx::PointF(50, 50);
    default:
      return gfx::PointF(0, 0);
  }
}

std::unique_ptr<ui::TouchEvent> MakeTouchEvent(ui::EventType type,
                                               EventLocation location) {
  return std::make_unique<ui::TouchEvent>(
      /*type=*/type,
      /*location=*/
      MakeLocation(location),
      /*root_location=*/gfx::PointF(0, 0),
      /*time_stamp=*/base::TimeTicks::Now(),
      /*pointer_details=*/ui::PointerDetails());
}

std::unique_ptr<ui::MouseEvent> MakeMouseEvent(ui::EventType type,
                                               EventLocation location) {
  return std::make_unique<ui::MouseEvent>(
      /*type=*/type,
      /*location=*/
      MakeLocation(location),
      /*root_location=*/gfx::PointF(0, 0),
      /*time_stamp=*/base::TimeTicks::Now(),
      /*flag=*/0,
      /*changed_button_flags=*/0,
      /*pointer_details=*/ui::PointerDetails());
}

class MakoBubbleEventHandlerTest : public ChromeViewsTestBase {
 public:
  MakoBubbleEventHandlerTest() : handler_(&delegate_) {}

 protected:
  DelegateForTest delegate_;
  ash::MakoBubbleEventHandler handler_;
};

TEST_F(MakoBubbleEventHandlerTest, TouchPressedEventStartsDragging) {
  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::EventType::kTouchPressed,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::DraggingState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, TouchPressedEventIsIgnored) {
  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::EventType::kTouchPressed,
      /*location=*/EventLocation::OTHER);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MousePressedEventStartsDragging) {
  std::unique_ptr<ui::MouseEvent> event = MakeMouseEvent(
      /*type=*/ui::EventType::kMousePressed,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnMouseEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::DraggingState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MousePressedEventIsIgnored) {
  std::unique_ptr<ui::MouseEvent> event = MakeMouseEvent(
      /*type=*/ui::EventType::kMousePressed,
      /*location=*/EventLocation::OTHER);

  handler_.OnMouseEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, TouchMovedEventProcceedsDragging) {
  // In this test, the widget was first positioned at (x=50,y=50,w=300,h=300).
  // Mouse drags from (140, 140) to (150, 150) and so the widget should be
  // moved to (x=60, y=60, w=300, h=300).
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{
      .original_bounds_in_screen = gfx::Rect(
          /*x=*/50,
          /*y=*/50,
          /*width=*/300,
          /*height=*/300),
      // The original pointer position is relative to screen, so it's 140 + 50.
      .original_pointer_pos = gfx::Vector2d(/*x=*/140 + 50, /*y=*/140 + 50),
  });
  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::EventType::kTouchMoved,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnTouchEvent(event.get());

  EXPECT_TRUE(delegate_.widget_bounds_updated);
  EXPECT_EQ(delegate_.widget_bounds, gfx::Rect(
                                         /*x=*/60,
                                         /*y=*/60,
                                         /*width=*/300,
                                         /*height=*/300));
}

TEST_F(MakoBubbleEventHandlerTest, TouchMovedEventIsIgnored) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::InitialState{});
  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::EventType::kTouchMoved,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnTouchEvent(event.get());

  EXPECT_FALSE(delegate_.widget_bounds_updated);
}

TEST_F(MakoBubbleEventHandlerTest, MouseDraggedEventProcceedsDragging) {
  // In this test, the widget was first positioned at (x=50,y=50,w=300,h=300).
  // Mouse drags from (140, 140) to (150, 150) and so the widget should be
  // moved to (x=60, y=60, w=300, h=300).
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{
      .original_bounds_in_screen = gfx::Rect(
          /*x=*/50,
          /*y=*/50,
          /*width=*/300,
          /*height=*/300),
      // The original pointer position is relative to screen, so it's 140 + 50.
      .original_pointer_pos = gfx::Vector2d(/*x=*/140 + 50, /*y=*/140 + 50)});
  std::unique_ptr<ui::MouseEvent> event = MakeMouseEvent(
      /*type=*/ui::EventType::kMouseDragged,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnMouseEvent(event.get());

  EXPECT_TRUE(delegate_.widget_bounds_updated);
  EXPECT_EQ(delegate_.widget_bounds, gfx::Rect(
                                         /*x=*/60,
                                         /*y=*/60,
                                         /*width=*/300,
                                         /*height=*/300));
}

TEST_F(MakoBubbleEventHandlerTest, MouseDraggedEventIsIgnored) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::InitialState{});
  std::unique_ptr<ui::MouseEvent> event = MakeMouseEvent(
      /*type=*/ui::EventType::kMouseDragged,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnMouseEvent(event.get());

  EXPECT_FALSE(delegate_.widget_bounds_updated);
}

TEST_F(MakoBubbleEventHandlerTest, TouchReleasedEventStopsDragging) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{});

  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::EventType::kTouchReleased,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MouseReleasedEventStopsDragging) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{});

  std::unique_ptr<ui::MouseEvent> event = MakeMouseEvent(
      /*type=*/ui::EventType::kMouseReleased,
      /*location=*/EventLocation::DRAGGABLE_REGION);

  handler_.OnMouseEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

}  // namespace
}  // namespace ash
