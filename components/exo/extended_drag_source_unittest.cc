// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/extended_drag_source.h"

#include <memory>
#include <string>

#include "ash/constants/app_types.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/drag_drop_operation.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace exo {
namespace {

void DispatchGesture(ui::EventType gesture_type, gfx::Point location) {
  ui::GestureEventDetails event_details(gesture_type);
  ui::GestureEvent gesture_event(location.x(), location.y(), 0,
                                 ui::EventTimeForNow(), event_details);
  ui::EventSource* event_source =
      ash::Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource();
  ui::EventSourceTestApi event_source_test(event_source);
  ui::EventDispatchDetails details =
      event_source_test.SendEventToSink(&gesture_event);
  CHECK(!details.dispatcher_destroyed);
}

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
    data_source_delegate_ = std::make_unique<TestDataSourceDelegate>();
    data_source_ = std::make_unique<DataSource>(data_source_delegate_.get());
    extended_drag_source_ = std::make_unique<ExtendedDragSource>(
        data_source_.get(), new TestExtendedDragSourceDelegate(
                                /*allow_drop_no_target=*/true,
                                /*lock_cursor=*/true));
  }

  void TearDown() override {
    extended_drag_source_.reset();
    data_source_.reset();
    data_source_delegate_.reset();
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
  std::unique_ptr<TestDataSourceDelegate> data_source_delegate_;
};

// Enables or disables tablet mode and waits for the transition to finish.
void SetTabletModeEnabled(bool enabled) {
  // This does not use ShellTestApi or TabletModeControllerTestApi because those
  // are implemented in test-only files.
  ash::TabletMode::Waiter waiter(enabled);
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
  waiter.Wait();
}

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
  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.PressLeftButton();
  StartExtendedDragSession(window, gfx::Point(0, 0),
                           ui::DragDropTypes::DRAG_MOVE,
                           ui::mojom::DragEventSource::kMouse);

  // Verify that dragging it by 190,190, with the current pointer location being
  // 10,10 will set the dragged window bounds as expected.
  generator.MoveMouseBy(190, 190);
  generator.ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(200, 200), window->GetBoundsInScreen().origin());
}

// This test covers the basic scenario of `DragSurfaceAlreadyMapped` above, but
// in this case simulates the call to
// ExtendedDragSource::OnToplevelWindowDragStarted() happening before the
// associated call to ExtendedDragSource::Drag().
//
// This is a race scenario in current code.
TEST_F(ExtendedDragSourceTest, DragSurfaceAlreadyMapped_Race) {
  // Create and map a toplevel shell surface.
  gfx::Size buffer_size({32, 32});
  auto shell_surface =
      exo::test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  gfx::Point origin(0, 0);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, buffer_size));
  EXPECT_EQ(origin, surface->window()->GetBoundsInRootWindow().origin());

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Start the DND + extended-drag session.
  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.PressLeftButton();
  StartExtendedDragSession(window, gfx::Point(0, 0),
                           ui::DragDropTypes::DRAG_MOVE,
                           ui::mojom::DragEventSource::kMouse);

  // Set it as the dragged surface when it's already mapped. This allows clients
  // to set existing/visible windows as the dragged surface and possibly
  // snapping it to another surface, which is required for Chrome's tab drag use
  // case, for example.
  extended_drag_source_->Drag(surface, gfx::Vector2d());
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());
  EXPECT_TRUE(extended_drag_source_->GetDragOffsetForTesting().has_value());
  EXPECT_EQ(gfx::Vector2d(0, 0),
            *extended_drag_source_->GetDragOffsetForTesting());

  // Verify that dragging it by 190,190, with the current pointer location being
  // 10,10 will set the dragged window bounds as expected.
  generator.MoveMouseBy(190, 190);
  generator.ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(200, 200), window->GetBoundsInScreen().origin());
}

// This class installs an observer to the window being dragged.
// The goal is to ensure the drag 'n drop only effectively starts
// off of the aura::WindowObserver::OnWindowVisibilityChanged() hook,
// when it is guarantee the its state is properly set.
class WindowObserverHookChecker : public aura::WindowObserver {
 public:
  explicit WindowObserverHookChecker(aura::Window* surface_window)
      : surface_window_(surface_window) {
    DCHECK(!surface_window_->GetRootWindow());
    surface_window_->AddObserver(this);
  }
  ~WindowObserverHookChecker() override {
    DCHECK(dragged_window_);
    dragged_window_->RemoveObserver(this);
  }

  void OnWindowAddedToRootWindow(aura::Window* window) override {
    dragged_window_ = surface_window_->GetToplevelWindow();
    dragged_window_->AddObserver(this);
    surface_window_->RemoveObserver(this);

    dragged_window_->SetProperty(aura::client::kAppType,
                                 static_cast<int>(ash::AppType::LACROS));
  }
  MOCK_METHOD(void,
              OnWindowVisibilityChanging,
              (aura::Window*, bool),
              (override));
  MOCK_METHOD(void,
              OnWindowVisibilityChanged,
              (aura::Window*, bool),
              (override));

 private:
  aura::Window* surface_window_ = nullptr;
  aura::Window* dragged_window_ = nullptr;
};

// Differently than the window observer class above, this one observers
// the window instance being directly provided to its ctor.
class WindowObserverHookChecker2 : public aura::WindowObserver {
 public:
  explicit WindowObserverHookChecker2(aura::Window* surface_window)
      : surface_window_(surface_window) {
    surface_window_->AddObserver(this);
  }
  ~WindowObserverHookChecker2() override {
    surface_window_->RemoveObserver(this);
  }
  MOCK_METHOD(void,
              OnWindowPropertyChanged,
              (aura::Window*, const void*, intptr_t),
              (override));

 private:
  aura::Window* surface_window_ = nullptr;
};

TEST_F(ExtendedDragSourceTest, DragSurfaceNotMappedYet) {
  // Create and Map the drag origin surface
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  auto buffer = CreateBuffer({32, 32});
  surface->Attach(buffer.get());
  surface->Commit();

  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.PressLeftButton();

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

  // Ensure drag 'n drop starts after
  // ExtendedDragSource::OnDraggedWindowVisibilityChanged()
  aura::Window* toplevel_window;
  WindowObserverHookChecker checker(detached_surface->window());
  EXPECT_CALL(checker, OnWindowVisibilityChanging(_, _))
      .Times(1)
      .WillOnce(DoAll(
          SaveArg<0>(&toplevel_window), InvokeWithoutArgs([&]() {
            auto* toplevel_handler =
                ash::Shell::Get()->toplevel_window_event_handler();
            EXPECT_FALSE(toplevel_handler->is_drag_in_progress());
            EXPECT_TRUE(toplevel_window->GetProperty(ash::kIsDraggingTabsKey));
          })));

  EXPECT_CALL(checker, OnWindowVisibilityChanged(_, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([]() {
        auto* toplevel_handler =
            ash::Shell::Get()->toplevel_window_event_handler();
        EXPECT_TRUE(toplevel_handler->is_drag_in_progress());
      }));

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
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.DragMouseBy(50, 50);
  generator.ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(140, 140), window->GetBoundsInScreen().origin());
}

TEST_F(ExtendedDragSourceTest, DragSurfaceNotMappedYet_TabletMode) {
  SetTabletModeEnabled(true);

  // Create and Map the drag origin surface
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  auto buffer = CreateBuffer({32, 32});
  surface->Attach(buffer.get());
  surface->Commit();

  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.PressLeftButton();

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

  // Ensure drag 'n drop starts after
  // ExtendedDragSource::OnDraggedWindowVisibilityChanged()
  aura::Window* toplevel_window = nullptr;
  WindowObserverHookChecker checker(detached_surface->window());
  EXPECT_CALL(checker, OnWindowVisibilityChanging(_, _))
      .Times(1)
      .WillOnce(DoAll(
          SaveArg<0>(&toplevel_window), InvokeWithoutArgs([&]() {
            auto* toplevel_handler =
                ash::Shell::Get()->toplevel_window_event_handler();
            EXPECT_FALSE(toplevel_handler->is_drag_in_progress());
            EXPECT_TRUE(toplevel_window->GetProperty(ash::kIsDraggingTabsKey));
          })));
  EXPECT_CALL(checker, OnWindowVisibilityChanged(_, _))
      .Times(1)
      .WillOnce(DoAll(
          SaveArg<0>(&toplevel_window), InvokeWithoutArgs([&]() {
            auto* toplevel_handler =
                ash::Shell::Get()->toplevel_window_event_handler();
            EXPECT_TRUE(toplevel_handler->is_drag_in_progress());
            gfx::Rect* override_bounds =
                toplevel_window->GetProperty(ash::kRestoreBoundsOverrideKey);
            EXPECT_TRUE(override_bounds && !override_bounds->IsEmpty());
            EXPECT_EQ(*override_bounds, toplevel_window->bounds());
          })));

  // Map the |detached_surface|.
  auto detached_buffer = CreateBuffer({50, 50});
  detached_surface->Attach(detached_buffer.get());
  detached_surface->Commit();

  // Ensure the toplevel window for the dragged surface set above, is correctly
  // detected, after it's mapped.
  aura::Window* window = detached_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_TRUE(extended_drag_source_->GetDraggedWindowForTesting());
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());

  WindowObserverHookChecker2 checker2(
      shell_surface->GetWidget()->GetNativeWindow());
  aura::Window* source_window = nullptr;
  const void* property_key;
  EXPECT_CALL(checker2, OnWindowPropertyChanged(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          DoAll(SaveArg<0>(&source_window), SaveArg<1>(&property_key),
                InvokeWithoutArgs([&]() {
                  if (property_key ==
                      chromeos::kIsDeferredTabDraggingTargetWindowKey) {
                    bool new_value = source_window->GetProperty(
                        chromeos::kIsDeferredTabDraggingTargetWindowKey);
                    EXPECT_TRUE(new_value);
                  }
                })));

  generator.ReleaseLeftButton();
  SetTabletModeEnabled(false);
}

TEST_F(ExtendedDragSourceTest, DragSurfaceNotMappedYet_Touch) {
  // Create and Map the drag origin surface
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  auto buffer = CreateBuffer({32, 32});
  surface->Attach(buffer.get());
  surface->Commit();

  // Start the DND + extended-drag session.
  StartExtendedDragSession(shell_surface->GetWidget()->GetNativeWindow(),
                           gfx::Point(0, 0), ui::DragDropTypes::DRAG_MOVE,
                           ui::mojom::DragEventSource::kTouch);

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

  // Initiate the gesture sequence.
  DispatchGesture(ui::ET_GESTURE_BEGIN, gfx::Point(10, 10));

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
  generator.PressMoveAndReleaseTouchBy(50, 50);
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
  aura::Window* dragged_window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(dragged_window,
            extended_drag_source_->GetDraggedWindowForTesting());

  // Create, show |source_window| and start an (extended-)drag-and-drop
  // session with it as the source window.
  auto source_window = CreateToplevelTestWindow({10, 10, 200, 100});
  extended_drag_source_->OnToplevelWindowDragStarted(
      {50.0, 50.0}, ui::mojom::DragEventSource::kMouse, source_window.get());
  EXPECT_EQ(extended_drag_source_->GetDragSourceWindowForTesting(),
            source_window.get());

  // Make sure extended drag source gracefully handles |dragged_window|
  // visibility change (calls into dragged_window->Hide()) during while the drag
  // session is still alive.
  shell_surface.reset();
  EXPECT_EQ(extended_drag_source_->GetDraggedWindowForTesting(), nullptr);
  EXPECT_TRUE(surface->window()->GetBoundsInScreen().origin().IsOrigin());

  ui::test::EventGenerator generator(GetContext());
  generator.DragMouseBy(190, 190);
  EXPECT_TRUE(surface->window()->GetBoundsInScreen().origin().IsOrigin());
}

// Regression test for crbug.com/1330125.
// In exo, the drag source window could be destroyed while the dragged window
// is updating its visibility. The example in this bug was while merging a
// single-tab browser window in and out of another browser window.
TEST_F(ExtendedDragSourceTest, DestroyDragSourceWindowWhileDragging) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({32, 32}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  // Start hidden so when it's shown later it triggers the memory violation.
  shell_surface->GetWidget()->Hide();

  extended_drag_source_->Drag(surface, gfx::Vector2d());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(window, extended_drag_source_->GetDraggedWindowForTesting());

  auto drag_source_window = CreateToplevelTestWindow({20, 30, 200, 100});
  extended_drag_source_->OnToplevelWindowDragStarted(
      {70.0, 70.0}, ui::mojom::DragEventSource::kMouse,
      drag_source_window.get());
  EXPECT_EQ(extended_drag_source_->GetDragSourceWindowForTesting(),
            drag_source_window.get());
  drag_source_window.reset();
  EXPECT_EQ(extended_drag_source_->GetDragSourceWindowForTesting(), nullptr);
  // Without the fix, in asan build, this should cause the use-after-free.
  shell_surface->GetWidget()->Show();
}

TEST_F(ExtendedDragSourceTest, DragRequestsInRow_NoCrash) {
  // Create and map a toplevel shell surface, but hidden.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({32, 32}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  shell_surface->GetWidget()->Hide();

  // Request two dragging |surface|'s actions in a roll, which a real
  // world use case scenario when system is under heavy load.
  extended_drag_source_->Drag(surface, gfx::Vector2d());
  extended_drag_source_->Drag(surface, gfx::Vector2d());

  // Make sure extended drag source gracefully handles window destruction during
  // while the drag session is still alive.
  shell_surface.reset();
}

TEST_F(ExtendedDragSourceTest, CancelDraggingOperation) {
  // Create and map a toplevel shell surface.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({32, 32}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  extended_drag_source_->Drag(surface, gfx::Vector2d());

  // Start the DND + extended-drag session.
  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.PressLeftButton();

  // Start a DragDropOperation.
  drag_drop_controller_->set_should_block_during_drag_drop(true);
  seat_->StartDrag(data_source_.get(), surface, /*icon=*/nullptr,
                   ui::mojom::DragEventSource::kMouse);

  auto task_1 = base::BindLambdaForTesting([&]() {
    generator.MoveMouseBy(190, 190);

    auto* toplevel_handler = ash::Shell::Get()->toplevel_window_event_handler();
    EXPECT_TRUE(toplevel_handler->is_drag_in_progress());
    drag_drop_controller_->DragCancel();
    EXPECT_FALSE(toplevel_handler->is_drag_in_progress());
  });
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(task_1)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_source_delegate_->cancelled());
}

// Make sure that the extended drag is recognized as shell surface drag.
TEST_F(ExtendedDragSourceTest, DragWithScreenCoordinates) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  TestDataExchangeDelegate data_exchange_delegate;

  auto delegate = std::make_unique<TestDataSourceDelegate>();
  auto data_source = std::make_unique<DataSource>(delegate.get());
  constexpr char kTextMimeType[] = "text/plain";
  data_source->Offer(kTextMimeType);

  gfx::PointF location(10, 10);
  auto operation = DragDropOperation::Create(
      &data_exchange_delegate, data_source.get(), shell_surface->root_surface(),
      nullptr, location, ui::mojom::DragEventSource::kMouse);
  auto* drag_drop_controller = static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()));

  EXPECT_FALSE(shell_surface->IsDragged());
  base::RunLoop loop;
  drag_drop_controller->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        // The drag session must have been started by the time
        // drag loop starts.
        EXPECT_TRUE(shell_surface->IsDragged());
        drag_drop_controller->DragCancel();
        loop.Quit();
      }),
      base::DoNothing());
  loop.Run();
  operation.reset();
  EXPECT_FALSE(shell_surface->IsDragged());
}

TEST_F(ExtendedDragSourceTest, DragWithScreenCoordinates_Touch) {
  // Create and map a toplevel shell surface.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({32, 32}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  extended_drag_source_->Drag(surface, gfx::Vector2d());

  // Start the DND + extended-drag session.
  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator generator(GetContext(), gfx::Point(10, 10));
  generator.MoveTouch(surface->window()->GetBoundsInScreen().origin());
  generator.PressTouch();

  // Start a DragDropOperation.
  drag_drop_controller_->set_should_block_during_drag_drop(true);
  seat_->StartDrag(data_source_.get(), surface, /*icon=*/nullptr,
                   ui::mojom::DragEventSource::kTouch);

  base::RunLoop loop;
  drag_drop_controller_->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        auto* toplevel_handler =
            ash::Shell::Get()->toplevel_window_event_handler();
        EXPECT_TRUE(toplevel_handler->is_drag_in_progress());

        auto* window_state =
            ash::WindowState::Get(surface->window()->GetToplevelWindow());
        auto* drag_details = window_state->drag_details();
        DCHECK(drag_details);
        EXPECT_EQ(gfx::ToRoundedPoint(drag_details->initial_location_in_parent),
                  surface->window()->GetBoundsInScreen().origin());
        drag_drop_controller_->DragCancel();
        EXPECT_FALSE(toplevel_handler->is_drag_in_progress());
        loop.Quit();
      }),
      base::DoNothing());

  loop.Run();
  EXPECT_TRUE(data_source_delegate_->cancelled());
}

TEST_F(ExtendedDragSourceTest, DragToAnotherDisplay) {
  UpdateDisplay("400x300,800x600");
  const gfx::Rect kOriginalWindowBounds(410, 10, 500, 200);

  // Create and map a toplevel shell surface, with the size larger than 2nd
  // display to test if configure uses the adjusted size.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder(kOriginalWindowBounds.size())
          .SetOrigin(kOriginalWindowBounds.origin())
          .BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  EXPECT_EQ(kOriginalWindowBounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());
  EXPECT_EQ(gfx::Rect(400, 0, 800, 600), display.bounds());

  uint32_t serial = 0;
  gfx::Rect drop_bounds;
  auto configure_callback = base::BindLambdaForTesting(
      [&](const gfx::Rect& bounds, chromeos::WindowStateType state_type,
          bool resizing, bool activated, const gfx::Vector2d& origin_offset) {
        drop_bounds = bounds;
        return ++serial;
      });
  std::vector<gfx::Point> origins;
  auto origin_change_callback = base::BindLambdaForTesting(
      [&](const gfx::Point& point) { origins.push_back(point); });

  // Map shell surface.
  shell_surface->set_configure_callback(configure_callback);
  shell_surface->set_origin_change_callback(origin_change_callback);

  constexpr int kDragOffset = 100;
  extended_drag_source_->Drag(surface, gfx::Vector2d(kDragOffset, 0));

  // Start the DND + extended-drag session.
  // Creates a mouse-pressed event before starting the drag session.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo({410 + kDragOffset, 10});
  generator->PressLeftButton();

  // Start a DragDropOperation.
  drag_drop_controller_->set_should_block_during_drag_drop(true);
  seat_->StartDrag(data_source_.get(), surface, /*icon=*/nullptr,
                   ui::mojom::DragEventSource::kMouse);
  // Just move to the middle to avoid snapping.
  int x_movement = 300;

  constexpr int kXDragDelta = 20;
  auto* toplevel_handler = ash::Shell::Get()->toplevel_window_event_handler();

  base::RunLoop loop;
  size_t move_count = 0;
  drag_drop_controller_->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        if (x_movement == 300) {
          auto* window_state = ash::WindowState::Get(
              shell_surface->GetWidget()->GetNativeWindow());
          EXPECT_EQ(gfx::PointF(110, 10),
                    window_state->drag_details()->initial_location_in_parent);
        }
        if (x_movement > 0) {
          x_movement -= kXDragDelta;
          move_count++;
          generator->MoveMouseBy(-kXDragDelta, 0);
          EXPECT_TRUE(toplevel_handler->is_drag_in_progress());
        } else {
          generator->ReleaseLeftButton();
        }
      }),
      loop.QuitClosure());
  loop.Run();
  EXPECT_FALSE(toplevel_handler->is_drag_in_progress());
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());

  gfx::Size secondary_display_size(400, 300);
  EXPECT_EQ(gfx::Rect(secondary_display_size), display.bounds());
  EXPECT_EQ(move_count, origins.size());

  // Configure when dropped.
  EXPECT_EQ(1u, serial);
  // Upon drop, the window is shrunk horizontally.
  gfx::Rect expected_drop_bounds =
      gfx::Rect(origins.back(), kOriginalWindowBounds.size());
  expected_drop_bounds.set_width(secondary_display_size.width());
  int offset =
      (kOriginalWindowBounds.width() - expected_drop_bounds.width()) / 2;
  expected_drop_bounds.set_x(expected_drop_bounds.x() + offset);
  EXPECT_EQ(expected_drop_bounds, drop_bounds);
  EXPECT_EQ(expected_drop_bounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  // Make sure all origins are kXDragDelta apart.
  for (size_t i = 1; origins.size() < i; i++) {
    EXPECT_EQ(origins[i - 1].x() + kXDragDelta, origins[i].x());
  }

  // The size will be adjusted when dropped.
  EXPECT_EQ(gfx::Size(400, 200), drop_bounds.size());
}

}  // namespace exo
