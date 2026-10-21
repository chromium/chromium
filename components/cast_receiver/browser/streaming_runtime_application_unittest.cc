// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_runtime_application.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/browser/public/streaming_config_manager.h"
#include "components/cast_receiver/browser/streaming_receiver_channel.h"
#include "components/cast_receiver/proto/display_info.pb.h"
#include "components/cast_receiver/proto/exo_bootstrap.pb.h"
#include "components/cast_streaming/common/message_serialization.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/devices/device_data_manager.h"

using testing::_;
using testing::NiceMock;
using testing::StrictMock;

namespace cast_receiver {
namespace {

class MockEmbedderApplication : public EmbedderApplication {
 public:
  MOCK_METHOD0(NotifyApplicationStarted, void());
  MOCK_METHOD2(NotifyApplicationStopped, void(ApplicationStopReason, int32_t));
  MOCK_METHOD1(NotifyMediaPlaybackChanged, void(bool));
  MOCK_METHOD1(GetAllBindings, void(GetAllBindingsCallback));
  MOCK_METHOD0(GetMessagePortService, MessagePortService*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD0(GetContentWindowControls, ContentWindowControls*());
  MOCK_METHOD0(GetStreamingConfigManager, StreamingConfigManager*());
};

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

class MockContentWindowControls : public ContentWindowControls {
 public:
  MOCK_METHOD0(ShowWindow, void());
  MOCK_METHOD0(HideWindow, void());
  MOCK_METHOD0(EnableTouchInput, void());
  MOCK_METHOD0(DisableTouchInput, void());
};

class TestScreenWithViewOverride : public display::test::TestScreen {
 public:
  using TestScreen::TestScreen;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override {
    return GetPrimaryDisplay();
  }
};

class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(display::Screen* new_screen)
      : old_screen_(display::Screen::SetScreenInstance(nullptr)) {
    display::Screen::SetScreenInstance(new_screen);
  }
  ScopedScreenOverride(const ScopedScreenOverride&) = delete;
  ScopedScreenOverride& operator=(const ScopedScreenOverride&) = delete;
  ~ScopedScreenOverride() {
    display::Screen::SetScreenInstance(nullptr);
    if (old_screen_) {
      display::Screen::SetScreenInstance(old_screen_);
    }
  }

 private:
  raw_ptr<display::Screen> old_screen_;
};

}  // namespace

class StreamingRuntimeApplicationTest
    : public content::RenderViewHostTestHarness {
 protected:
  StreamingRuntimeApplicationTest()
      : app_client_(base::BindRepeating(
            []() -> network::mojom::NetworkContext* { return nullptr; })) {
    ON_CALL(embedder_app_, GetWebContents()).WillByDefault([this]() {
      return web_contents();
    });
    ON_CALL(embedder_app_, GetMessagePortService())
        .WillByDefault(testing::Return(&message_port_service_));
    ON_CALL(embedder_app_, GetContentWindowControls())
        .WillByDefault(testing::Return(&window_controls_));
    ON_CALL(embedder_app_, GetStreamingConfigManager())
        .WillByDefault(testing::Return(&config_manager_));
  }

  ~StreamingRuntimeApplicationTest() override {
    if (created_data_manager_) {
      ui::DeviceDataManager::DeleteInstance();
    }
  }

  void InitializeDeviceDataManager() {
    if (!ui::DeviceDataManager::HasInstance()) {
      ui::DeviceDataManager::CreateInstance();
      created_data_manager_ = true;
    }
  }

  ApplicationClient app_client_;
  NiceMock<MockEmbedderApplication> embedder_app_;
  NiceMock<MockMessagePortService> message_port_service_;
  NiceMock<MockContentWindowControls> window_controls_;
  StreamingConfigManager config_manager_;
  bool created_data_manager_ = false;
};

TEST_F(StreamingRuntimeApplicationTest, LaunchWithoutExtendedInput) {
  ApplicationConfig app_config;
  app_config.is_extended_input_supported = false;

  StreamingRuntimeApplication app("test_session", std::move(app_config),
                                  app_client_);
  app.SetEmbedderApplication(embedder_app_);

  // We expect only Cast Transport to be connected.
  EXPECT_CALL(message_port_service_, ConnectToPortAsync(_, _)).Times(0);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .Times(1);

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());
}

TEST_F(StreamingRuntimeApplicationTest, LaunchWithExtendedInput) {
  InitializeDeviceDataManager();

  ApplicationConfig app_config;
  app_config.is_extended_input_supported = true;

  StreamingRuntimeApplication app("test_session", std::move(app_config),
                                  app_client_);
  app.SetEmbedderApplication(embedder_app_);

  // With port multiplexing, only cast_transport is connected.
  EXPECT_CALL(message_port_service_, ConnectToPortAsync(_, _)).Times(0);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .Times(1);

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());

  std::string nav_url = web_contents()->GetVisibleURL().spec();
  EXPECT_THAT(nav_url, testing::HasSubstr("touch-action:none"));
  EXPECT_THAT(nav_url, testing::HasSubstr("user-scalable=no"));
  EXPECT_THAT(nav_url, testing::HasSubstr("object-fit:contain"));
  EXPECT_THAT(nav_url, testing::Not(testing::HasSubstr("object-fit:fill")));
}

TEST_F(StreamingRuntimeApplicationTest, LaunchWithExtendedInputNoDataManager) {
  // Do NOT call InitializeDeviceDataManager() here.

  ApplicationConfig app_config;
  app_config.is_extended_input_supported = true;

  StreamingRuntimeApplication app("test_session", std::move(app_config),
                                  app_client_);
  app.SetEmbedderApplication(embedder_app_);

  // With port multiplexing, only cast_transport is connected.
  EXPECT_CALL(message_port_service_, ConnectToPortAsync(_, _)).Times(0);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .Times(1);

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());
}

TEST_F(StreamingRuntimeApplicationTest,
       LaunchWithExtendedInputPopulatesPhysicalDisplayInfo) {
  InitializeDeviceDataManager();

  display::Display custom_display(1);
  custom_display.set_bounds(gfx::Rect(0, 0, 1280, 800));
  custom_display.set_device_scale_factor(2.0f);
  custom_display.set_size_in_pixels(gfx::Size(2560, 1600));
  custom_display.set_rotation(display::Display::ROTATE_90);

  TestScreenWithViewOverride test_screen(/*create_display=*/true,
                                         /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);
  test_screen.display_list().UpdateDisplay(custom_display);

  ApplicationConfig app_config;
  app_config.is_extended_input_supported = true;

  StreamingRuntimeApplication app("test_session", std::move(app_config),
                                  app_client_);
  app.SetEmbedderApplication(embedder_app_);

  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .WillOnce([&](std::string_view port_name,
                    std::unique_ptr<cast_api_bindings::MessagePort> port) {
        client_port = std::move(port);
      });

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());

  ASSERT_TRUE(client_port);
  cast_api_bindings::TestMessagePortReceiver client_receiver;
  client_port->SetReceiver(&client_receiver);

  ExoBootstrapMessage request;
  request.set_session_id(42);
  request.mutable_bootstrap_action()->set_action(
      BootstrapAction::START_BOOTSTRAP);
  std::string serialized_request;
  ASSERT_TRUE(request.SerializeToString(&serialized_request));

  std::string cast_msg = cast_streaming::SerializeCastMessage(
      "test-sender", "urn:x-cast:com.google.cast.exo.bootstrap",
      base::Base64Encode(serialized_request));
  EXPECT_TRUE(client_port->PostMessage(cast_msg));

  ASSERT_TRUE(client_receiver.RunUntilMessageCountEqual(1));
  std::string sender_id;
  std::string message_namespace;
  std::string data;
  ASSERT_TRUE(cast_streaming::DeserializeCastMessage(
      client_receiver.buffer()[0].first, &sender_id, &message_namespace,
      &data));

  std::string decoded_response;
  ASSERT_TRUE(base::Base64Decode(data, &decoded_response));
  ExoBootstrapMessage response;
  ASSERT_TRUE(response.ParseFromString(decoded_response));

  const DisplayInfo& display_info =
      response.bootstrap_action().launch_info().display_info();
  EXPECT_EQ(display_info.width_px(), 2560);
  EXPECT_EQ(display_info.height_px(), 1600);
  EXPECT_EQ(display_info.dpi(), 320);
  EXPECT_EQ(display_info.orientation(), DisplayInfo::DEGREES_90);
}

}  // namespace cast_receiver
