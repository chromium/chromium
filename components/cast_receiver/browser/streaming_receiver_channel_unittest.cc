// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_channel.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/proto/input_capabilities.pb.h"
#include "components/cast_receiver/proto/input_event.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace cast_receiver {

class MockMessagePortService : public MessagePortService {
 public:
  MOCK_METHOD2(ConnectToPortAsync,
               void(std::string_view,
                    std::unique_ptr<cast_api_bindings::MessagePort>));
  MOCK_METHOD1(RegisterOutgoingPort,
               uint32_t(std::unique_ptr<cast_api_bindings::MessagePort>));
  MOCK_METHOD2(RegisterIncomingPort,
               void(uint32_t, std::unique_ptr<cast_api_bindings::MessagePort>));
  MOCK_METHOD1(Remove, void(uint32_t));
};

class StreamingReceiverChannelTest : public ::testing::Test {
 protected:
  StreamingReceiverChannelTest() {
    EXPECT_CALL(message_port_service_,
                ConnectToPortAsync(
                    StreamingReceiverChannel::kInputEventChannelNamespace, _))
        .WillOnce([this](std::string_view,
                         std::unique_ptr<cast_api_bindings::MessagePort> port) {
          event_receiver_port_ = std::move(port);
          event_receiver_port_->SetReceiver(&event_receiver_);
        });

    EXPECT_CALL(
        message_port_service_,
        ConnectToPortAsync(
            StreamingReceiverChannel::kInputCapabilitiesChannelNamespace, _))
        .WillOnce([this](std::string_view,
                         std::unique_ptr<cast_api_bindings::MessagePort> port) {
          capabilities_receiver_port_ = std::move(port);
          capabilities_receiver_port_->SetReceiver(&capabilities_receiver_);
        });

    channel_ =
        std::make_unique<StreamingReceiverChannel>(&message_port_service_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockMessagePortService message_port_service_;
  std::unique_ptr<StreamingReceiverChannel> channel_;

  std::unique_ptr<cast_api_bindings::MessagePort> event_receiver_port_;
  cast_api_bindings::TestMessagePortReceiver event_receiver_;

  std::unique_ptr<cast_api_bindings::MessagePort> capabilities_receiver_port_;
  cast_api_bindings::TestMessagePortReceiver capabilities_receiver_;
};

TEST_F(StreamingReceiverChannelTest, SerializeMouseEvent) {
  InputEvent event;
  event.set_timestamp_ms(1000);
  auto* mouse_event = event.mutable_mouse_event();
  mouse_event->set_action_type(MouseEvent::MOUSE_DOWN);
  mouse_event->set_x_ratio(0.1f);
  mouse_event->set_y_ratio(0.2f);
  mouse_event->set_move_x_ratio(0.3f);
  mouse_event->set_move_y_ratio(0.4f);
  mouse_event->set_alt_key_press(true);
  mouse_event->set_ctrl_key_press(false);
  mouse_event->set_shift_key_press(true);
  mouse_event->set_meta_key_press(false);
  mouse_event->add_buttons(MouseEvent::LEFT_BUTTON);
  mouse_event->add_buttons(MouseEvent::RIGHT_BUTTON);

  channel_->SendInputEvent(event);

  ASSERT_TRUE(event_receiver_.RunUntilMessageCountEqual(1));
  std::string received_msg = event_receiver_.buffer()[0].first;
  std::string decoded_msg;
  ASSERT_TRUE(base::Base64Decode(received_msg, &decoded_msg));

  InputEvent received_event;
  ASSERT_TRUE(received_event.ParseFromString(decoded_msg));

  EXPECT_EQ(received_event.timestamp_ms(), 1000);
  ASSERT_TRUE(received_event.has_mouse_event());
  const auto& received_mouse = received_event.mouse_event();

  EXPECT_EQ(received_mouse.action_type(), MouseEvent::MOUSE_DOWN);
  EXPECT_NEAR(received_mouse.x_ratio(), 0.1f, 1e-6);
  EXPECT_NEAR(received_mouse.y_ratio(), 0.2f, 1e-6);
  EXPECT_NEAR(received_mouse.move_x_ratio(), 0.3f, 1e-6);
  EXPECT_NEAR(received_mouse.move_y_ratio(), 0.4f, 1e-6);
  EXPECT_EQ(received_mouse.alt_key_press(), true);
  EXPECT_EQ(received_mouse.ctrl_key_press(), false);
  EXPECT_EQ(received_mouse.shift_key_press(), true);
  EXPECT_EQ(received_mouse.meta_key_press(), false);

  ASSERT_EQ(received_mouse.buttons_size(), 2);
  EXPECT_EQ(received_mouse.buttons(0), MouseEvent::LEFT_BUTTON);
  EXPECT_EQ(received_mouse.buttons(1), MouseEvent::RIGHT_BUTTON);
}

TEST_F(StreamingReceiverChannelTest, SerializeKeyboardEvent) {
  InputEvent event;
  event.set_timestamp_ms(2000);
  auto* keyboard_event = event.mutable_keyboard_event();
  keyboard_event->set_action_type(KeyboardEvent::KEY_DOWN);
  keyboard_event->set_key_code("KeyA");
  keyboard_event->set_key_value("a");
  keyboard_event->set_repeat(true);
  keyboard_event->set_alt_key_press(false);
  keyboard_event->set_ctrl_key_press(true);
  keyboard_event->set_shift_key_press(false);
  keyboard_event->set_meta_key_press(true);
  keyboard_event->set_caps_lock_enabled(true);
  keyboard_event->set_timestamp_ms(2001);

  channel_->SendInputEvent(event);

  ASSERT_TRUE(event_receiver_.RunUntilMessageCountEqual(1));
  std::string received_msg = event_receiver_.buffer()[0].first;
  std::string decoded_msg;
  ASSERT_TRUE(base::Base64Decode(received_msg, &decoded_msg));

  InputEvent received_event;
  ASSERT_TRUE(received_event.ParseFromString(decoded_msg));

  EXPECT_EQ(received_event.timestamp_ms(), 2000);
  ASSERT_TRUE(received_event.has_keyboard_event());
  const auto& received_keyboard = received_event.keyboard_event();

  EXPECT_EQ(received_keyboard.action_type(), KeyboardEvent::KEY_DOWN);
  EXPECT_EQ(received_keyboard.key_code(), "KeyA");
  EXPECT_EQ(received_keyboard.key_value(), "a");
  EXPECT_EQ(received_keyboard.repeat(), true);
  EXPECT_EQ(received_keyboard.alt_key_press(), false);
  EXPECT_EQ(received_keyboard.ctrl_key_press(), true);
  EXPECT_EQ(received_keyboard.shift_key_press(), false);
  EXPECT_EQ(received_keyboard.meta_key_press(), true);
  EXPECT_EQ(received_keyboard.caps_lock_enabled(), true);
  EXPECT_EQ(received_keyboard.timestamp_ms(), 2001);
}

TEST_F(StreamingReceiverChannelTest, SerializeTouchEvent) {
  InputEvent event;
  event.set_timestamp_ms(3000);
  auto* touch_event = event.mutable_touch_event();
  touch_event->set_action_type(TouchEvent::TOUCH_MOVE);
  touch_event->set_alt_key_press(true);
  touch_event->set_ctrl_key_press(false);
  touch_event->set_shift_key_press(true);
  touch_event->set_meta_key_press(false);

  auto* touch1 = touch_event->add_touches();
  touch1->set_id(1);
  touch1->set_x_ratio(0.5f);
  touch1->set_y_ratio(0.6f);

  auto* touch2 = touch_event->add_touches();
  touch2->set_id(2);
  touch2->set_x_ratio(0.7f);
  touch2->set_y_ratio(0.8f);

  channel_->SendInputEvent(event);

  ASSERT_TRUE(event_receiver_.RunUntilMessageCountEqual(1));
  std::string received_msg = event_receiver_.buffer()[0].first;
  std::string decoded_msg;
  ASSERT_TRUE(base::Base64Decode(received_msg, &decoded_msg));

  InputEvent received_event;
  ASSERT_TRUE(received_event.ParseFromString(decoded_msg));

  EXPECT_EQ(received_event.timestamp_ms(), 3000);
  ASSERT_TRUE(received_event.has_touch_event());
  const auto& received_touch = received_event.touch_event();

  EXPECT_EQ(received_touch.action_type(), TouchEvent::TOUCH_MOVE);
  EXPECT_EQ(received_touch.alt_key_press(), true);
  EXPECT_EQ(received_touch.ctrl_key_press(), false);
  EXPECT_EQ(received_touch.shift_key_press(), true);
  EXPECT_EQ(received_touch.meta_key_press(), false);

  ASSERT_EQ(received_touch.touches_size(), 2);

  const auto& t1 = received_touch.touches(0);
  EXPECT_EQ(t1.id(), 1);
  EXPECT_NEAR(t1.x_ratio(), 0.5f, 1e-6);
  EXPECT_NEAR(t1.y_ratio(), 0.6f, 1e-6);

  const auto& t2 = received_touch.touches(1);
  EXPECT_EQ(t2.id(), 2);
  EXPECT_NEAR(t2.x_ratio(), 0.7f, 1e-6);
  EXPECT_NEAR(t2.y_ratio(), 0.8f, 1e-6);
}

TEST_F(StreamingReceiverChannelTest, SerializeKeyboardConfigurationChange) {
  InputEvent event;
  event.set_timestamp_ms(4000);
  auto* config = event.mutable_keyboard_configuration_change();
  config->set_ime_id("ime_id");
  config->set_ime_long_name("ime_long_name");
  config->set_ime_short_name("ime_short_name");
  config->set_ime_layout_name("ime_layout_name");

  channel_->SendInputEvent(event);

  ASSERT_TRUE(event_receiver_.RunUntilMessageCountEqual(1));
  std::string received_msg = event_receiver_.buffer()[0].first;
  std::string decoded_msg;
  ASSERT_TRUE(base::Base64Decode(received_msg, &decoded_msg));

  InputEvent received_event;
  ASSERT_TRUE(received_event.ParseFromString(decoded_msg));

  EXPECT_EQ(received_event.timestamp_ms(), 4000);
  ASSERT_TRUE(received_event.has_keyboard_configuration_change());
  const auto& received_config = received_event.keyboard_configuration_change();

  EXPECT_EQ(received_config.ime_id(), "ime_id");
  EXPECT_EQ(received_config.ime_long_name(), "ime_long_name");
  EXPECT_EQ(received_config.ime_short_name(), "ime_short_name");
  EXPECT_EQ(received_config.ime_layout_name(), "ime_layout_name");
}

TEST_F(StreamingReceiverChannelTest, SerializeInputCapabilities) {
  InputCapabilities capabilities;
  auto* device = capabilities.add_devices();
  device->set_device_id("123");
  device->set_display_name("Test Device");
  device->set_type(INPUT_TYPE_KEYBOARD);
  device->set_vendor_id(0x1111);
  device->set_product_id(0x2222);
  device->mutable_keyboard_metadata()->set_is_virtual(false);

  channel_->SendInputCapabilities(capabilities);

  ASSERT_TRUE(capabilities_receiver_.RunUntilMessageCountEqual(1));
  std::string received_msg = capabilities_receiver_.buffer()[0].first;
  std::string decoded_msg;
  ASSERT_TRUE(base::Base64Decode(received_msg, &decoded_msg));

  InputCapabilities received_caps;
  ASSERT_TRUE(received_caps.ParseFromString(decoded_msg));

  ASSERT_EQ(received_caps.devices_size(), 1);
  const auto& received_device = received_caps.devices(0);
  EXPECT_EQ(received_device.device_id(), "123");
  EXPECT_EQ(received_device.display_name(), "Test Device");
  EXPECT_EQ(received_device.type(), INPUT_TYPE_KEYBOARD);
  EXPECT_EQ(received_device.vendor_id(), 0x1111);
  EXPECT_EQ(received_device.product_id(), 0x2222);
  EXPECT_FALSE(received_device.keyboard_metadata().is_virtual());
}

}  // namespace cast_receiver
