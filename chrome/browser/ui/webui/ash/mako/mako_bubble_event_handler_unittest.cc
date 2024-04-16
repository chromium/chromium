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

  bool widget_bounds_updated = false;
  gfx::Rect widget_bounds;
};

std::unique_ptr<ui::TouchEvent> MakeTouchEvent(ui::EventType type,
                                               bool in_draggable_region) {
  return std::make_unique<ui::TouchEvent>(
      /*type=*/type,
      /*location=*/
      in_draggable_region ? gfx::PointF(150, 150) : gfx::PointF(0, 0),
      /*root_location=*/gfx::PointF(0, 0),
      /*time_stamp=*/base::TimeTicks::Now(),
      /*pointer_details=*/ui::PointerDetails());
}

std::unique_ptr<ui::MouseEvent> makeMouseEvent(ui::EventType type,
                                               bool in_draggable_region) {
  return std::make_unique<ui::MouseEvent>(
      /*type=*/type,
      /*location=*/
      in_draggable_region ? gfx::PointF(150, 150) : gfx::PointF(0, 0),
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
      /*type=*/ui::ET_TOUCH_PRESSED,
      /*in_draggable_region=*/true);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::DraggingState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, TouchPressedEventIsIgnored) {
  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::ET_TOUCH_PRESSED,
      /*in_draggable_region=*/false);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MousePressedEventStartsDragging) {
  std::unique_ptr<ui::MouseEvent> event = makeMouseEvent(
      /*type=*/ui::ET_MOUSE_PRESSED,
      /*in_draggable_region=*/true);

  handler_.OnMouseEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::DraggingState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MousePressedEventIsIgnored) {
  std::unique_ptr<ui::MouseEvent> event = makeMouseEvent(
      /*type=*/ui::ET_MOUSE_PRESSED,
      /*in_draggable_region=*/false);

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
      /*type=*/ui::ET_TOUCH_MOVED,
      /*in_draggable_region=*/true);

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
      /*type=*/ui::ET_TOUCH_MOVED,
      /*in_draggable_region=*/true);

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
  std::unique_ptr<ui::MouseEvent> event = makeMouseEvent(
      /*type=*/ui::ET_MOUSE_DRAGGED,
      /*in_draggable_region=*/true);

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
  std::unique_ptr<ui::MouseEvent> event = makeMouseEvent(
      /*type=*/ui::ET_MOUSE_DRAGGED,
      /*in_draggable_region=*/true);

  handler_.OnMouseEvent(event.get());

  EXPECT_FALSE(delegate_.widget_bounds_updated);
}

TEST_F(MakoBubbleEventHandlerTest, TouchReleasedEventStopsDragging) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{});

  std::unique_ptr<ui::TouchEvent> event = MakeTouchEvent(
      /*type=*/ui::ET_TOUCH_RELEASED,
      /*in_draggable_region=*/true);

  handler_.OnTouchEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

TEST_F(MakoBubbleEventHandlerTest, MouseReleasedEventStopsDragging) {
  handler_.set_state_for_testing(MakoBubbleEventHandler::DraggingState{});

  std::unique_ptr<ui::MouseEvent> event = makeMouseEvent(
      /*type=*/ui::ET_MOUSE_RELEASED,
      /*in_draggable_region=*/true);

  handler_.OnMouseEvent(event.get());

  EXPECT_THAT(handler_.get_state_for_testing(),
              VariantWith<ash::MakoBubbleEventHandler::InitialState>(_));
}

}  // namespace
}  // namespace ash
