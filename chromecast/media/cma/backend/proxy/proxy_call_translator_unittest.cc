// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/proxy_call_translator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/backend/proxy/audio_channel_push_buffer_handler.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/stream_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

ACTION_P(CompareTimestampInfos, buffer_id, timestamp) {
  const CastRuntimeAudioChannelBroker::TimestampInfo& result = arg0;
  EXPECT_EQ(result.buffer_id(), buffer_id);
  const int64_t micros =
      result.system_timestamp().seconds() * base::Time::kMicrosecondsPerSecond +
      result.system_timestamp().nanoseconds() /
          base::Time::kNanosecondsPerMicrosecond;
  EXPECT_EQ(micros, timestamp);
}

class MockTranslatorClient : public CmaProxyHandler::Client {
 public:
  ~MockTranslatorClient() override = default;

  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnPipelineStateChange, void(CmaProxyHandler::PipelineState));
  MOCK_METHOD1(OnBytesDecoded, void(int64_t));
  MOCK_METHOD2(OnMediaTimeUpdate, void(int64_t, base::TimeTicks));
  MOCK_METHOD0(OnEndOfStream, void());
};

class MockDecoderChannel : public CastRuntimeAudioChannelBroker {
 public:
  ~MockDecoderChannel() override = default;

  MOCK_METHOD2(
      InitializeAsync,
      void(const std::string&,
           CastRuntimeAudioChannelBroker::CastAudioDecoderMode decoder_mode));
  MOCK_METHOD1(SetVolumeAsync, void(float));
  MOCK_METHOD1(SetPlaybackAsync, void(double));
  MOCK_METHOD0(GetMediaTimeAsync, void());
  MOCK_METHOD2(StartAsync,
               void(int64_t, CastRuntimeAudioChannelBroker::TimestampInfo));
  MOCK_METHOD0(StopAsync, void());
  MOCK_METHOD0(PauseAsync, void());
  MOCK_METHOD1(ResumeAsync, void(CastRuntimeAudioChannelBroker::TimestampInfo));
  MOCK_METHOD1(UpdateTimestampAsync,
               void(CastRuntimeAudioChannelBroker::TimestampInfo));
};

class MockPushBufferHandler : public AudioChannelPushBufferHandler::Client {
 public:
  ~MockPushBufferHandler() override = default;

  MOCK_METHOD1(OnAudioChannelPushBufferComplete,
               void(CmaBackend::BufferStatus));
};

}  // namespace

class ProxyCallTranslatorTest : public testing::Test {
 public:
  ProxyCallTranslatorTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        chromecast_task_runner_(task_runner_),
        decoder_channel_not_owned_(
            std::make_unique<testing::StrictMock<MockDecoderChannel>>()),
        decoder_channel_(decoder_channel_not_owned_.get()),
        translator_(&chromecast_task_runner_,
                    &translator_client_,
                    &push_buffer_handler_,
                    std::move(decoder_channel_not_owned_)),
        translator_as_handler_(
            static_cast<CastRuntimeAudioChannelBroker::Handler*>(
                &translator_)) {}

  ~ProxyCallTranslatorTest() override = default;

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TaskRunnerImpl chromecast_task_runner_;
  testing::StrictMock<MockTranslatorClient> translator_client_;
  std::unique_ptr<testing::StrictMock<MockDecoderChannel>>
      decoder_channel_not_owned_;
  testing::StrictMock<MockDecoderChannel>* decoder_channel_;
  testing::StrictMock<MockPushBufferHandler> push_buffer_handler_;

  ProxyCallTranslator translator_;
  CastRuntimeAudioChannelBroker::Handler* translator_as_handler_;

  const CastRuntimeAudioChannelBroker::StatusCode failure_status_ =
      CastRuntimeAudioChannelBroker::StatusCode::kUnknown;
  const CastRuntimeAudioChannelBroker::StatusCode success_status_ =
      CastRuntimeAudioChannelBroker::StatusCode::kOk;
};

TEST_F(ProxyCallTranslatorTest, TestExternalInitialize) {
  std::string session_id = "foo";
  EXPECT_CALL(
      *decoder_channel_,
      InitializeAsync(session_id, cast::media::CAST_AUDIO_DECODER_MODE_ALL));
  translator_.Initialize(session_id,
                         CmaProxyHandler::AudioDecoderOperationMode::kAll);

  session_id = "bar";
  EXPECT_CALL(
      *decoder_channel_,
      InitializeAsync(session_id,
                      cast::media::CAST_AUDIO_DECODER_MODE_MULTIROOM_ONLY));
  translator_.Initialize(
      session_id, CmaProxyHandler::AudioDecoderOperationMode::kMultiroomOnly);

  session_id = "foobar";
  EXPECT_CALL(*decoder_channel_,
              InitializeAsync(session_id,
                              cast::media::CAST_AUDIO_DECODER_MODE_AUDIO_ONLY));
  translator_.Initialize(
      session_id, CmaProxyHandler::AudioDecoderOperationMode::kAudioOnly);
}

TEST_F(ProxyCallTranslatorTest, TestExternalStart) {
  constexpr int64_t start_pts = 42;
  BufferIdManager::TargetBufferInfo target_buffer_info;
  static constexpr int64_t timestamp = 112358;
  static constexpr BufferIdManager::BufferId buffer_id = 12481516;
  target_buffer_info.buffer_id = buffer_id;
  target_buffer_info.timestamp_micros = timestamp;

  // TODO(rwkeane): Validate the duration in the StartAsync call.
  EXPECT_CALL(*decoder_channel_, StartAsync(start_pts, testing::_))
      .WillOnce(
          testing::WithArgs<1>(CompareTimestampInfos(buffer_id, timestamp)));
  translator_.Start(start_pts, target_buffer_info);
}

TEST_F(ProxyCallTranslatorTest, TestExternalStop) {
  EXPECT_CALL(*decoder_channel_, StopAsync());
  translator_.Stop();
}

TEST_F(ProxyCallTranslatorTest, TestExternalPause) {
  EXPECT_CALL(*decoder_channel_, PauseAsync());
  translator_.Pause();
}

TEST_F(ProxyCallTranslatorTest, TestExternalResume) {
  BufferIdManager::TargetBufferInfo target_buffer_info;
  static constexpr int64_t timestamp = 112358;
  static constexpr BufferIdManager::BufferId buffer_id = 12481516;
  target_buffer_info.buffer_id = buffer_id;
  target_buffer_info.timestamp_micros = timestamp;

  EXPECT_CALL(*decoder_channel_, ResumeAsync(testing::_))
      .WillOnce(
          testing::WithArgs<0>(CompareTimestampInfos(buffer_id, timestamp)));
  translator_.Resume(target_buffer_info);
}

TEST_F(ProxyCallTranslatorTest, TestExternalUpdateTimestamp) {
  BufferIdManager::TargetBufferInfo target_buffer_info;
  static constexpr int timestamp = 112358;
  static constexpr int buffer_id = 12481516;
  target_buffer_info.buffer_id = buffer_id;
  target_buffer_info.timestamp_micros = timestamp;

  EXPECT_CALL(*decoder_channel_, UpdateTimestampAsync(testing::_))
      .WillOnce(
          testing::WithArgs<0>(CompareTimestampInfos(buffer_id, timestamp)));
  translator_.UpdateTimestamp(target_buffer_info);
}

TEST_F(ProxyCallTranslatorTest, TestExternalSetPlaybackRate) {
  constexpr float rate = 42;
  EXPECT_CALL(*decoder_channel_, SetPlaybackAsync(rate));
  translator_.SetPlaybackRate(rate);
}

TEST_F(ProxyCallTranslatorTest, TestExternalSetVolume) {
  constexpr float multiplier = 42;
  EXPECT_CALL(*decoder_channel_, SetVolumeAsync(multiplier));
  translator_.SetVolume(multiplier);
}

TEST_F(ProxyCallTranslatorTest, TestExternalPushBuffer) {
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(3, StreamId::kPrimary));
  buffer->writable_data()[0] = 1;
  buffer->writable_data()[1] = 2;
  buffer->writable_data()[2] = 3;
  EXPECT_EQ(translator_.PushBuffer(buffer, 1),
            CmaBackend::BufferStatus::kBufferSuccess);

  EXPECT_EQ(translator_.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer(), 2),
            CmaBackend::BufferStatus::kBufferSuccess);

  ASSERT_TRUE(translator_as_handler_->HasBufferedData());
  auto result = translator_as_handler_->GetBufferedData();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().has_audio_config());
  ASSERT_TRUE(result.value().has_buffer());
  EXPECT_EQ(result.value().buffer().id(), 1);
  EXPECT_FALSE(result.value().buffer().end_of_stream());
  EXPECT_EQ(result.value().buffer().data().size(), size_t{3});
  EXPECT_EQ(result.value().buffer().data()[0], 1);
  EXPECT_EQ(result.value().buffer().data()[1], 2);
  EXPECT_EQ(result.value().buffer().data()[2], 3);

  ASSERT_TRUE(translator_as_handler_->HasBufferedData());
  result = translator_as_handler_->GetBufferedData();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().has_audio_config());
  ASSERT_TRUE(result.value().has_buffer());
  EXPECT_EQ(result.value().buffer().id(), 2);
  EXPECT_TRUE(result.value().buffer().end_of_stream());
  EXPECT_EQ(result.value().buffer().data().size(), size_t{0});
}

TEST_F(ProxyCallTranslatorTest, TestExternalSetConfig) {
  AudioConfig config;
  config.codec = AudioCodec::kCodecPCM;
  config.channel_layout = ChannelLayout::SURROUND_5_1;
  config.sample_format = SampleFormat::kSampleFormatPlanarS16;
  config.bytes_per_channel = 42;
  config.channel_number = 5;
  config.samples_per_second = 5000;
  config.extra_data = {1, 2, 3};
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

  EXPECT_TRUE(translator_.SetConfig(config));

  ASSERT_TRUE(translator_as_handler_->HasBufferedData());
  auto result = translator_as_handler_->GetBufferedData();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().has_buffer());
  ASSERT_TRUE(result.value().has_audio_config());
  EXPECT_EQ(result.value().audio_config().codec(),
            cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_PCM);
  EXPECT_EQ(result.value().audio_config().channel_layout(),
            cast::media::
                AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_SURROUND_5_1);
  EXPECT_EQ(
      result.value().audio_config().sample_format(),
      cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_PLANAR_S16);
  EXPECT_EQ(result.value().audio_config().bytes_per_channel(),
            config.bytes_per_channel);
  EXPECT_EQ(result.value().audio_config().channel_number(),
            config.channel_number);
  EXPECT_EQ(result.value().audio_config().samples_per_second(),
            config.samples_per_second);
  EXPECT_EQ(result.value().audio_config().extra_data().size(),
            config.extra_data.size());
  EXPECT_EQ(result.value().audio_config().extra_data()[0],
            config.extra_data[0]);
  EXPECT_EQ(result.value().audio_config().extra_data()[1],
            config.extra_data[1]);
  EXPECT_EQ(result.value().audio_config().extra_data()[2],
            config.extra_data[2]);
}

TEST_F(ProxyCallTranslatorTest, TestInternalHandleInitializeFailure) {
  EXPECT_CALL(translator_client_, OnError());
  translator_as_handler_->HandleInitializeResponse(failure_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TestHandleStateChangeFailure) {
  EXPECT_CALL(translator_client_, OnError());
  translator_as_handler_->HandleStateChangeResponse(
      CastRuntimeAudioChannelBroker::Handler::PipelineState::
          PIPELINE_STATE_UNINITIALIZED,
      failure_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TestHandleSetVolumeFailure) {
  EXPECT_CALL(translator_client_, OnError());
  translator_as_handler_->HandleSetVolumeResponse(failure_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TesSetPlaybackFailure) {
  EXPECT_CALL(translator_client_, OnError());
  translator_as_handler_->HandleSetPlaybackResponse(failure_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TestPushBufferFailure) {
  EXPECT_CALL(translator_client_, OnError());
  translator_as_handler_->HandlePushBufferResponse(42, failure_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TestStateChangeSuccess) {
  EXPECT_CALL(
      translator_client_,
      OnPipelineStateChange(CmaProxyHandler::PipelineState::kUninitialized));
  translator_as_handler_->HandleStateChangeResponse(
      CastRuntimeAudioChannelBroker::Handler::PipelineState::
          PIPELINE_STATE_UNINITIALIZED,
      success_status_);
  task_runner_->RunPendingTasks();

  EXPECT_CALL(translator_client_,
              OnPipelineStateChange(CmaProxyHandler::PipelineState::kStopped));
  translator_as_handler_->HandleStateChangeResponse(
      CastRuntimeAudioChannelBroker::Handler::PipelineState::
          PIPELINE_STATE_STOPPED,
      success_status_);
  task_runner_->RunPendingTasks();

  EXPECT_CALL(translator_client_,
              OnPipelineStateChange(CmaProxyHandler::PipelineState::kPlaying));
  translator_as_handler_->HandleStateChangeResponse(
      CastRuntimeAudioChannelBroker::Handler::PipelineState::
          PIPELINE_STATE_PLAYING,
      success_status_);
  task_runner_->RunPendingTasks();

  EXPECT_CALL(translator_client_,
              OnPipelineStateChange(CmaProxyHandler::PipelineState::kPaused));
  translator_as_handler_->HandleStateChangeResponse(
      CastRuntimeAudioChannelBroker::Handler::PipelineState::
          PIPELINE_STATE_PAUSED,
      success_status_);
  task_runner_->RunPendingTasks();
}

TEST_F(ProxyCallTranslatorTest, TestPushBufferSuccess) {
  EXPECT_CALL(translator_client_, OnBytesDecoded(42));
  translator_as_handler_->HandlePushBufferResponse(42, success_status_);
  task_runner_->RunPendingTasks();

  EXPECT_CALL(translator_client_, OnBytesDecoded(112358));
  translator_as_handler_->HandlePushBufferResponse(112358, success_status_);
  task_runner_->RunPendingTasks();
}

}  // namespace media
}  // namespace chromecast
