// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/serial_tracker.h"

#include <linux/input-event-codes.h>
#include <wayland-client-core.h>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/shell_client_data.h"
#include "components/exo/wayland/test/test_client.h"
#include "components/exo/wayland/test/wayland_server_test.h"

namespace exo::wayland {

using SerialTrackerTest = test::WaylandServerTest;

namespace {

class InputListenerImpl2 : public test::InputListener {
 public:
  // test::InputListener:
  void OnButtonPressed(uint32_t serial, uint32_t button) override {
    serial_map[button] = serial;
  }
  void OnButtonReleased(uint32_t serial, uint32_t button) override {
    serial_map[button] = serial;
  }

  base::flat_map<uint32_t, uint32_t> serial_map;
};

}  // namespace

TEST_F(SerialTrackerTest, CheckButtonsSeparately) {
  test::ResourceKey surface_key;
  InputListenerImpl2* input_listener = nullptr;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));
    auto* data_ptr =
        client->set_data(std::make_unique<test::ShellClientData>(client));
    data_ptr->CreateXdgToplevel();
    data_ptr->CreateAndAttachBuffer({256, 256});
    data_ptr->Commit();
    auto input_listener_impl = std::make_unique<InputListenerImpl2>();
    input_listener = input_listener_impl.get();
    data_ptr->set_input_listener(std::move(input_listener_impl));
    surface_key = data_ptr->GetSurfaceResourceKey();
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);

  auto* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(surface->window());
  generator->PressLeftButton();

  // process events on client side.
  PostToClientAndWait([&](test::TestClient* client) {});
  EXPECT_TRUE(base::Contains(input_listener->serial_map, BTN_LEFT));
  EXPECT_FALSE(base::Contains(input_listener->serial_map, BTN_MIDDLE));
  EXPECT_FALSE(base::Contains(input_listener->serial_map, BTN_RIGHT));

  generator->PressButton(ui::EF_MIDDLE_MOUSE_BUTTON);
  generator->PressRightButton();

  // process events on client side.
  PostToClientAndWait([&](test::TestClient* client) {});
  EXPECT_TRUE(base::Contains(input_listener->serial_map, BTN_LEFT));
  EXPECT_TRUE(base::Contains(input_listener->serial_map, BTN_MIDDLE));
  EXPECT_TRUE(base::Contains(input_listener->serial_map, BTN_RIGHT));

  auto* tracker = server_->serial_tracker_for_test();
  auto get_event_type_for_serial = [=](uint32_t button) -> uint32_t {
    return tracker->GetEventType(input_listener->serial_map[button])
        .value_or(SerialTracker::EventType::OTHER_EVENT);
  };

  EXPECT_EQ(SerialTracker::EventType::POINTER_LEFT_BUTTON_DOWN,
            get_event_type_for_serial(BTN_LEFT));
  EXPECT_EQ(SerialTracker::EventType::POINTER_MIDDLE_BUTTON_DOWN,
            get_event_type_for_serial(BTN_MIDDLE));
  EXPECT_EQ(SerialTracker::EventType::POINTER_RIGHT_BUTTON_DOWN,
            get_event_type_for_serial(BTN_RIGHT));

  generator->ReleaseLeftButton();

  // process events on client side.
  PostToClientAndWait([&](test::TestClient* client) {});

  EXPECT_EQ(SerialTracker::EventType::POINTER_LEFT_BUTTON_UP,
            get_event_type_for_serial(BTN_LEFT));
  EXPECT_EQ(SerialTracker::EventType::POINTER_MIDDLE_BUTTON_DOWN,
            get_event_type_for_serial(BTN_MIDDLE));
  EXPECT_EQ(SerialTracker::EventType::POINTER_RIGHT_BUTTON_DOWN,
            get_event_type_for_serial(BTN_RIGHT));

  generator->ReleaseButton(ui::EF_MIDDLE_MOUSE_BUTTON);

  // process events on client side.
  PostToClientAndWait([&](test::TestClient* client) {});

  EXPECT_EQ(SerialTracker::EventType::POINTER_LEFT_BUTTON_UP,
            get_event_type_for_serial(BTN_LEFT));
  EXPECT_EQ(SerialTracker::EventType::POINTER_MIDDLE_BUTTON_UP,
            get_event_type_for_serial(BTN_MIDDLE));
  EXPECT_EQ(SerialTracker::EventType::POINTER_RIGHT_BUTTON_DOWN,
            get_event_type_for_serial(BTN_RIGHT));

  // Forward/Back events
  EXPECT_EQ(SerialTracker::EventType::OTHER_EVENT,
            get_event_type_for_serial(BTN_FORWARD));
  EXPECT_EQ(SerialTracker::EventType::OTHER_EVENT,
            get_event_type_for_serial(BTN_BACK));

  generator->PressButton(ui::EF_FORWARD_MOUSE_BUTTON);
  generator->PressButton(ui::EF_BACK_MOUSE_BUTTON);

  PostToClientAndWait([&](test::TestClient* client) {});

  EXPECT_EQ(SerialTracker::EventType::POINTER_FORWARD_BUTTON_DOWN,
            get_event_type_for_serial(BTN_EXTRA));
  EXPECT_EQ(SerialTracker::EventType::POINTER_BACK_BUTTON_DOWN,
            get_event_type_for_serial(BTN_SIDE));
}

}  // namespace exo::wayland
