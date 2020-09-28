// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_positioning_utils.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/exo/buffer.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/pointer_constraint_delegate.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/relative_pointer_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace exo {
namespace {

class MockPointerDelegate : public PointerDelegate {
 public:
  MockPointerDelegate() {}

  // Overridden from PointerDelegate:
  MOCK_METHOD1(OnPointerDestroying, void(Pointer*));
  MOCK_CONST_METHOD1(CanAcceptPointerEventsForSurface, bool(Surface*));
  MOCK_METHOD3(OnPointerEnter, void(Surface*, const gfx::PointF&, int));
  MOCK_METHOD1(OnPointerLeave, void(Surface*));
  MOCK_METHOD2(OnPointerMotion, void(base::TimeTicks, const gfx::PointF&));
  MOCK_METHOD3(OnPointerButton, void(base::TimeTicks, int, bool));
  MOCK_METHOD3(OnPointerScroll,
               void(base::TimeTicks, const gfx::Vector2dF&, bool));
  MOCK_METHOD1(OnPointerScrollStop, void(base::TimeTicks));
  MOCK_METHOD0(OnPointerFrame, void());
};

class MockRelativePointerDelegate : public RelativePointerDelegate {
 public:
  MockRelativePointerDelegate() = default;
  ~MockRelativePointerDelegate() = default;

  // Overridden from RelativePointerDelegate:
  MOCK_METHOD1(OnPointerDestroying, void(Pointer*));
  MOCK_METHOD3(OnPointerRelativeMotion,
               void(base::TimeTicks,
                    const gfx::Vector2dF&,
                    const gfx::Vector2dF&));
};

class MockPointerConstraintDelegate : public PointerConstraintDelegate {
 public:
  MockPointerConstraintDelegate() = default;
  ~MockPointerConstraintDelegate() = default;

  // Overridden from PointerConstraintDelegate:
  MOCK_METHOD0(OnConstraintBroken, void());
  MOCK_METHOD0(GetConstrainedSurface, Surface*());
};

class MockPointerStylusDelegate : public PointerStylusDelegate {
 public:
  MockPointerStylusDelegate() {}

  // Overridden from PointerStylusDelegate:
  MOCK_METHOD(void, OnPointerDestroying, (Pointer*));
  MOCK_METHOD(void, OnPointerToolChange, (ui::EventPointerType));
  MOCK_METHOD(void, OnPointerForce, (base::TimeTicks, float));
  MOCK_METHOD(void, OnPointerTilt, (base::TimeTicks, const gfx::Vector2dF&));
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

class PointerTest : public test::ExoTestBase {
 public:
  PointerTest() = default;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    // Sometimes underlying infra (i.e. X11 / Xvfb) may emit pointer events
    // which can break MockPointerDelegate's expectations, so they should be
    // consumed before starting. See https://crbug.com/854674.
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerTest);
};

TEST_F(PointerTest, SetCursor) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  std::unique_ptr<Surface> pointer_surface(new Surface);
  std::unique_ptr<Buffer> pointer_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  pointer_surface->Attach(pointer_buffer.get());
  pointer_surface->Commit();

  // Set pointer surface.
  pointer->SetCursor(pointer_surface.get(), gfx::Point(5, 5));
  base::RunLoop().RunUntilIdle();

  const viz::CompositorRenderPass* last_render_pass;
  {
    viz::SurfaceId surface_id = pointer->host_window()->GetSurfaceId();
    viz::SurfaceManager* surface_manager = GetSurfaceManager();
    ASSERT_TRUE(surface_manager->GetSurfaceForId(surface_id)->HasActiveFrame());
    const viz::CompositorFrame& frame =
        surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
    EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
              frame.render_pass_list.back()->output_rect);
    last_render_pass = frame.render_pass_list.back().get();
  }

  // Adjust hotspot.
  pointer->SetCursor(pointer_surface.get(), gfx::Point());
  base::RunLoop().RunUntilIdle();

  // Verify that adjustment to hotspot resulted in new frame.
  {
    viz::SurfaceId surface_id = pointer->host_window()->GetSurfaceId();
    viz::SurfaceManager* surface_manager = GetSurfaceManager();
    ASSERT_TRUE(surface_manager->GetSurfaceForId(surface_id)->HasActiveFrame());
    const viz::CompositorFrame& frame =
        surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
    EXPECT_TRUE(frame.render_pass_list.back().get() != last_render_pass);
  }

  // Unset pointer surface.
  pointer->SetCursor(nullptr, gfx::Point());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, SetCursorNull) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  pointer->SetCursor(nullptr, gfx::Point());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, pointer->root_surface());
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      shell_surface->GetWidget()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(ui::mojom::CursorType::kNone, cursor_client->GetCursor().type());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, SetCursorType) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  pointer->SetCursorType(ui::mojom::CursorType::kIBeam);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, pointer->root_surface());
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      shell_surface->GetWidget()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(ui::mojom::CursorType::kIBeam, cursor_client->GetCursor().type());

  // Set the pointer with surface after setting pointer type.
  std::unique_ptr<Surface> pointer_surface(new Surface);
  std::unique_ptr<Buffer> pointer_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  pointer_surface->Attach(pointer_buffer.get());
  pointer_surface->Commit();

  pointer->SetCursor(pointer_surface.get(), gfx::Point());
  base::RunLoop().RunUntilIdle();

  {
    viz::SurfaceId surface_id = pointer->host_window()->GetSurfaceId();
    viz::SurfaceManager* surface_manager = GetSurfaceManager();
    ASSERT_TRUE(surface_manager->GetSurfaceForId(surface_id)->HasActiveFrame());
    const viz::CompositorFrame& frame =
        surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
    EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
              frame.render_pass_list.back()->output_rect);
  }

  // Set the pointer type after the pointer surface is specified.
  pointer->SetCursorType(ui::mojom::CursorType::kCross);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, pointer->root_surface());
  EXPECT_EQ(ui::mojom::CursorType::kCross, cursor_client->GetCursor().type());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, SetCursorTypeOutsideOfSurface) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() -
                        gfx::Vector2d(1, 1));

  pointer->SetCursorType(ui::mojom::CursorType::kIBeam);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, pointer->root_surface());
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      shell_surface->GetWidget()->GetNativeWindow()->GetRootWindow());
  // The cursor type shouldn't be the specified one, since the pointer is
  // located outside of the surface.
  EXPECT_NE(ui::mojom::CursorType::kIBeam, cursor_client->GetCursor().type());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, SetCursorAndSetCursorType) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  std::unique_ptr<Surface> pointer_surface(new Surface);
  std::unique_ptr<Buffer> pointer_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  pointer_surface->Attach(pointer_buffer.get());
  pointer_surface->Commit();

  // Set pointer surface.
  pointer->SetCursor(pointer_surface.get(), gfx::Point());
  EXPECT_EQ(1u, pointer->GetActivePresentationCallbacksForTesting().size());
  base::RunLoop().RunUntilIdle();

  {
    viz::SurfaceId surface_id = pointer->host_window()->GetSurfaceId();
    viz::SurfaceManager* surface_manager = GetSurfaceManager();
    ASSERT_TRUE(surface_manager->GetSurfaceForId(surface_id)->HasActiveFrame());
    const viz::CompositorFrame& frame =
        surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
    EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
              frame.render_pass_list.back()->output_rect);
  }

  // Set the cursor type to the kNone through SetCursorType.
  pointer->SetCursorType(ui::mojom::CursorType::kNone);
  EXPECT_TRUE(pointer->GetActivePresentationCallbacksForTesting().empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, pointer->root_surface());

  // Set the same pointer surface again.
  pointer->SetCursor(pointer_surface.get(), gfx::Point());
  EXPECT_EQ(1u, pointer->GetActivePresentationCallbacksForTesting().size());
  auto& list =
      pointer->GetActivePresentationCallbacksForTesting().begin()->second;
  base::RunLoop runloop;
  list.push_back(base::BindRepeating(
      [](base::RepeatingClosure callback, const gfx::PresentationFeedback&) {
        callback.Run();
      },
      runloop.QuitClosure()));
  runloop.Run();

  {
    viz::SurfaceId surface_id = pointer->host_window()->GetSurfaceId();
    viz::SurfaceManager* surface_manager = GetSurfaceManager();
    ASSERT_TRUE(surface_manager->GetSurfaceForId(surface_id)->HasActiveFrame());
    const viz::CompositorFrame& frame =
        surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
    EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
              frame.render_pass_list.back()->output_rect);
  }

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, SetCursorNullAndSetCursorType) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  // Set nullptr surface.
  pointer->SetCursor(nullptr, gfx::Point());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, pointer->root_surface());
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      shell_surface->GetWidget()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(ui::mojom::CursorType::kNone, cursor_client->GetCursor().type());

  // Set the cursor type.
  pointer->SetCursorType(ui::mojom::CursorType::kIBeam);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, pointer->root_surface());
  EXPECT_EQ(ui::mojom::CursorType::kIBeam, cursor_client->GetCursor().type());

  // Set nullptr surface again.
  pointer->SetCursor(nullptr, gfx::Point());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, pointer->root_surface());
  EXPECT_EQ(ui::mojom::CursorType::kNone, cursor_client->GetCursor().type());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerEnter) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerLeave) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(4);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().bottom_right());

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  shell_surface.reset();
  surface.reset();

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerMotion) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(6);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  std::unique_ptr<Surface> sub_surface(new Surface);
  std::unique_ptr<SubSurface> sub(
      new SubSurface(sub_surface.get(), surface.get()));
  surface->SetSubSurfacePosition(sub_surface.get(), gfx::Point(5, 5));
  gfx::Size sub_buffer_size(5, 5);
  std::unique_ptr<Buffer> sub_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(sub_buffer_size)));
  sub_surface->Attach(sub_buffer.get());
  sub_surface->Commit();
  surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(sub_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(sub_surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get(), gfx::Point(9, 9), true, false,
                       ash::desks_util::GetActiveDeskContainerId()));
  child_shell_surface->DisableMovement();
  child_shell_surface->SetParent(shell_surface.get());
  gfx::Size child_buffer_size(15, 15);
  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size)));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(sub_surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(child_surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(10, 10)));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(10, 10));

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerButton) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(3);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate,
              OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, true));
  EXPECT_CALL(delegate,
              OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, false));
  generator.ClickLeftButton();

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerScroll) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  gfx::Point location = surface->window()->GetBoundsInScreen().origin();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(3);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(location);

  {
    // Expect fling stop followed by scroll and scroll stop.
    testing::InSequence sequence;

    EXPECT_CALL(delegate,
                OnPointerScroll(testing::_, gfx::Vector2dF(1.2, 1.2), false));
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_));
  }
  generator.ScrollSequence(location, base::TimeDelta(), 1, 1, 1, 1);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerScrollWithThreeFinger) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  gfx::Point location = surface->window()->GetBoundsInScreen().origin();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(2);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(location);

  {
    // Expect no scroll.
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_));
  }

  // Three fingers scroll.
  generator.ScrollSequence(location, base::TimeDelta(), 1, 1, 1,
                           3 /* num_fingers */);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerScrollDiscrete) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(2);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate,
              OnPointerScroll(testing::_, gfx::Vector2dF(1, 1), true));
  generator.MoveMouseWheel(1, 1);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, RegisterPointerEventsOnModal) {
  // Create modal surface.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(
      new ShellSurface(surface.get(), gfx::Point(), true, false,
                       ash::kShellWindowId_SystemModalContainer));
  shell_surface->DisableMovement();
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(5, 5))));
  surface->Attach(buffer.get());
  surface->Commit();
  ash::CenterWindow(shell_surface->GetWidget()->GetNativeWindow());
  // Make the window modal.
  shell_surface->SetSystemModal(true);
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnPointerFrame()).Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  // Pointer events on modal window should be registered.
  gfx::Point origin = surface->window()->GetBoundsInScreen().origin();
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
    generator.MoveMouseTo(origin);

    EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
    generator.MoveMouseTo(origin + gfx::Vector2d(1, 1));

    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, true));
    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, false));
    generator.ClickLeftButton();

    EXPECT_CALL(delegate,
                OnPointerScroll(testing::_, gfx::Vector2dF(1.2, 1.2), false));
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_));
    generator.ScrollSequence(origin, base::TimeDelta(), 1, 1, 1, 1);
  }

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, IgnorePointerEventsOnNonModalWhenModalIsOpen) {
  // Create surface for non-modal window.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(10, 10))));
  surface->Attach(buffer.get());
  surface->Commit();

  // Create surface for modal window.
  std::unique_ptr<Surface> surface2(new Surface);
  std::unique_ptr<ShellSurface> shell_surface2(
      new ShellSurface(surface2.get(), gfx::Point(), true, false,
                       ash::kShellWindowId_SystemModalContainer));
  shell_surface2->DisableMovement();
  std::unique_ptr<Buffer> buffer2(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(5, 5))));
  surface2->Attach(buffer2.get());
  surface2->Commit();
  ash::CenterWindow(shell_surface2->GetWidget()->GetNativeWindow());
  // Make the window modal.
  shell_surface2->SetSystemModal(true);
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnPointerFrame()).Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface2.get()))
      .WillRepeatedly(testing::Return(true));

  // Check if pointer events on non-modal window are ignored.
  gfx::Point nonModalOrigin = surface->window()->GetBoundsInScreen().origin();
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0))
        .Times(0);
    generator.MoveMouseTo(nonModalOrigin);

    EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)))
        .Times(0);
    generator.MoveMouseTo(nonModalOrigin + gfx::Vector2d(1, 1));

    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, true))
        .Times(0);
    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, false))
        .Times(0);
    generator.ClickLeftButton();

    EXPECT_CALL(delegate,
                OnPointerScroll(testing::_, gfx::Vector2dF(1.2, 1.2), false))
        .Times(0);
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_)).Times(0);
    generator.ScrollSequence(nonModalOrigin, base::TimeDelta(), 1, 1, 1, 1);

    EXPECT_CALL(delegate, OnPointerLeave(surface.get())).Times(0);
    generator.MoveMouseTo(
        surface->window()->GetBoundsInScreen().bottom_right());
  }

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, IgnorePointerLeaveOnModal) {
  // Create modal surface.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(
      new ShellSurface(surface.get(), gfx::Point(), true, false,
                       ash::kShellWindowId_SystemModalContainer));
  shell_surface->DisableMovement();
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(5, 5))));
  surface->Attach(buffer.get());
  surface->Commit();
  ash::CenterWindow(shell_surface->GetWidget()->GetNativeWindow());
  // Make the window modal.
  shell_surface->SetSystemModal(true);
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnPointerFrame()).Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  gfx::Point origin = surface->window()->GetBoundsInScreen().origin();

  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
    generator.MoveMouseTo(origin);

    // OnPointerLeave should not be called on the modal surface when the pointer
    // moves out of its bounds.
    EXPECT_CALL(delegate, OnPointerLeave(surface.get())).Times(0);
    generator.MoveMouseTo(
        surface->window()->GetBoundsInScreen().bottom_right());
  }

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, RegisterPointerEventsOnNonModal) {
  // Create surface for non-modal window.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(10, 10))));
  surface->Attach(buffer.get());
  surface->Commit();

  // Create another surface for a non-modal window.
  std::unique_ptr<Surface> surface2(new Surface);
  std::unique_ptr<ShellSurface> shell_surface2(
      new ShellSurface(surface2.get(), gfx::Point(), true, false,
                       ash::kShellWindowId_SystemModalContainer));
  shell_surface2->DisableMovement();
  std::unique_ptr<Buffer> buffer2(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(5, 5))));
  surface2->Attach(buffer2.get());
  surface2->Commit();
  ash::CenterWindow(shell_surface2->GetWidget()->GetNativeWindow());

  MockPointerDelegate delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, OnPointerFrame()).Times(testing::AnyNumber());
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface2.get()))
      .WillRepeatedly(testing::Return(true));

  // Ensure second window is non-modal.
  shell_surface2->SetSystemModal(false);
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());

  // Check if pointer events on first non-modal window are registered.
  gfx::Point firstWindowOrigin =
      surface->window()->GetBoundsInScreen().origin();
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
    generator.MoveMouseTo(firstWindowOrigin);

    EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
    generator.MoveMouseTo(firstWindowOrigin + gfx::Vector2d(1, 1));

    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, true));
    EXPECT_CALL(delegate,
                OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, false));
    generator.ClickLeftButton();

    EXPECT_CALL(delegate,
                OnPointerScroll(testing::_, gfx::Vector2dF(1.2, 1.2), false));
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_));
    generator.ScrollSequence(firstWindowOrigin, base::TimeDelta(), 1, 1, 1, 1);

    EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
    generator.MoveMouseTo(
        surface->window()->GetBoundsInScreen().bottom_right());
  }

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, DragDropAbort) {
  Seat seat;
  MockPointerDelegate pointer_delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&pointer_delegate, &seat));
  TestDataSourceDelegate data_source_delegate;
  DataSource source(&data_source_delegate);
  Surface origin, icon;

  // Make origin into a real window so the pointer can click it
  ShellSurface shell_surface(&origin);
  Buffer buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(10, 10)));
  origin.Attach(&buffer);
  origin.Commit();

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(pointer_delegate, CanAcceptPointerEventsForSurface(&origin))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(pointer_delegate, OnPointerFrame()).Times(3);
  EXPECT_CALL(pointer_delegate, OnPointerEnter(&origin, gfx::PointF(), 0));
  generator.MoveMouseTo(origin.window()->GetBoundsInScreen().origin());

  seat.StartDrag(&source, &origin, &icon, ui::mojom::DragEventSource::kMouse);
  EXPECT_TRUE(seat.get_drag_drop_operation_for_testing());

  EXPECT_CALL(pointer_delegate, OnPointerButton).Times(2);
  generator.PressLeftButton();
  EXPECT_TRUE(seat.get_drag_drop_operation_for_testing());
  generator.ReleaseLeftButton();
  EXPECT_FALSE(seat.get_drag_drop_operation_for_testing());

  EXPECT_CALL(pointer_delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerRelativeMotion) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockRelativePointerDelegate relative_delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  pointer->RegisterRelativePointerDelegate(&relative_delegate);

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(9);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(1, 1), testing::_));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(2, 2)));
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(1, 1), testing::_));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(2, 2));

  auto sub_surface = std::make_unique<Surface>();
  auto sub = std::make_unique<SubSurface>(sub_surface.get(), surface.get());
  surface->SetSubSurfacePosition(sub_surface.get(), gfx::Point(5, 5));
  gfx::Size sub_buffer_size(5, 5);
  auto sub_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(sub_buffer_size));
  sub_surface->Attach(sub_buffer.get());
  sub_surface->Commit();
  surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(sub_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(sub_surface.get(), gfx::PointF(), 0));
  // OnPointerMotion will not be called, because the pointer location is already
  // sent with OnPointerEnter, but we should still receive
  // OnPointerRelativeMotion.
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(3, 3), testing::_));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(1, 1), testing::_));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  const gfx::Point child_surface_origin =
      sub_surface->window()->GetBoundsInScreen().origin() +
      gfx::Vector2d(10, 10);
  auto child_surface = std::make_unique<Surface>();
  auto child_shell_surface = std::make_unique<ShellSurface>(
      child_surface.get(), child_surface_origin, true, false,
      ash::desks_util::GetActiveDeskContainerId());
  child_shell_surface->DisableMovement();
  child_shell_surface->SetParent(shell_surface.get());
  gfx::Size child_buffer_size(15, 15);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(sub_surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(child_surface.get(), gfx::PointF(), 0));
  // OnPointerMotion will not be called, because the pointer location is already
  // sent with OnPointerEnter, but we should still receive
  // OnPointerRelativeMotion.
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(9, 9), testing::_));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(10, 10)));
  EXPECT_CALL(
      relative_delegate,
      OnPointerRelativeMotion(testing::_, gfx::Vector2dF(10, 10), testing::_));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(10, 10));

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  EXPECT_CALL(relative_delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OrdinalMotionOverridesRelativeMotion) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  // Set up the pointer and move it to the origin.
  testing::NiceMock<MockPointerDelegate> delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  gfx::Point origin = surface->window()->GetBoundsInScreen().origin();
  generator.MoveMouseTo(origin);

  // Start sending relative motion events.
  testing::StrictMock<MockRelativePointerDelegate> relative_delegate;
  pointer->RegisterRelativePointerDelegate(&relative_delegate);

  // By default, ordinal and relative are the same.
  gfx::Point new_location = origin + gfx::Vector2d(1, 1);
  ui::MouseEvent ev1(ui::ET_MOUSE_MOVED, new_location, new_location,
                     ui::EventTimeForNow(), generator.flags(), 0);
  EXPECT_CALL(relative_delegate,
              OnPointerRelativeMotion(testing::_, gfx::Vector2dF(1, 1),
                                      gfx::Vector2dF(1, 1)));
  generator.Dispatch(&ev1);

  // When set, ordinal overrides the relative motion.
  //
  // TODO(b/161755250): the ifdef is only necessary because of the feature
  // flag. This code should work fine on non-cros.
#if defined(OS_CHROMEOS)
  new_location = new_location + gfx::Vector2d(1, 1);
  ui::MouseEvent ev2(ui::ET_MOUSE_MOVED, new_location, new_location,
                     ui::EventTimeForNow(), generator.flags(), 0);
  ui::MouseEvent::DispatcherApi(&ev2).set_movement(gfx::Vector2dF(99, 99));
  EXPECT_CALL(relative_delegate,
              OnPointerRelativeMotion(testing::_, gfx::Vector2dF(1, 1),
                                      gfx::Vector2dF(99, 99)));
  // This feature is gated behind a flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kExoOrdinalMotion);
  generator.Dispatch(&ev2);
#endif

  pointer->UnregisterRelativePointerDelegate(&relative_delegate);
}

#if defined(OS_CHROMEOS)
TEST_F(PointerTest, ConstrainPointer) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      chromeos::features::kExoPointerLock);

  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockPointerConstraintDelegate constraint_delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
      ->FocusWindow(surface->window());
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(constraint_delegate, GetConstrainedSurface())
      .WillRepeatedly(testing::Return(surface.get()));
  EXPECT_TRUE(pointer->ConstrainPointer(&constraint_delegate));

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  EXPECT_CALL(delegate, OnPointerFrame());
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, testing::_)).Times(0);
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(-1, -1));

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

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));

  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(child_surface.get(), gfx::PointF(), 0));
  EXPECT_CALL(delegate, OnPointerFrame());
  // Moving the cursor to a different surface should change the focus when
  // the pointer is unconstrained.
  pointer->UnconstrainPointer();
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(PointerTest, ConstrainPointerFailsWhenSurfaceIsNotFocused) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      chromeos::features::kExoPointerLock);

  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockPointerConstraintDelegate constraint_delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(constraint_delegate, GetConstrainedSurface())
      .WillRepeatedly(testing::Return(surface.get()));
  EXPECT_FALSE(pointer->ConstrainPointer(&constraint_delegate));

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

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));

  focus_client->FocusWindow(child_surface->window());
  EXPECT_FALSE(pointer->ConstrainPointer(&constraint_delegate));

  focus_client->FocusWindow(surface->window());
  EXPECT_TRUE(pointer->ConstrainPointer(&constraint_delegate));

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  EXPECT_CALL(delegate, OnPointerFrame());
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  pointer->UnconstrainPointer();

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(PointerTest, UnconstrainPointerWhenSurfaceIsDestroyed) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      chromeos::features::kExoPointerLock);

  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockPointerConstraintDelegate constraint_delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
      ->FocusWindow(surface->window());
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(constraint_delegate, GetConstrainedSurface())
      .WillRepeatedly(testing::Return(surface.get()));
  EXPECT_TRUE(pointer->ConstrainPointer(&constraint_delegate));

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  EXPECT_CALL(delegate, OnPointerFrame());
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  // Constraint should be broken if surface is destroyed.
  EXPECT_CALL(constraint_delegate, OnConstraintBroken());
  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  EXPECT_CALL(delegate, OnPointerFrame());
  pointer->OnSurfaceDestroying(surface.get());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(PointerTest, UnconstrainPointerWhenWindowLosesFocus) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      chromeos::features::kExoPointerLock);

  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockPointerConstraintDelegate constraint_delegate;
  Seat seat;
  auto pointer = std::make_unique<Pointer>(&delegate, &seat);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(surface->window());
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(constraint_delegate, GetConstrainedSurface())
      .WillRepeatedly(testing::Return(surface.get()));
  EXPECT_TRUE(pointer->ConstrainPointer(&constraint_delegate));

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  EXPECT_CALL(delegate, OnPointerFrame());
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(constraint_delegate, OnConstraintBroken());
  focus_client->FocusWindow(nullptr);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}
#endif

TEST_F(PointerTest, PointerStylus) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  MockPointerStylusDelegate stylus_delegate;
  Seat seat;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate, &seat));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  pointer->SetStylusDelegate(&stylus_delegate);

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));

  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
    EXPECT_CALL(delegate, OnPointerFrame());
    EXPECT_CALL(stylus_delegate,
                OnPointerToolChange(ui::EventPointerType::kMouse));
    EXPECT_CALL(delegate, OnPointerFrame());
  }

  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  EXPECT_CALL(stylus_delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

}  // namespace
}  // namespace exo
