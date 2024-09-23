// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <wayland-client-core.h>

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/test/bind.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/shell_client_data.h"
#include "components/exo/wayland/test/test_client.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "ui/aura/client/drag_drop_client.h"

namespace exo::wayland {

using DataDeviceManagerTest = test::WaylandServerTest;

namespace {

class InputListenerImpl : public test::InputListener {
 public:
  // test::InputListener:
  void OnButtonPressed(uint32_t serial, uint32_t button) override {
    button_serial_map[button] = serial;
  }
  void OnTouchDown(uint32_t serial,
                   wl_surface* surface,
                   int32_t id,
                   const gfx::PointF& point) override {
    touch_serial_map[id] = serial;
  }

  base::flat_map<uint32_t, uint32_t> button_serial_map;
  base::flat_map<int32_t, uint32_t> touch_serial_map;
};

}  // namespace

// TODO(crbug.com/41494812): enable the flaky test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_Mouse DISABLED_Mouse
#else
#define MAYBE_Mouse Mouse
#endif
TEST_F(DataDeviceManagerTest, MAYBE_Mouse) {
  test::ResourceKey surface_key;
  InputListenerImpl* input_listener = nullptr;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));
    auto* data_ptr =
        client->set_data(std::make_unique<test::ShellClientData>(client));

    auto input_listener_impl = std::make_unique<InputListenerImpl>();
    input_listener = input_listener_impl.get();
    data_ptr->set_input_listener(std::move(input_listener_impl));

    data_ptr->CreateXdgToplevel();
    data_ptr->CreateAndAttachBuffer({256, 256});
    data_ptr->Commit();

    surface_key = data_ptr->GetSurfaceResourceKey();
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);

  auto* drag_drop_controller = static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()));

  auto* generator = GetEventGenerator();
  {
    generator->MoveMouseToCenterOf(surface->window());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();

    // process events on client side. This should not start D&D, (thus it will
    // not start nested loop) because the button has already been released
    PostToClientAndWait([&](test::TestClient* client) {
      EXPECT_TRUE(base::Contains(input_listener->button_serial_map, BTN_LEFT));
      auto* shell_client_data = client->GetDataAs<test::ShellClientData>();
      uint32_t serial = input_listener->button_serial_map[BTN_LEFT];
      shell_client_data->StartDrag(serial);
    });
  }

  {
    generator->PressLeftButton();
    generator->PressButton(ui::EF_MIDDLE_MOUSE_BUTTON);

    bool nested_loop_started;
    auto* nested_loop_started_ptr = &nested_loop_started;

    // This scenario will start D&D, which will run nested loop, so use
    // the nested loop closure to drive the test.
    drag_drop_controller->SetLoopClosureForTesting(
        base::BindLambdaForTesting([generator, nested_loop_started_ptr]() {
          generator->ReleaseLeftButton();
          *nested_loop_started_ptr = true;
        }),
        base::DoNothing());

    PostToClientAndWait([&](test::TestClient* client) {
      EXPECT_TRUE(base::Contains(input_listener->button_serial_map, BTN_LEFT));
      auto* shell_client_data = client->GetDataAs<test::ShellClientData>();
      uint32_t serial = input_listener->button_serial_map[BTN_LEFT];
      shell_client_data->StartDrag(serial);
    });
    EXPECT_TRUE(nested_loop_started);
  }

  {
    generator->PressLeftButton();
    generator->PressButton(ui::EF_MIDDLE_MOUSE_BUTTON);
    generator->ReleaseButton(ui::EF_MIDDLE_MOUSE_BUTTON);

    bool nested_loop_started = false;
    auto* nested_loop_started_ptr = &nested_loop_started;

    // This scenario will start D&D, which will run nested loop, so use
    // the nested loop closure to drive the test.
    drag_drop_controller->SetLoopClosureForTesting(
        base::BindLambdaForTesting([generator, nested_loop_started_ptr]() {
          generator->ReleaseLeftButton();
          *nested_loop_started_ptr = true;
        }),
        base::DoNothing());

    PostToClientAndWait([&](test::TestClient* client) {
      EXPECT_TRUE(base::Contains(input_listener->button_serial_map, BTN_LEFT));
      auto* shell_client_data = client->GetDataAs<test::ShellClientData>();
      uint32_t serial = input_listener->button_serial_map[BTN_LEFT];
      shell_client_data->StartDrag(serial);
    });
    EXPECT_TRUE(nested_loop_started);
  }
}

// TODO(crbug.com/41494812): enable the flaky test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_Touch DISABLED_Touch
#else
#define MAYBE_Touch Touch
#endif
TEST_F(DataDeviceManagerTest, MAYBE_Touch) {
  test::ResourceKey surface_key;
  InputListenerImpl* input_listener = nullptr;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));
    auto* data_ptr =
        client->set_data(std::make_unique<test::ShellClientData>(client));

    data_ptr->CreateXdgToplevel();
    data_ptr->CreateAndAttachBuffer({256, 256});
    data_ptr->Commit();
    auto input_listener_impl = std::make_unique<InputListenerImpl>();
    input_listener = input_listener_impl.get();
    data_ptr->set_input_listener(std::move(input_listener_impl));
    surface_key = data_ptr->GetSurfaceResourceKey();
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);

  auto* drag_drop_controller = static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow()));
  drag_drop_controller->enable_no_image_touch_drag_for_test();

  auto* generator = GetEventGenerator();
  {
    generator->PressTouch(
        generator->delegate()->CenterOfWindow(surface->window()));
    generator->ReleaseTouch();

    // process events on client side. This should not start D&D, (thus it will
    // not start nested loop) because the button has already been released
    PostToClientAndWait([&](test::TestClient* client) {
      EXPECT_TRUE(base::Contains(input_listener->touch_serial_map, 0));
      auto* shell_client_data = client->GetDataAs<test::ShellClientData>();
      uint32_t serial = input_listener->touch_serial_map[0];
      shell_client_data->StartDrag(serial);
    });
  }

  {
    generator->PressTouch();
    enum Step {
      Started,
      Moved,
      Released,
    };
    Step step = Step::Started;
    auto* step_ptr = &step;
    // This scenario will start D&D, which will run nested loop, so use
    // the nested loop closure to drive the test.
    drag_drop_controller->SetLoopClosureForTesting(
        base::BindLambdaForTesting([generator, step_ptr]() {
          switch (*step_ptr) {
            case Started:
              generator->MoveTouchBy(5, 5);
              *step_ptr = Moved;
              break;
            case Moved:
              generator->ReleaseTouch();
              *step_ptr = Released;
              break;
            case Released:
              break;
          }
        }),
        base::DoNothing());

    PostToClientAndWait([&](test::TestClient* client) {
      EXPECT_TRUE(base::Contains(input_listener->touch_serial_map, 0));
      auto* shell_client_data = client->GetDataAs<test::ShellClientData>();
      uint32_t serial = input_listener->touch_serial_map[0];
      shell_client_data->StartDrag(serial);
    });
    EXPECT_EQ(step, Released);
  }
}

}  // namespace exo::wayland
