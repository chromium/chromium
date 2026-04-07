// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_streaming_session.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

class CastStreamingSessionTest : public testing::Test,
                                 public CastStreamingSession::Client {
 public:
  CastStreamingSessionTest() = default;
  ~CastStreamingSessionTest() override = default;

  // CastStreamingSession::Client implementation.
  void OnSessionInitialization(
      StreamingInitializationInfo initialization_info,
      std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
      std::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer)
      override {}

  void OnAudioBufferReceived(
      media::mojom::DecoderBufferPtr audio_buffer) override {}
  void OnVideoBufferReceived(
      media::mojom::DecoderBufferPtr video_buffer) override {}
  void OnSessionReinitialization(
      StreamingInitializationInfo initialization_info,
      std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
      std::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer)
      override {}
  void OnSessionReinitializationPending() override {}

  void OnSessionEnded() override {
    // Note: Emulates what real production code (e.g.
    // RuntimeApplicationServiceImpl) does!
    session_.reset();
  }

  void StartSession() {
    session_ = std::make_unique<CastStreamingSession>();

    std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
    std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
    cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port,
                                                     &receiver_message_port);

    // Bind the fake sender port to a test receiver to prevent memory leaks or
    // warnings
    sender_message_port_ = std::move(sender_message_port);
    sender_message_port_->SetReceiver(&sender_message_port_receiver_);

    session_->Start(
        this, std::nullopt, ReceiverConfig(),
        base::BindOnce(
            [](std::unique_ptr<cast_api_bindings::MessagePort> port) {
              return port;
            },
            std::move(receiver_message_port)),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

 protected:
  std::unique_ptr<CastStreamingSession> session_;
  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port_;
  cast_api_bindings::TestMessagePortReceiver sender_message_port_receiver_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
};

TEST_F(CastStreamingSessionTest, TestTeardownCrash) {
  StartSession();

  // Fast forward time by 16 seconds to trigger the no-data timeout.
  // This will call OnDataTimeout, which will call receiver_session_.reset(),
  // which will trigger ~ReceiverSession, which will trigger MessagePort close,
  // which will trigger OnCastChannelClosed re-entrantly,
  // which will call EndSession, which will delete session_, causing
  // use-after-free!
  task_environment_.FastForwardBy(base::Seconds(16));
}

}  // namespace cast_streaming
