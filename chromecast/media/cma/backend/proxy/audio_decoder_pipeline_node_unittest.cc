// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/audio_decoder_pipeline_node.h"

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

class AudioDecoderPipelineNodeTests : public testing::Test {
 public:
  AudioDecoderPipelineNodeTests()
      : parent_delegate_(
            std::make_unique<
                testing::StrictMock<MockCmaBackend::DecoderDelegate>>()),
        child_node_(std::make_unique<
                    testing::StrictMock<MockCmaBackend::AudioDecoder>>()) {}

  ~AudioDecoderPipelineNodeTests() override = default;

 protected:
  std::unique_ptr<MockCmaBackend::DecoderDelegate> parent_delegate_;
  std::unique_ptr<MockCmaBackend::AudioDecoder> child_node_;
};

TEST_F(AudioDecoderPipelineNodeTests, TestAudioDecoderCalls) {
  AudioDecoderPipelineNode test_node(child_node_.get());

  // SetDelegate.
  EXPECT_CALL(*child_node_, SetDelegate(&test_node));
  test_node.SetDelegate(parent_delegate_.get());
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // PushBuffer.
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(3, StreamId::kPrimary));
  buffer->writable_data()[0] = 1;
  buffer->writable_data()[1] = 2;
  buffer->writable_data()[2] = 3;
  EXPECT_CALL(*child_node_, PushBuffer(testing::_))
      .WillOnce(testing::Invoke([ptr = buffer.get()](auto result)
                                    -> CmaBackend::Decoder::BufferStatus {
        EXPECT_EQ(ptr, result.get());
        return CmaBackend::Decoder::BufferStatus::kBufferSuccess;
      }));
  EXPECT_EQ(test_node.PushBuffer(buffer),
            CmaBackend::Decoder::BufferStatus::kBufferSuccess);
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // SetConfig.
  AudioConfig config;
  EXPECT_CALL(*child_node_, SetConfig(testing::_))
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(test_node.SetConfig(config), true);
  EXPECT_EQ(test_node.SetConfig(config), false);
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // SetVolume.
  EXPECT_CALL(*child_node_, SetVolume(42)).WillOnce(testing::Return(true));
  EXPECT_EQ(test_node.SetVolume(42), true);
  EXPECT_CALL(*child_node_, SetVolume(23)).WillOnce(testing::Return(false));
  EXPECT_EQ(test_node.SetVolume(23), false);
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // GetStatistics
  CmaBackend::AudioDecoder::Statistics stats;
  EXPECT_CALL(*child_node_, GetStatistics(&stats));
  test_node.GetStatistics(&stats);
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // RequiresDecryption.
  EXPECT_CALL(*child_node_, RequiresDecryption())
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(test_node.RequiresDecryption(), true);
  EXPECT_EQ(test_node.RequiresDecryption(), false);
  testing::Mock::VerifyAndClearExpectations(child_node_.get());
}

TEST_F(AudioDecoderPipelineNodeTests, TestDecoderDelegateCalls) {
  AudioDecoderPipelineNode test_node(child_node_.get());
  auto* node_as_delegate =
      static_cast<CmaBackend::Decoder::Delegate*>(&test_node);
  EXPECT_CALL(*child_node_, SetDelegate(&test_node));
  test_node.SetDelegate(parent_delegate_.get());
  testing::Mock::VerifyAndClearExpectations(child_node_.get());

  // OnPushBufferComplete.
  EXPECT_CALL(
      *parent_delegate_,
      OnPushBufferComplete(CmaBackend::Decoder::BufferStatus::kBufferSuccess));
  node_as_delegate->OnPushBufferComplete(
      CmaBackend::Decoder::BufferStatus::kBufferSuccess);
  testing::Mock::VerifyAndClearExpectations(parent_delegate_.get());
  EXPECT_CALL(
      *parent_delegate_,
      OnPushBufferComplete(CmaBackend::Decoder::BufferStatus::kBufferFailed));
  node_as_delegate->OnPushBufferComplete(
      CmaBackend::Decoder::BufferStatus::kBufferFailed);

  // OnEndOfStream.
  EXPECT_CALL(*parent_delegate_, OnEndOfStream());
  node_as_delegate->OnEndOfStream();
  testing::Mock::VerifyAndClearExpectations(parent_delegate_.get());

  // OnDecoderError.
  EXPECT_CALL(*parent_delegate_, OnDecoderError());
  node_as_delegate->OnDecoderError();
  testing::Mock::VerifyAndClearExpectations(parent_delegate_.get());

  // OnKeyStatusChanged.
  std::string key = "foobar";
  CastKeyStatus key_status = CastKeyStatus::KEY_STATUS_EXPIRED;
  uint32_t system_code = 42;
  EXPECT_CALL(*parent_delegate_,
              OnKeyStatusChanged(key, key_status, system_code));
  node_as_delegate->OnKeyStatusChanged(key, key_status, system_code);
  testing::Mock::VerifyAndClearExpectations(parent_delegate_.get());

  // OnVideoResolutionChanged.
  Size size(12, 27);
  EXPECT_CALL(*parent_delegate_, OnVideoResolutionChanged(testing::_));
  node_as_delegate->OnVideoResolutionChanged(size);
  testing::Mock::VerifyAndClearExpectations(parent_delegate_.get());
}

}  // namespace media
}  // namespace chromecast
