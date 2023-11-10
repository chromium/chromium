// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/push_buffer_pending_handler.h"

#include <cstring>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/backend/proxy/audio_channel_push_buffer_handler.h"
#include "chromecast/public/media/decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::WithArgs;

ACTION_P(ComparePushBuffers, expected) {
  const AudioChannelPushBufferHandler::PushBufferRequest& result = arg0;

  ASSERT_EQ(expected.has_buffer(), result.has_buffer());
  ASSERT_EQ(expected.has_audio_config(), result.has_audio_config());

  if (expected.has_buffer()) {
    EXPECT_EQ(expected.buffer().pts_micros(), result.buffer().pts_micros());
    EXPECT_EQ(expected.buffer().data(), result.buffer().data());
    EXPECT_EQ(expected.buffer().end_of_stream(),
              result.buffer().end_of_stream());
    EXPECT_EQ(expected.buffer().id(), result.buffer().id());
  }

  if (expected.has_audio_config()) {
    EXPECT_EQ(expected.audio_config().codec(), result.audio_config().codec());
    EXPECT_EQ(expected.audio_config().channel_layout(),
              result.audio_config().channel_layout());
    EXPECT_EQ(expected.audio_config().sample_format(),
              result.audio_config().sample_format());
    EXPECT_EQ(expected.audio_config().bytes_per_channel(),
              result.audio_config().bytes_per_channel());
    EXPECT_EQ(expected.audio_config().channel_number(),
              result.audio_config().channel_number());
    EXPECT_EQ(expected.audio_config().samples_per_second(),
              result.audio_config().samples_per_second());
    EXPECT_EQ(expected.audio_config().extra_data(),
              result.audio_config().extra_data());
  }
}

class MockClient : public AudioChannelPushBufferHandler::Client {
 public:
  ~MockClient() override = default;

  MOCK_METHOD1(OnAudioChannelPushBufferComplete,
               void(CmaBackend::BufferStatus));
};

class MockHandler : public AudioChannelPushBufferHandler {
 public:
  ~MockHandler() override = default;

  MOCK_METHOD1(PushBuffer, CmaBackend::BufferStatus(const PushBufferRequest&));
  MOCK_CONST_METHOD0(HasBufferedData, bool());
  MOCK_METHOD0(GetBufferedData, std::optional<PushBufferRequest>());
};

}  // namespace

class PushBufferPendingHandlerTest : public testing::Test {
 public:
  PushBufferPendingHandlerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        chromecast_task_runner_(task_runner_),
        handler_temp_storage(
            std::make_unique<testing::StrictMock<MockHandler>>()),
        handler_(handler_temp_storage.get()),
        push_buffer_helper_(&chromecast_task_runner_,
                            &client_,
                            std::move(handler_temp_storage)) {
    buffer_.mutable_buffer()->set_pts_micros(3);
    buffer_.mutable_buffer()->set_id(42);

    second_buffer_.mutable_buffer()->set_pts_micros(7);
    second_buffer_.mutable_buffer()->set_id(24);

    config_before_.mutable_audio_config()->set_channel_number(42);

    config_after_.mutable_audio_config()->set_channel_number(124815);
  }

  ~PushBufferPendingHandlerTest() override = default;

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TaskRunnerImpl chromecast_task_runner_;
  testing::StrictMock<MockClient> client_;
  std::unique_ptr<testing::StrictMock<MockHandler>> handler_temp_storage;
  testing::StrictMock<MockHandler>* handler_;
  PushBufferPendingHandler push_buffer_helper_;

  AudioChannelPushBufferHandler::PushBufferRequest buffer_;
  AudioChannelPushBufferHandler::PushBufferRequest second_buffer_;
  AudioChannelPushBufferHandler::PushBufferRequest config_before_;
  AudioChannelPushBufferHandler::PushBufferRequest config_after_;
};

TEST_F(PushBufferPendingHandlerTest, PushBufferPassesThroughCorrectly) {
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)));

  EXPECT_EQ(push_buffer_helper_.PushBuffer(buffer_),
            CmaBackend::BufferStatus::kBufferSuccess);
  EXPECT_FALSE(push_buffer_helper_.IsCallPending());

  task_runner_->RunUntilIdle();
}

TEST_F(PushBufferPendingHandlerTest, CallsPassThroughCorrectly) {
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)));
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_before_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(buffer_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_after_),
            CmaBackend::BufferStatus::kBufferPending);

  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)));
  task_runner_->RunPendingTasks();

  EXPECT_EQ(push_buffer_helper_.PushBuffer(second_buffer_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_CALL(client_, OnAudioChannelPushBufferComplete(
                           CmaBackend::BufferStatus::kBufferSuccess));
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(second_buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)));
  task_runner_->RunUntilIdle();
}

TEST_F(PushBufferPendingHandlerTest, CallsContinueTryingUntilSuccessful) {
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)));
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_before_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(buffer_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_after_),
            CmaBackend::BufferStatus::kBufferPending);

  EXPECT_CALL(client_, OnAudioChannelPushBufferComplete(
                           CmaBackend::BufferStatus::kBufferSuccess));
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)));
  task_runner_->RunUntilIdle();
}

TEST_F(PushBufferPendingHandlerTest,
       CallingSetConfigMultipleTimesBeforePushWorks) {
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)));

  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_before_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_after_),
            CmaBackend::BufferStatus::kBufferPending);

  EXPECT_TRUE(push_buffer_helper_.IsCallPending());

  EXPECT_CALL(client_, OnAudioChannelPushBufferComplete(
                           CmaBackend::BufferStatus::kBufferSuccess));
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)));
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(push_buffer_helper_.IsCallPending());
}

TEST_F(PushBufferPendingHandlerTest,
       CallingSetConfigMultipleTimesAfterPushWorks) {
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferFailed)));

  EXPECT_EQ(push_buffer_helper_.PushBuffer(buffer_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_before_),
            CmaBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(push_buffer_helper_.PushBuffer(config_after_),
            CmaBackend::BufferStatus::kBufferPending);

  EXPECT_CALL(client_, OnAudioChannelPushBufferComplete(
                           CmaBackend::BufferStatus::kBufferSuccess));
  EXPECT_CALL(*handler_, PushBuffer(_))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(buffer_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_before_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)))
      .WillOnce(DoAll(WithArgs<0>(ComparePushBuffers(config_after_)),
                      Return(CmaBackend::BufferStatus::kBufferSuccess)));

  task_runner_->RunUntilIdle();
}

}  // namespace media
}  // namespace chromecast
