// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_session_client.h"

#include "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "components/cast_receiver/browser/streaming_controller.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace cast_receiver {
namespace {

class MockStreamingController : public StreamingController {
 public:
  ~MockStreamingController() override = default;

  MOCK_METHOD2(InitializeReceiverSession,
               void(cast_streaming::ReceiverConfig,
                    cast_streaming::ReceiverSession::Client*));
  MOCK_METHOD1(StartPlaybackAsync,
               void(StreamingController::PlaybackStartedCB));
};

class MockStreamingReceiverSessionHandler
    : public StreamingReceiverSessionClient::Handler {
 public:
  ~MockStreamingReceiverSessionHandler() override = default;

  MOCK_METHOD0(OnStreamingSessionStarted, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD2(OnResolutionChanged,
               void(const gfx::Rect&, const ::media::VideoTransformation&));
};

class MockStreamingConfigManager
    : public cast_receiver::StreamingConfigManager {
 public:
  void SetConfig(cast_streaming::ReceiverConfig config) {
    OnStreamingConfigSet(std::move(config));
  }
};

}  // namespace

class StreamingReceiverSessionClientTest : public testing::Test {
 public:
  StreamingReceiverSessionClientTest() {
    // NOTE: Required to ensure this test suite isn't affected by use of this
    // static function elsewhere in the codebase's tests.
    cast_streaming::ClearNetworkContextGetter();

    auto streaming_controller =
        std::make_unique<StrictMock<MockStreamingController>>();
    streaming_controller_ = streaming_controller.get();

    // Note: Can't use make_unique<> because the private ctor is needed.
    auto* client = new StreamingReceiverSessionClient(
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            []() -> network::mojom::NetworkContext* { return nullptr; }),
        std::move(streaming_controller), &handler_, &config_manager_, true,
        true);
    receiver_session_client_.reset(client);
  }

  ~StreamingReceiverSessionClientTest() override {
    task_environment_.FastForwardBy(
        StreamingReceiverSessionClient::kMaxAVSettingsWaitTime);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockStreamingReceiverSessionHandler> handler_;
  StrictMock<MockStreamingConfigManager> config_manager_;
  StrictMock<MockStreamingController>* streaming_controller_;
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;
};

TEST_F(StreamingReceiverSessionClientTest, FailureWhenNoConfigAfterLaunch) {
  EXPECT_FALSE(receiver_session_client_->is_streaming_launch_pending());
  EXPECT_FALSE(receiver_session_client_->has_streaming_launched());
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());

  EXPECT_CALL(*streaming_controller_, StartPlaybackAsync(_));
  receiver_session_client_->LaunchStreamingReceiverAsync();

  EXPECT_TRUE(receiver_session_client_->is_streaming_launch_pending());
  EXPECT_FALSE(receiver_session_client_->has_streaming_launched());
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());

  EXPECT_CALL(handler_, OnError());
  task_environment_.FastForwardBy(
      StreamingReceiverSessionClient::kMaxAVSettingsWaitTime);
}

}  // namespace cast_receiver
