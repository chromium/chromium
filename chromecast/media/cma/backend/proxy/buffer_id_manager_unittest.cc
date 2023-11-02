// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/monotonic_clock.h"
#include "chromecast/media/api/test/mock_cma_backend.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using testing::_;
using testing::Mock;
using testing::Return;

ACTION_P(CompareBufferInfos, expected) {
  const BufferIdManager::TargetBufferInfo& actual = arg0;
  EXPECT_EQ(actual.buffer_id, expected.buffer_id);
  EXPECT_EQ(actual.timestamp_micros, expected.timestamp_micros);
}

class MockClock : public MonotonicClock {
 public:
  ~MockClock() override = default;

  MOCK_CONST_METHOD0(Now, int64_t());
};

class MockBufferIdManagerClient : public BufferIdManager::Client {
 public:
  MOCK_METHOD1(OnTimestampUpdateNeeded,
               void(BufferIdManager::TargetBufferInfo));
};

}  // namespace

class BufferIdManagerTest : public testing::Test {
 public:
  BufferIdManagerTest() {
    default_config_.bytes_per_channel = 100;
    default_config_.channel_number = 20;
    default_config_.samples_per_second = 5000;

    auto clock = std::make_unique<testing::StrictMock<MockClock>>();
    clock_ = clock.get();
    id_manager_ = base::WrapUnique<BufferIdManager>(
        new BufferIdManager(&audio_decoder_, &client_, std::move(clock)));
  }
  ~BufferIdManagerTest() override = default;

 protected:
  BufferIdManager::BufferId AssignBufferId(int64_t rendering_delay,
                                           int64_t timestamp,
                                           int64_t pts) {
    EXPECT_CALL(audio_decoder_, GetRenderingDelay())
        .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
            rendering_delay, timestamp)));
    auto buffer = base::MakeRefCounted<CastDecoderBufferImpl>(1);
    buffer->set_timestamp(base::Microseconds(pts));
    auto result = id_manager_->AssignBufferId(*buffer);
    Mock::VerifyAndClearExpectations(&audio_decoder_);
    return result;
  }

  BufferIdManager::TargetBufferInfo GetCurrentlyProcessingBufferInfo(
      int64_t rendering_delay,
      int64_t timestamp,
      BufferIdManager::BufferId target_buffer) {
    auto buffer_info = id_manager_->GetCurrentlyProcessingBufferInfo();
    EXPECT_EQ(buffer_info.buffer_id, target_buffer);
    EXPECT_EQ(buffer_info.timestamp_micros, timestamp);
    Mock::VerifyAndClearExpectations(&audio_decoder_);
    return buffer_info;
  }

  testing::StrictMock<MockCmaBackend::AudioDecoder> audio_decoder_;
  testing::StrictMock<MockBufferIdManagerClient> client_;
  MockClock* clock_;
  std::unique_ptr<BufferIdManager> id_manager_;

  AudioConfig default_config_;
};

TEST_F(BufferIdManagerTest, TestDelayedFrames) {
  // Push 3 new buffers.
  EXPECT_CALL(*clock_, Now()).Times(3).WillRepeatedly(Return(0));
  BufferIdManager::BufferId i =
      AssignBufferId(0, std::numeric_limits<int64_t>::min(), 30);
  ASSERT_GE(i, 0);

  BufferIdManager::BufferId j =
      AssignBufferId(0, std::numeric_limits<int64_t>::min(), 70);
  EXPECT_EQ(i + 1, j);

  BufferIdManager::BufferId k =
      AssignBufferId(0, std::numeric_limits<int64_t>::min(), 120);
  EXPECT_EQ(j + 1, k);
  Mock::VerifyAndClearExpectations(clock_);

  // Call UpdateAndGetCurrentlyProcessingBufferInfo() pulling no buffers off the
  // queue.
  EXPECT_CALL(*clock_, Now()).WillOnce(Return(0));
  GetCurrentlyProcessingBufferInfo(100, 0, i);
  Mock::VerifyAndClearExpectations(clock_);

  int64_t rendering_delay = 100;
  int64_t renderer_timestamp = 0;
  int64_t returned_timestamp = 10;

  EXPECT_CALL(client_, OnTimestampUpdateNeeded(_))
      .WillOnce(testing::WithArgs<0>(CompareBufferInfos(
          BufferIdManager::TargetBufferInfo{i, returned_timestamp})));
  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
          rendering_delay, renderer_timestamp)));
  auto buffer_info = id_manager_->UpdateAndGetCurrentlyProcessingBufferInfo();
  EXPECT_EQ(buffer_info.buffer_id, i);
  EXPECT_EQ(buffer_info.timestamp_micros, returned_timestamp);
  Mock::VerifyAndClearExpectations(&audio_decoder_);
  Mock::VerifyAndClearExpectations(&client_);

  // No buffers should be pulled off when calling
  // GetCurrentlyProcessingBufferInfo() regardless of timestamp change.
  rendering_delay = 80;
  GetCurrentlyProcessingBufferInfo(rendering_delay, returned_timestamp, i);
  Mock::VerifyAndClearExpectations(clock_);

  // Pull the first buffer off the queue, and expect no callback because there's
  // been no rendering clock changes.
  renderer_timestamp = 20;
  returned_timestamp = 50;

  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
          rendering_delay, renderer_timestamp)));
  buffer_info = id_manager_->UpdateAndGetCurrentlyProcessingBufferInfo();
  EXPECT_EQ(buffer_info.buffer_id, j);
  EXPECT_EQ(buffer_info.timestamp_micros, returned_timestamp);
  Mock::VerifyAndClearExpectations(&audio_decoder_);

  // Pull the last buffer off the queue
  rendering_delay = 30;
  GetCurrentlyProcessingBufferInfo(rendering_delay, returned_timestamp, j);

  renderer_timestamp = 70;
  returned_timestamp = 100;
  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
          rendering_delay, renderer_timestamp)));
  buffer_info = id_manager_->UpdateAndGetCurrentlyProcessingBufferInfo();
  EXPECT_EQ(buffer_info.buffer_id, k);
  EXPECT_EQ(buffer_info.timestamp_micros, returned_timestamp);
  Mock::VerifyAndClearExpectations(&audio_decoder_);

  // When no buffer is remaining in the queue, return the id of the last
  // processed.
  rendering_delay = 0;
  GetCurrentlyProcessingBufferInfo(rendering_delay, returned_timestamp, k);

  renderer_timestamp = 100;
  returned_timestamp = 100;
  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
          rendering_delay, renderer_timestamp)));
  buffer_info = id_manager_->UpdateAndGetCurrentlyProcessingBufferInfo();
  EXPECT_EQ(buffer_info.buffer_id, k);
  EXPECT_EQ(buffer_info.timestamp_micros, returned_timestamp);
}

TEST_F(BufferIdManagerTest, TestUpdateTimestamp) {
  auto buffer = base::MakeRefCounted<CastDecoderBufferImpl>(1);

  // Push a new buffer.
  int64_t rendering_delay = 0;
  int64_t pts = 0;
  EXPECT_CALL(*clock_, Now()).WillOnce(Return(0));
  BufferIdManager::BufferId i =
      AssignBufferId(0, std::numeric_limits<int64_t>::min(), pts);
  EXPECT_GE(i, 0);
  Mock::VerifyAndClearExpectations(clock_);

  // Push a second buffer, with the first one still pending.
  rendering_delay = 5;
  int64_t renderer_timestamp = 20;
  pts = 10;
  EXPECT_CALL(client_, OnTimestampUpdateNeeded(_)).Times(1);
  AssignBufferId(rendering_delay, renderer_timestamp, pts);
  Mock::VerifyAndClearExpectations(&client_);

  // When only 5 microseconds left in the queue, the first pushed buffer is
  // removed.
  rendering_delay = 15;
  renderer_timestamp = 30;
  pts = 20;
  AssignBufferId(rendering_delay, renderer_timestamp, pts);
  Mock::VerifyAndClearExpectations(&client_);

  // When a big change is detected, a client callback is sent. With only one
  // element in the queue, the rendering delay retrieved from the underlying
  // renderer is treated as exact.
  rendering_delay = 5;
  renderer_timestamp = 30000;
  pts = 40;
  EXPECT_CALL(client_, OnTimestampUpdateNeeded(_))
      .WillOnce(testing::WithArgs<0>(
          CompareBufferInfos(BufferIdManager::TargetBufferInfo{
              i + 2, renderer_timestamp + rendering_delay})));
  AssignBufferId(rendering_delay, renderer_timestamp, pts);
  Mock::VerifyAndClearExpectations(&client_);

  // Push more buffers with none falling off the queue. No client callback
  // should be sent.
  rendering_delay = 15;
  renderer_timestamp += 1;
  pts += 10;
  AssignBufferId(rendering_delay, renderer_timestamp, pts);

  rendering_delay = 25;
  renderer_timestamp += 1;
  pts += 10;
  AssignBufferId(rendering_delay, renderer_timestamp, pts);

  rendering_delay = 35;
  renderer_timestamp += 1;
  pts += 10;
  auto last_buffer_id =
      AssignBufferId(rendering_delay, renderer_timestamp, pts);

  // During the next iteration, if multiple buffers are removed from the queue
  // when a timestamp update is needed, only update for the last one.
  rendering_delay = 5;
  renderer_timestamp = 1500000;
  EXPECT_CALL(client_, OnTimestampUpdateNeeded(_))
      .WillOnce(testing::WithArgs<0>(
          CompareBufferInfos(BufferIdManager::TargetBufferInfo{
              last_buffer_id, renderer_timestamp + rendering_delay})));
  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
          rendering_delay, renderer_timestamp)));
  auto buffer_info = id_manager_->UpdateAndGetCurrentlyProcessingBufferInfo();
  EXPECT_EQ(buffer_info.buffer_id, last_buffer_id);
  EXPECT_EQ(buffer_info.timestamp_micros, renderer_timestamp + rendering_delay);
  Mock::VerifyAndClearExpectations(&audio_decoder_);
}

}  // namespace media
}  // namespace chromecast
