// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/touch.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_positioning_utils.h"
#include "components/exo/buffer.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_file_helper.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

using TouchTest = test::ExoTestBase;

class MockTouchDelegate : public TouchDelegate {
 public:
  MockTouchDelegate() {}

  // Overridden from TouchDelegate:
  MOCK_METHOD1(OnTouchDestroying, void(Touch*));
  MOCK_CONST_METHOD1(CanAcceptTouchEventsForSurface, bool(Surface*));
  MOCK_METHOD4(OnTouchDown,
               void(Surface*, base::TimeTicks, int, const gfx::PointF&));
  MOCK_METHOD2(OnTouchUp, void(base::TimeTicks, int));
  MOCK_METHOD3(OnTouchMotion, void(base::TimeTicks, int, const gfx::PointF&));
  MOCK_METHOD3(OnTouchShape, void(int, float, float));
  MOCK_METHOD0(OnTouchFrame, void());
  MOCK_METHOD0(OnTouchCancel, void());
};

class MockTouchStylusDelegate : public TouchStylusDelegate {
 public:
  MockTouchStylusDelegate() {}

  // Overridden from TouchStylusDelegate:
  MOCK_METHOD1(OnTouchDestroying, void(Touch*));
  MOCK_METHOD2(OnTouchTool, void(int, ui::EventPointerType));
  MOCK_METHOD3(OnTouchForce, void(base::TimeTicks, int, float));
  MOCK_METHOD3(OnTouchTilt, void(base::TimeTicks, int, const gfx::Vector2dF&));
};

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() {}

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override {}
  void OnTarget(const base::Optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {}
  void OnCancelled() override {}
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestDataSourceDelegate);
};

TEST_F(TouchTest, OnTouchDown) {
  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  auto bottom_window = exo_test_helper()->CreateWindow(10, 10, false);
  auto top_window = exo_test_helper()->CreateWindow(8, 8, false);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(top_window.surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(top_window.surface(), testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.set_current_screen_location(top_window.origin());
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(bottom_window.surface()))
      .WillRepeatedly(testing::Return(true));

  // Second touch point should be relative to the focus surface.
  EXPECT_CALL(delegate, OnTouchDown(top_window.surface(), testing::_, 2,
                                    gfx::PointF(-1, -1)));
  EXPECT_CALL(delegate, OnTouchFrame());

  generator.set_current_screen_location(bottom_window.origin());
  generator.PressTouchId(2);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchUp) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                    gfx::PointF()))
      .Times(2);
  EXPECT_CALL(delegate, OnTouchFrame()).Times(2);
  generator.set_current_screen_location(window.origin());
  generator.PressTouchId(1);
  generator.PressTouchId(2);

  EXPECT_CALL(delegate, OnTouchUp(testing::_, 1));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(1);
  EXPECT_CALL(delegate, OnTouchUp(testing::_, 2));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(2);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchMotion) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                    gfx::PointF()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::PointF(5, 5)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  EXPECT_CALL(delegate, OnTouchFrame()).Times(3);
  generator.set_current_screen_location(window.origin());
  generator.PressMoveAndReleaseTouchBy(5, 5);

  // Check if touch point motion outside focus surface is reported properly to
  // the focus surface.
  EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                    gfx::PointF()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::PointF(100, 100)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  EXPECT_CALL(delegate, OnTouchFrame()).Times(3);
  generator.set_current_screen_location(window.origin());
  generator.PressMoveAndReleaseTouchBy(100, 100);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchShape) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(delegate, OnTouchShape(testing::_, 20, 10));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(5, 5)));
    EXPECT_CALL(delegate, OnTouchShape(testing::_, 20, 10));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(10, 10)));
    EXPECT_CALL(delegate, OnTouchShape(testing::_, 20, 20));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(window.origin());
  generator.SetTouchRadius(10, 5);
  generator.PressTouch();
  generator.MoveTouchBy(5, 5);
  generator.SetTouchRadius(10, 0);  // Minor not supported
  generator.MoveTouchBy(5, 5);
  generator.ReleaseTouch();
  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancel) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                    gfx::PointF()))
      .Times(2);
  EXPECT_CALL(delegate, OnTouchFrame()).Times(2);
  generator.set_current_screen_location(window.origin());
  generator.PressTouchId(1);
  generator.PressTouchId(2);

  // One touch point being canceled is enough for OnTouchCancel to be called.
  EXPECT_CALL(delegate, OnTouchCancel());
  EXPECT_CALL(delegate, OnTouchFrame());
  ui::TouchEvent cancel_event(
      ui::ET_TOUCH_CANCELLED, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&cancel_event);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancelWhenSurfaceDestroying) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point origin = surface->window()->GetBoundsInScreen().origin();
  generator.set_current_screen_location(origin);
  generator.PressTouchId(1);

  // Since there is an active touch pointer on the surface, destroying the
  // surface should cancel touches.
  EXPECT_CALL(delegate, OnTouchCancel());
  EXPECT_CALL(delegate, OnTouchFrame());
  surface.reset();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancelNotTriggeredAfterTouchReleased) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point origin = surface->window()->GetBoundsInScreen().origin();
  generator.set_current_screen_location(origin);
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, OnTouchUp(testing::_, 1));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(1);

  // Since the surface no longer has any active touch pointers, destroying the
  // surface should not cancel any touches.
  EXPECT_CALL(delegate, OnTouchCancel()).Times(0);
  surface.reset();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, IgnoreTouchEventDuringModal) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);
  auto modal = exo_test_helper()->CreateWindow(5, 5, true);

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Make the window modal.
  modal.shell_surface()->SetSystemModal(true);

  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(modal.surface()))
      .WillRepeatedly(testing::Return(true));

  // Check if touch events on modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(modal.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(modal.origin());
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Check if touch events on non-modal window are ignored.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()))
        .Times(0);
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)))
        .Times(0);
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_)).Times(0);
    EXPECT_CALL(delegate, OnTouchFrame()).Times(0);
  }
  generator.set_current_screen_location(window.origin());
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Make the window non-modal.
  modal.shell_surface()->SetSystemModal(false);
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());

  // Check if touch events on non-modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(window.origin());
  generator.PressMoveAndReleaseTouchBy(1, 1);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchTool) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(window.origin());
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchForce) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(stylus_delegate, OnTouchForce(testing::_, 0, 1.0));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(window.origin());
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.SetTouchForce(1.0);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchTilt) {
  auto window = exo_test_helper()->CreateWindow(10, 10, false);

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(window.surface()))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(window.surface(), testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(stylus_delegate,
                OnTouchTilt(testing::_, 0, gfx::Vector2dF(1.0, 2.0)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(window.origin());
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.SetTouchTilt(1.0, 2.0);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, DragDropAbort) {
  Seat seat;
  MockTouchDelegate touch_delegate;
  std::unique_ptr<Touch> touch(new Touch(&touch_delegate, &seat));
  TestDataSourceDelegate data_source_delegate;
  DataSource source(&data_source_delegate);
  Surface origin, icon;
  TestFileHelper file_helper;

  // Make origin into a real window so the touch can click it
  ShellSurface shell_surface(&origin);
  Buffer buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(10, 10)));
  origin.Attach(&buffer);
  origin.Commit();

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(touch_delegate, CanAcceptTouchEventsForSurface(&origin))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(touch_delegate, OnTouchFrame()).Times(2);
  generator.MoveTouch(origin.window()->GetBoundsInScreen().origin());

  seat.StartDrag(&file_helper, &source, &origin, &icon,
                 ui::mojom::DragEventSource::kMouse);
  EXPECT_TRUE(seat.get_drag_drop_operation_for_testing());

  EXPECT_CALL(touch_delegate, OnTouchDown).Times(1);
  EXPECT_CALL(touch_delegate, OnTouchUp).Times(1);
  EXPECT_CALL(touch_delegate, OnTouchShape).Times(1);
  generator.PressTouch();
  EXPECT_TRUE(seat.get_drag_drop_operation_for_testing());
  generator.ReleaseTouch();
  EXPECT_FALSE(seat.get_drag_drop_operation_for_testing());

  EXPECT_CALL(touch_delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, TouchMultipleSurfaces) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  auto child_surface = std::make_unique<Surface>();
  auto child_shell_surface = std::make_unique<ShellSurface>(
      child_surface.get(), gfx::Point(), true, false,
      ash::desks_util::GetActiveDeskContainerId());
  child_shell_surface->DisableMovement();
  child_shell_surface->SetParent(shell_surface.get());
  gfx::Size child_buffer_size(15, 15);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Touch down on the two surfaces.
  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point origin = surface->window()->GetBoundsInScreen().origin();
  generator.set_current_screen_location(origin);
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(child_surface.get(), testing::_, 2, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point child_origin =
      child_surface->window()->GetBoundsInScreen().origin();
  generator.set_current_screen_location(child_origin);
  generator.PressTouchId(2);

  // Move the two touch pointers.
  for (int i = 1; i <= 10; i++) {
    EXPECT_CALL(delegate, OnTouchMotion(testing::_, 1, gfx::PointF(i, i)));
    EXPECT_CALL(delegate, OnTouchFrame());
    generator.MoveTouchId(origin + gfx::Vector2d(i, i), 1);

    EXPECT_CALL(delegate, OnTouchMotion(testing::_, 2, gfx::PointF(i, i)));
    EXPECT_CALL(delegate, OnTouchFrame());
    generator.MoveTouchId(child_origin + gfx::Vector2d(i, i), 2);
  }

  // Release the two touch pointers.
  EXPECT_CALL(delegate, OnTouchUp(testing::_, 2));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(2);

  EXPECT_CALL(delegate, OnTouchUp(testing::_, 1));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(1);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

}  // namespace
}  // namespace exo
