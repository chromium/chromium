// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/api/cma_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

class MockAudioDecoder : public CmaBackend::AudioDecoder {
 public:
  ~MockAudioDecoder() override = default;

  MOCK_METHOD1(SetDelegate, void(CmaBackend::Decoder::Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(scoped_refptr<DecoderBufferBase>));
  MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
  MOCK_METHOD1(SetVolume, bool(float));
  MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
  MOCK_METHOD1(GetStatistics, void(Statistics*));
  MOCK_METHOD0(RequiresDecryption, bool());
  MOCK_METHOD1(SetObserver, void(Observer*));
};

}  // namespace

class BufferIdManagerTest : public testing::Test {
 public:
  BufferIdManagerTest()
      : id_manager_(std::make_unique<BufferIdManager>(&audio_decoder_)) {}
  ~BufferIdManagerTest() override = default;

 protected:
  testing::StrictMock<MockAudioDecoder> audio_decoder_;
  std::unique_ptr<BufferIdManager> id_manager_;
};

TEST_F(BufferIdManagerTest, TestDelayedFrames) {
  AudioConfig config;
  config.bytes_per_channel = 100;
  config.channel_number = 20;
  config.samples_per_second = 5000;
  id_manager_->SetAudioConfig(config);

  constexpr double kBytesPerMicrosecond = 10;
  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .Times(3)
      .WillRepeatedly(
          testing::Return(MediaPipelineBackend::AudioDecoder::RenderingDelay(
              1000 * kBytesPerMicrosecond, 0)));
  BufferIdManager::BufferId i =
      id_manager_->AssignBufferId(30 * kBytesPerMicrosecond);
  BufferIdManager::BufferId j =
      id_manager_->AssignBufferId(40 * kBytesPerMicrosecond);
  BufferIdManager::BufferId k =
      id_manager_->AssignBufferId(50 * kBytesPerMicrosecond);
  EXPECT_EQ(i + 1, j);
  EXPECT_EQ(j + 1, k);

  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(testing::Return(
          MediaPipelineBackend::AudioDecoder::RenderingDelay(150, 0)));
  EXPECT_EQ(id_manager_->GetCurrentlyProcessingBuffer(), i);

  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(testing::Return(
          MediaPipelineBackend::AudioDecoder::RenderingDelay(100, 0)));
  EXPECT_EQ(id_manager_->GetCurrentlyProcessingBuffer(), j);

  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(testing::Return(
          MediaPipelineBackend::AudioDecoder::RenderingDelay(60, 0)));
  EXPECT_EQ(id_manager_->GetCurrentlyProcessingBuffer(), k);

  EXPECT_CALL(audio_decoder_, GetRenderingDelay())
      .WillOnce(testing::Return(
          MediaPipelineBackend::AudioDecoder::RenderingDelay(0, 0)));
  EXPECT_EQ(id_manager_->GetCurrentlyProcessingBuffer(), k);
}

}  // namespace media
}  // namespace chromecast
