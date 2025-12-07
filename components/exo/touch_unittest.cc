// Copyright 2015 The Chromium Authors
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
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/test_data_device_delegate.h"
#include "components/exo/test/test_data_source_delegate.h"
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
using test::TestDataSourceDelegate;

class MockTouchDelegate : public TouchDelegate {
 public:
  MockTouchDelegate() = default;

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
  MockTouchStylusDelegate() = default;

  // Overridden from TouchStylusDelegate:
  MOCK_METHOD1(OnTouchDestroying, void(Touch*));
  MOCK_METHOD2(OnTouchTool, void(int, ui::EventPointerType));
  MOCK_METHOD3(OnTouchForce, void(base::TimeTicks, int, float));
  MOCK_METHOD3(OnTouchTilt, void(base::TimeTicks, int, const gfx::Vector2dF&));
};

TEST_F(TouchTest, OnTouchDown) {
  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  auto bottom_shell_surface =
      test::ShellSurfaceBuilder({10, 10}).SetCentered().BuildShellSurface();
  auto top_shell_surface =
      test::ShellSurfaceBuilder({8, 8}).SetCentered().BuildShellSurface();

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate,
              CanAcceptTouchEventsForSurface(top_shell_surface->root_surface()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnTouchDown(top_shell_surface->root_surface(),
                                    testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.set_current_screen_location(
      GetOriginOfShellSurface(top_shell_surface.get()));
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(
                            bottom_shell_surface->root_surface()))
      .WillRepeatedly(testing::Return(true));

  // Second touch point should be relative to the focus surface.
  EXPECT_CALL(delegate, OnTouchDown(top_shell_surface->root_surface(),
                                    testing::_, 2, gfx::PointF(-1, -1)));
  EXPECT_CALL(delegate, OnTouchFrame());

  generator.set_current_screen_location(
      GetOriginOfShellSurface(bottom_shell_surface.get()));
  generator.PressTouchId(2);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchUp) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, testing::_, gfx::PointF()))
      .Times(2);
  EXPECT_CALL(delegate, OnTouchFrame()).Times(2);
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
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
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, testing::_, gfx::PointF()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::PointF(5, 5)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  EXPECT_CALL(delegate, OnTouchFrame()).Times(3);
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressMoveAndReleaseTouchBy(5, 5);

  // Check if touch point motion outside focus surface is reported properly to
  // the focus surface.
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, testing::_, gfx::PointF()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::PointF(100, 100)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  EXPECT_CALL(delegate, OnTouchFrame()).Times(3);
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressMoveAndReleaseTouchBy(100, 100);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchShape) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
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
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
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
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, testing::_, gfx::PointF()))
      .Times(2);
  EXPECT_CALL(delegate, OnTouchFrame()).Times(2);
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressTouchId(1);
  generator.PressTouchId(2);

  // One touch point being canceled is enough for OnTouchCancel to be called.
  EXPECT_CALL(delegate, OnTouchCancel());
  EXPECT_CALL(delegate, OnTouchFrame());
  ui::TouchEvent cancel_event(
      ui::EventType::kTouchCancelled, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&cancel_event);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancelWhenSurfaceDestroying) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressTouchId(1);

  // Since there is an active touch pointer on the surface, destroying the
  // surface should cancel touches.
  EXPECT_CALL(delegate, OnTouchCancel());
  EXPECT_CALL(delegate, OnTouchFrame());

  test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface.get());

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancelNotTriggeredAfterTouchReleased) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, OnTouchUp(testing::_, 1));
  EXPECT_CALL(delegate, OnTouchFrame());
  generator.ReleaseTouchId(1);

  // Since the surface no longer has any active touch pointers, destroying the
  // surface should not cancel any touches.
  EXPECT_CALL(delegate, OnTouchCancel()).Times(0);
  test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface.get());

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, IgnoreTouchEventDuringModal) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10})
                           .SetCentered()
                           .SetCanMinimize(false)
                           .BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  auto modal_shell_surface = test::ShellSurfaceBuilder({5, 5})
                                 .SetCentered()
                                 .SetUseSystemModalContainer()
                                 .SetCanMinimize(false)
                                 .BuildShellSurface();
  auto* modal_root_surface = modal_shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Make the window modal.
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());

  modal_shell_surface->SetSystemModal(true);

  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(modal_root_surface))
      .WillRepeatedly(testing::Return(true));

  // Check if touch events on modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(modal_root_surface, testing::_,
                                      testing::_, gfx::PointF()));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(modal_shell_surface.get()));
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Check if touch events on non-modal window are ignored.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
                                      gfx::PointF()))
        .Times(0);
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)))
        .Times(0);
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_)).Times(0);
    EXPECT_CALL(delegate, OnTouchFrame()).Times(0);
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Make the window non-modal.
  modal_shell_surface->SetSystemModal(false);
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());

  // Check if touch events on non-modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::PointF(1, 1)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.PressMoveAndReleaseTouchBy(1, 1);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchTool) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchForce) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(stylus_delegate, OnTouchForce(testing::_, 0, 1.0));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.SetTouchForce(1.0);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchTilt) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  MockTouchDelegate delegate;
  MockTouchStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Touch> touch(new Touch(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  touch->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));

  // Expect tool change to happen before frame of down event.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(root_surface, testing::_, testing::_,
                                      gfx::PointF()));
    EXPECT_CALL(stylus_delegate, OnTouchTool(0, ui::EventPointerType::kPen));
    EXPECT_CALL(stylus_delegate,
                OnTouchTilt(testing::_, 0, gfx::Vector2dF(1.0, 2.0)));
    EXPECT_CALL(delegate, OnTouchFrame());
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
    EXPECT_CALL(delegate, OnTouchFrame());
  }
  generator.set_current_screen_location(
      GetOriginOfShellSurface(shell_surface.get()));
  generator.SetTouchPointerType(ui::EventPointerType::kPen);
  generator.SetTouchTilt(1.0, 2.0);
  generator.PressTouch();
  generator.ReleaseTouch();

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, DragDropAbort) {
  Seat seat(std::make_unique<TestDataExchangeDelegate>());
  test::TestDataDeviceDelegate data_device_delegate;
  DataDevice data_device(&data_device_delegate, &seat);
  MockTouchDelegate touch_delegate;
  std::unique_ptr<Touch> touch(new Touch(&touch_delegate, &seat));
  TestDataSourceDelegate data_source_delegate;
  DataSource source(&data_source_delegate);
  Surface origin, icon;

  // Make origin into a real window so the touch can click it
  ShellSurface shell_surface(&origin);
  auto buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(10, 10));
  origin.Attach(buffer.get());
  origin.Commit();

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(touch_delegate, CanAcceptTouchEventsForSurface(&origin))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(touch_delegate, OnTouchFrame()).Times(2);
  generator.MoveTouch(origin.window()->GetBoundsInScreen().origin());

  data_device.StartDrag(&source, &origin, &icon,
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

TEST_F(TouchTest, TouchMultiple2Surfaces) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  auto child_shell_surface = test::ShellSurfaceBuilder({15, 15})
                                 .SetParent(shell_surface.get())
                                 .SetCanMinimize(false)
                                 .SetDisableMovement()
                                 .BuildShellSurface();
  auto* child_surface = child_shell_surface->root_surface();

  MockTouchDelegate delegate;
  Seat seat;
  auto touch = std::make_unique<Touch>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Touch down on the two surfaces.
  EXPECT_CALL(delegate, OnTouchShape(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(root_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(root_surface, testing::_, 1, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point origin = GetOriginOfShellSurface(shell_surface.get());
  generator.set_current_screen_location(origin);
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(child_surface))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(child_surface, testing::_, 2, gfx::PointF()));
  EXPECT_CALL(delegate, OnTouchFrame());
  const gfx::Point child_origin =
      GetOriginOfShellSurface(child_shell_surface.get());
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

TEST_F(TouchTest, IgnoresHandledEvents) {
  // A very dumb handler that simply marks all events as handled. This is needed
  // allows us to mark a mouse event as handled as it gets processed by the
  // event processor.
  class SetHandledHandler : public ui::EventHandler {
    void OnTouchEvent(ui::TouchEvent* event) override { event->SetHandled(); }
  };
  SetHandledHandler handler;
  ash::Shell::Get()->AddPreTargetHandler(&handler);

  Seat seat(std::make_unique<TestDataExchangeDelegate>());

  testing::NiceMock<MockTouchDelegate> touch_delegate;
  std::unique_ptr<Touch> touch(new Touch(&touch_delegate, &seat));

  // Make origin into a real window so the touch can click it
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // The SetHandlerHandler should have marked the event as processed. Therefore
  // the event should simply be ignored.
  EXPECT_CALL(touch_delegate, OnTouchFrame()).Times(0);

  generator.GestureTapAt(shell_surface->surface_for_testing()
                             ->window()
                             ->GetBoundsInScreen()
                             .CenterPoint());

  ash::Shell::Get()->RemovePreTargetHandler(&handler);
}

}  // namespace
}  // namespace exo
