// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/extended_drag_source.h"

#include <memory>
#include <string>

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {
namespace {

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() {}
  TestDataSourceDelegate(const TestDataSourceDelegate&) = delete;
  TestDataSourceDelegate& operator=(const TestDataSourceDelegate&) = delete;
  ~TestDataSourceDelegate() override = default;

  bool cancelled() const { return cancelled_; }

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override { delete this; }
  void OnTarget(const base::Optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {}
  void OnCancelled() override { cancelled_ = true; }
  void OnDndDropPerformed() override {}
  void OnDndFinished() override { finished_ = true; }
  void OnAction(DndAction dnd_action) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return true;
  }

 private:
  bool cancelled_ = false;
  bool finished_ = false;
};

class TestExtendedDragSourceDelegate : public ExtendedDragSource::Delegate {
 public:
  TestExtendedDragSourceDelegate(bool allow_drop_no_target, bool lock_cursor)
      : allow_drap_no_target_(allow_drop_no_target),
        lock_cursor_(lock_cursor) {}
  TestExtendedDragSourceDelegate(const TestExtendedDragSourceDelegate&) =
      delete;
  TestExtendedDragSourceDelegate& operator=(
      const TestExtendedDragSourceDelegate&) = delete;
  ~TestExtendedDragSourceDelegate() override = default;

  // ExtendedDragSource::Delegate:
  bool ShouldAllowDropAnywhere() const override {
    return allow_drap_no_target_;
  }

  bool ShouldLockCursor() const override { return lock_cursor_; }

  void OnSwallowed(const std::string& mime_type) override {
    ASSERT_FALSE(swallowed_);
    swallowed_ = true;
  }

  void OnUnswallowed(const std::string& mime_type,
                     const gfx::Vector2d& offset) override {
    ASSERT_TRUE(swallowed_);
    swallowed_ = false;
  }

  void OnDataSourceDestroying() override { delete this; }

 private:
  const bool allow_drap_no_target_;
  const bool lock_cursor_;

  bool swallowed_ = true;
};

class ExtendedDragSourceTest : public test::ExoTestBase {
 public:
  ExtendedDragSourceTest() {}
  ExtendedDragSourceTest(const ExtendedDragSourceTest&) = delete;
  ExtendedDragSourceTest& operator=(const ExtendedDragSourceTest&) = delete;
  ~ExtendedDragSourceTest() override = default;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    drag_drop_controller_ = static_cast<ash::DragDropController*>(
        aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()));
    ASSERT_TRUE(drag_drop_controller_);
    drag_drop_controller_->set_should_block_during_drag_drop(false);
    drag_drop_controller_->set_enabled(true);

    seat_ =
        std::make_unique<Seat>(std::make_unique<TestDataExchangeDelegate>());
    data_source_ = std::make_unique<DataSource>(new TestDataSourceDelegate);
    extended_drag_source_ = std::make_unique<ExtendedDragSource>(
        data_source_.get(), new TestExtendedDragSourceDelegate(
                                /*allow_drop_no_target=*/true,
                                /*lock_cursor=*/true));
  }

  void TearDown() override {
    extended_drag_source_.reset();
    data_source_.reset();
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

 protected:
  void StartExtendedDragSession(aura::Window* origin,
                                gfx::Point start_location,
                                int operation,
                                ui::mojom::DragEventSource source) {
    auto data = std::make_unique<ui::OSExchangeData>();
    data->SetString(u"I am being dragged");
    drag_drop_controller_->set_toplevel_window_drag_delegate(
        extended_drag_source_.get());
    drag_drop_controller_->StartDragAndDrop(std::move(data),
                                            origin->GetRootWindow(), origin,
                                            start_location, operation, source);
  }

  std::unique_ptr<Buffer> CreateBuffer(gfx::Size size) {
    return std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer(size));
  }

  ash::DragDropController* drag_drop_controller_ = nullptr;
  std::unique_ptr<Seat> seat_;
  std::unique_ptr<DataSource> data_source_;
  std::unique_ptr<ExtendedDragSource> extended_drag_source_;
};

}  // namespace

TEST_F(ExtendedDragSourceTest, DestroySource) {
  Surface origin;

  // Give |origin| a root window and start DragDropOperation.
  GetContext()->AddChild(origin.window());
  seat_->StartDrag(data_source_.get(), &origin,
                   /*icon=*/nullptr, ui::mojom::DragEventSource::kMouse);

  // Ensure that destroying the data source invalidates its extended_drag_source
  // counterpart for the rest of its lifetime.
  EXPECT_TRUE(seat_->get_drag_drop_operation_for_testing());
  EXPECT_TRUE(extended_drag_source_->IsActive());
  data_source_.reset();
  EXPECT_FALSE(seat_->get_drag_drop_operation_for_testing());
  EXPECT_FALSE(extended_drag_source_->IsActive());
}

TEST_F(ExtendedDragSourceTest, DragSurfaceAlreadyMapped) {
  // Create and map a toplevel shell surface.
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  auto buffer = CreateBuffer({32, 32});
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Point origin(0, 0);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, buffer->GetSize()));
  EXPECT_EQ(origin, surface->window()->GetBoundsInRootWindow().origin());

  // Set it as the dragged surface when it's already mapped. This allows clients
  // to set existing/visible windows as the dragged surface and possibly
  // snapping it to another surface, which is required for Chrome's tab drag use
  // case, for example.
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  extended_drag_source_->Drag(surface.get(), gfx::Vector2d());
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());
  EXPECT_TRUE(extended_drag_source_->GetDragOffsetForTesting().has_value());
  EXPECT_EQ(gfx::Vector2d(0, 0),
            *extended_drag_source_->GetDragOffsetForTesting());

  // Start the DND + extended-drag session.
  StartExtendedDragSession(window, gfx::Point(0, 0),
                           ui::DragDropTypes::DRAG_MOVE,
                           ui::mojom::DragEventSource::kMouse);

  // Verify that dragging it by 190,190, with the current pointer location being
  // 10,10 will set the dragged window bounds as expected.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.DragMouseBy(190, 190);
  EXPECT_EQ(gfx::Point(200, 200), window->GetBoundsInScreen().origin());
}

TEST_F(ExtendedDragSourceTest, DragSurfaceNotMappedYet) {
  // Create and Map the drag origin surface
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  auto buffer = CreateBuffer({32, 32});
  surface->Attach(buffer.get());
  surface->Commit();

  // Start the DND + extended-drag session.
  StartExtendedDragSession(shell_surface->GetWidget()->GetNativeWindow(),
                           gfx::Point(0, 0), ui::DragDropTypes::DRAG_MOVE,
                           ui::mojom::DragEventSource::kMouse);

  // Create a new surface to emulate a "detachment" process.
  auto detached_surface = std::make_unique<Surface>();
  auto detached_shell_surface =
      std::make_unique<ShellSurface>(detached_surface.get());

  // Set |surface| as the dragged surface while it's still unmapped/invisible.
  // This can be used to implement tab detaching in Chrome's tab drag use case,
  // for example. Extended drag source will monitor surface mapping and it's
  // expected to position it correctly using the provided drag offset here
  // relative to the current pointer location.
  extended_drag_source_->Drag(detached_surface.get(), gfx::Vector2d(10, 10));
  EXPECT_FALSE(extended_drag_source_->GetDraggedWindowForTesting());
  EXPECT_TRUE(extended_drag_source_->GetDragOffsetForTesting().has_value());
  EXPECT_EQ(gfx::Vector2d(10, 10),
            *extended_drag_source_->GetDragOffsetForTesting());

  // Map the |detached_surface|.
  auto detached_buffer = CreateBuffer({50, 50});
  detached_surface->Attach(detached_buffer.get());
  detached_surface->Commit();

  // Ensure the toplevel window for the dragged surface set above, is correctly
  // detected, after it's mapped.
  aura::Window* window = detached_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_TRUE(extended_drag_source_->GetDraggedWindowForTesting());
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());

  // Verify that dragging it by 100,100, with drag offset 10,10 and current
  // pointer location 50,50 will set the dragged window bounds as expected.
  ui::test::EventGenerator generator(GetContext());
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.DragMouseBy(50, 50);
  EXPECT_EQ(gfx::Point(140, 140), window->GetBoundsInScreen().origin());
}

TEST_F(ExtendedDragSourceTest, DestroyDraggedSurfaceWhileDragging) {
  // Create and map a toplevel shell surface.
  gfx::Size buffer_size(32, 32);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Point origin(0, 0);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, buffer_size));
  EXPECT_EQ(origin, surface->window()->GetBoundsInRootWindow().origin());

  // Start dragging |surface|'s window.
  extended_drag_source_->Drag(surface.get(), gfx::Vector2d());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());

  // Make sure extended drag source gracefully handles window destruction during
  // while the drag session is still alive.
  shell_surface.reset();
  EXPECT_TRUE(surface->window()->GetBoundsInScreen().origin().IsOrigin());

  ui::test::EventGenerator generator(GetContext());
  generator.DragMouseBy(190, 190);
  EXPECT_TRUE(surface->window()->GetBoundsInScreen().origin().IsOrigin());
}

}  // namespace exo
