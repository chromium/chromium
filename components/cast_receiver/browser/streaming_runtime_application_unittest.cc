// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_runtime_application.h"

#include "base/test/mock_callback.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/browser/public/streaming_config_manager.h"
#include "components/cast_receiver/browser/streaming_receiver_channel.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  // We expect both Cast Transport and Exo Bootstrap to be connected during
  // Launch.
  EXPECT_CALL(message_port_service_, ConnectToPortAsync(_, _)).Times(0);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .Times(1);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("urn:x-cast:com.google.cast.exo.bootstrap", _))
      .Times(1);

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());
}

TEST_F(StreamingRuntimeApplicationTest, LaunchWithExtendedInputNoDataManager) {
  // Do NOT call InitializeDeviceDataManager() here.

  ApplicationConfig app_config;
  app_config.is_extended_input_supported = true;

  StreamingRuntimeApplication app("test_session", std::move(app_config),
                                  app_client_);
  app.SetEmbedderApplication(embedder_app_);

  // We expect both Cast Transport and Exo Bootstrap to be connected during
  // Launch.
  EXPECT_CALL(message_port_service_, ConnectToPortAsync(_, _)).Times(0);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("cast.__platform__.cast_transport", _))
      .Times(1);
  EXPECT_CALL(message_port_service_,
              ConnectToPortAsync("urn:x-cast:com.google.cast.exo.bootstrap", _))
      .Times(1);

  base::MockCallback<RuntimeApplication::StatusCallback> callback;
  EXPECT_CALL(callback, Run(_));
  static_cast<RuntimeApplication&>(app).Launch(callback.Get());
}

}  // namespace cast_receiver
