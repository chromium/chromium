// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/media_pipeline_buffer_extension.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/api/test/mock_cma_backend.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "chromecast/public/media/cast_key_status.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/stream_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

using BufferStatus = CmaBackend::Decoder::BufferStatus;
using testing::_;
using testing::Mock;
using testing::Return;

class MediaPipelineBufferExtensionTests : public testing::Test {
 public:
  MediaPipelineBufferExtensionTests()
      : task_runner_(new base::TestSimpleTaskRunner()),
        chromecast_task_runner_(task_runner_),
        decoder_(std::make_unique<
                 testing::StrictMock<MockCmaBackend::AudioDecoder>>()),
        delegate_(std::make_unique<
                  testing::StrictMock<MockCmaBackend::DecoderDelegate>>()),
        buffer_(std::make_unique<MediaPipelineBufferExtension>(
            &chromecast_task_runner_,
            decoder_.get())) {
    EXPECT_CALL(*decoder_, SetDelegate(buffer_.get()));
    buffer_->SetDelegate(delegate_.get());
  }

  ~MediaPipelineBufferExtensionTests() override = default;

 protected:
  bool IsBufferEmpty() { return buffer_->IsBufferEmpty(); }

  void OnDecoderPushBufferComplete(BufferStatus status) {
    static_cast<CmaBackend::Decoder::Delegate*>(buffer_.get())
        ->OnPushBufferComplete(status);
    task_runner_->RunUntilIdle();
  }

  void ValidateRenderingDelay(int64_t delay_offset) {
    static constexpr int64_t kDelay = 12345;
    static constexpr int64_t kTimestamp = 678910;
    static CmaBackend::AudioDecoder::RenderingDelay decoder_delay(kDelay,
                                                                  kTimestamp);
    EXPECT_CALL(*decoder_, GetRenderingDelay()).WillOnce(Return(decoder_delay));

    auto calculated_delay = buffer_->GetRenderingDelay();
    EXPECT_EQ(calculated_delay.delay_microseconds, kDelay + delay_offset);
    EXPECT_EQ(calculated_delay.timestamp_microseconds, kTimestamp);
  }

  scoped_refptr<CastDecoderBufferImpl> CreateBuffer(int64_t pts_increment = 3) {
    scoped_refptr<CastDecoderBufferImpl> buffer(
        new CastDecoderBufferImpl(3, StreamId::kPrimary));
    buffer->set_timestamp(base::Microseconds(current_pts_));
    current_pts_ += pts_increment;
    return buffer;
  }

  AudioConfig CreateConfig() {
    AudioConfig config;
    config.bytes_per_channel = 17;
    config.channel_number = 3;
    config.samples_per_second = 42;
    return config;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TaskRunnerImpl chromecast_task_runner_;
  std::unique_ptr<MockCmaBackend::AudioDecoder> decoder_;
  std::unique_ptr<MockCmaBackend::DecoderDelegate> delegate_;
  std::unique_ptr<MediaPipelineBufferExtension> buffer_;

 private:
  uint64_t current_pts_ = 0;
};

TEST_F(MediaPipelineBufferExtensionTests, TestPushBufferOnSuccess) {
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .Times(4)
      .WillRepeatedly(Return(BufferStatus::kBufferSuccess));

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(IsBufferEmpty());
}

TEST_F(MediaPipelineBufferExtensionTests, TestPushBufferOnImmediateFailure) {
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferFailed));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferFailed);
}

TEST_F(MediaPipelineBufferExtensionTests, TestPushBufferDelayedFailure) {
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);

  OnDecoderPushBufferComplete(BufferStatus::kBufferFailed);
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferFailed);
}

TEST_F(MediaPipelineBufferExtensionTests, TestGetRenderingDelay) {
  // Calls with no data buffered are returned directly from the decoder.
  ValidateRenderingDelay(0);

  EXPECT_TRUE(buffer_->IsBufferEmpty());
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferSuccess));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(0);
  Mock::VerifyAndClearExpectations(decoder_.get());

  // The first push is buffered in the downstream decoder, not the queue.
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(0);
  Mock::VerifyAndClearExpectations(decoder_.get());

  // Following pushes are stored in the local queue.
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_FALSE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(3);

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_FALSE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(6);

  // When data is removed from the queue, the delay goes down.
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_FALSE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(3);
  Mock::VerifyAndClearExpectations(decoder_.get());

  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(0);
  Mock::VerifyAndClearExpectations(decoder_.get());

  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  ValidateRenderingDelay(0);
}

TEST_F(MediaPipelineBufferExtensionTests, QueueFillsAndEmptiesCorrectly) {
  EXPECT_TRUE(buffer_->IsBufferEmpty());

  // It should take 1668 pushes for the buffer to completely fill up.
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferSuccess));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer(3000)),
            BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  EXPECT_FALSE(buffer_->IsBufferFull());

  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer(3000)),
            BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  EXPECT_FALSE(buffer_->IsBufferFull());

  for (int i = 0; i < 1666; i++) {
    EXPECT_EQ(buffer_->PushBuffer(CreateBuffer(3000)),
              BufferStatus::kBufferSuccess);
    EXPECT_FALSE(buffer_->IsBufferEmpty());
    EXPECT_FALSE(buffer_->IsBufferFull());
  }

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer(3000)),
            BufferStatus::kBufferPending);
  EXPECT_TRUE(buffer_->IsBufferFull());

  Mock::VerifyAndClearExpectations(buffer_.get());

  // When one buffer is processed and another added, it's still full.
  EXPECT_CALL(*delegate_, OnPushBufferComplete(BufferStatus::kBufferSuccess));
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_FALSE(buffer_->IsBufferFull());
  Mock::VerifyAndClearExpectations(buffer_.get());
  Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer(3000)),
            BufferStatus::kBufferPending);
  EXPECT_TRUE(buffer_->IsBufferFull());
  Mock::VerifyAndClearExpectations(buffer_.get());

  // All data can successfully be emptied
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .Times(1667)
      .WillRepeatedly(Return(BufferStatus::kBufferPending));

  EXPECT_CALL(*delegate_, OnPushBufferComplete(BufferStatus::kBufferSuccess));
  for (int i = 0; i < 1666; i++) {
    OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
    EXPECT_FALSE(buffer_->IsBufferEmpty());
    EXPECT_FALSE(buffer_->IsBufferFull());
    Mock::VerifyAndClearExpectations(delegate_.get());
  }

  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  EXPECT_FALSE(buffer_->IsBufferFull());
}

TEST_F(MediaPipelineBufferExtensionTests, QueueEmptiesUntilPendingReceived) {
  EXPECT_TRUE(buffer_->IsBufferEmpty());

  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
  EXPECT_FALSE(buffer_->IsBufferFull());
  Mock::VerifyAndClearExpectations(decoder_.get());

  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()),
              BufferStatus::kBufferSuccess);
    EXPECT_FALSE(buffer_->IsBufferEmpty());
    EXPECT_FALSE(buffer_->IsBufferFull());
  }

  testing::InSequence s;
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .Times(50)
      .WillRepeatedly(Return(BufferStatus::kBufferSuccess));
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  Mock::VerifyAndClearExpectations(decoder_.get());

  EXPECT_CALL(*decoder_, PushBuffer(_))
      .Times(49)
      .WillRepeatedly(Return(BufferStatus::kBufferSuccess));
  OnDecoderPushBufferComplete(BufferStatus::kBufferSuccess);
  EXPECT_TRUE(buffer_->IsBufferEmpty());
}

TEST_F(MediaPipelineBufferExtensionTests, TestSetConfigOnSuccess) {
  EXPECT_CALL(*decoder_, SetConfig(_)).Times(4).WillRepeatedly(Return(true));

  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());
}

TEST_F(MediaPipelineBufferExtensionTests, SetConfigCalledAfterPushBufferStops) {
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferPending));
  EXPECT_CALL(*decoder_, SetConfig(_)).Times(2).WillRepeatedly(Return(true));

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_TRUE(IsBufferEmpty());

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferSuccess);
  EXPECT_FALSE(IsBufferEmpty());
  EXPECT_TRUE(buffer_->SetConfig(CreateConfig()));
  EXPECT_FALSE(IsBufferEmpty());
}

TEST_F(MediaPipelineBufferExtensionTests, FailedPushBufferBlocksCommands) {
  EXPECT_CALL(*decoder_, PushBuffer(_))
      .WillOnce(Return(BufferStatus::kBufferFailed));

  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferFailed);
  EXPECT_FALSE(buffer_->SetConfig(CreateConfig()));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferFailed);
}

TEST_F(MediaPipelineBufferExtensionTests, FailedSetConfigBlocksCommands) {
  EXPECT_CALL(*decoder_, SetConfig(_)).WillOnce(Return(false));

  EXPECT_FALSE(buffer_->SetConfig(CreateConfig()));
  EXPECT_EQ(buffer_->PushBuffer(CreateBuffer()), BufferStatus::kBufferFailed);
  EXPECT_FALSE(buffer_->SetConfig(CreateConfig()));
}

}  // namespace media
}  // namespace chromecast
