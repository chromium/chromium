// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/api/test/mock_cma_backend_factory.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_mixer.h"
#include "chromecast/media/audio/mock_cast_audio_manager_helper_delegate.h"
#include "chromecast/media/base/default_monotonic_clock.h"
#include "chromecast/public/task_runner.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace {

constexpr char kSessionId[] = "01234567-89ab-cdef-0123-456789abcdef";

}  // namespace

namespace chromecast {
namespace media {
namespace {
const char kDefaultDeviceId[] = "";
const int64_t kDelayUs = 123;
const double kDefaultVolume = 1.0f;

int on_more_data_call_count_ = 0;
int OnMoreData(base::TimeDelta /* delay */,
               base::TimeTicks /* delay_timestamp */,
               const ::media::AudioGlitchInfo& /* glitch_info */,
               ::media::AudioBus* dest) {
  on_more_data_call_count_++;
  dest->Zero();
  return dest->frames();
}

}  // namespace

class NotifyPushBufferCompleteTask : public chromecast::TaskRunner::Task {
 public:
  explicit NotifyPushBufferCompleteTask(CmaBackend::Decoder::Delegate* delegate)
      : delegate_(delegate) {}
  ~NotifyPushBufferCompleteTask() override = default;
  void Run() override {
    delegate_->OnPushBufferComplete(CmaBackend::BufferStatus::kBufferSuccess);
  }

 private:
  CmaBackend::Decoder::Delegate* const delegate_;
};

class FakeAudioDecoder : public CmaBackend::AudioDecoder {
 public:
  enum TestingPipelineStatus {
    PIPELINE_STATUS_OK,
    PIPELINE_STATUS_BUSY,
    PIPELINE_STATUS_ERROR,
    PIPELINE_STATUS_ASYNC_ERROR,
  };

  explicit FakeAudioDecoder(const MediaPipelineDeviceParams& params)
      : params_(params),
        volume_(kDefaultVolume),
        pipeline_status_(PIPELINE_STATUS_OK),
        pending_push_(false),
        pushed_buffer_count_(0),
        delegate_(nullptr) {}
  ~FakeAudioDecoder() override {}

  // CmaBackend::AudioDecoder implementation:
  void SetDelegate(Delegate* delegate) override {
    DCHECK(delegate);
    delegate_ = delegate;
  }
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override {
    last_buffer_ = std::move(buffer);
    ++pushed_buffer_count_;

    switch (pipeline_status_) {
      case PIPELINE_STATUS_OK:
        return CmaBackend::BufferStatus::kBufferSuccess;
      case PIPELINE_STATUS_BUSY:
        pending_push_ = true;
        return CmaBackend::BufferStatus::kBufferPending;
      case PIPELINE_STATUS_ERROR:
        return CmaBackend::BufferStatus::kBufferFailed;
      case PIPELINE_STATUS_ASYNC_ERROR:
        delegate_->OnDecoderError();
        return CmaBackend::BufferStatus::kBufferSuccess;
      default:
        NOTREACHED();
    }
  }
  void GetStatistics(Statistics* statistics) override {}
  bool SetConfig(const AudioConfig& config) override {
    config_ = config;
    return true;
  }
  bool SetVolume(float volume) override {
    volume_ = volume;
    return true;
  }
  RenderingDelay GetRenderingDelay() override { return rendering_delay_; }
  AudioTrackTimestamp GetAudioTrackTimestamp() override {
    return AudioTrackTimestamp();
  }
  int GetStartThresholdInFrames() override {
    return 0;
  }
  bool RequiresDecryption() override { return false; }

  const AudioConfig& config() const { return config_; }
  float volume() const { return volume_; }
  void set_pipeline_status(TestingPipelineStatus status) {
    if (status == PIPELINE_STATUS_OK && pending_push_) {
      pending_push_ = false;
      params_.task_runner->PostTask(new NotifyPushBufferCompleteTask(delegate_),
                                    0);
    }
    pipeline_status_ = status;
  }
  void set_rendering_delay(RenderingDelay rendering_delay) {
    rendering_delay_ = rendering_delay;
  }
  unsigned pushed_buffer_count() const { return pushed_buffer_count_; }
  const DecoderBufferBase* last_buffer() { return last_buffer_.get(); }

 private:
  const MediaPipelineDeviceParams params_;
  AudioConfig config_;
  float volume_;

  TestingPipelineStatus pipeline_status_;
  bool pending_push_;
  int pushed_buffer_count_;
  scoped_refptr<DecoderBufferBase> last_buffer_;
  Delegate* delegate_;
  RenderingDelay rendering_delay_;
};

class FakeCmaBackend : public CmaBackend {
 public:
  enum State { kStateStopped, kStateRunning, kStatePaused };

  explicit FakeCmaBackend(const MediaPipelineDeviceParams& params)
      : params_(params), state_(kStateStopped), audio_decoder_(nullptr) {}
  ~FakeCmaBackend() override {}

  // CmaBackend implementation:
  AudioDecoder* CreateAudioDecoder() override {
    DCHECK(!audio_decoder_);
    audio_decoder_ = std::make_unique<FakeAudioDecoder>(params_);
    return audio_decoder_.get();
  }
  VideoDecoder* CreateVideoDecoder() override { NOTREACHED(); }

  bool Initialize() override { return true; }
  bool Start(int64_t start_pts) override {
    EXPECT_EQ(kStateStopped, state_);
    state_ = kStateRunning;
    return true;
  }
  void Stop() override {
    EXPECT_TRUE(state_ == kStateRunning || state_ == kStatePaused);
    state_ = kStateStopped;
  }
  bool Pause() override {
    EXPECT_EQ(kStateRunning, state_);
    state_ = kStatePaused;
    return true;
  }
  bool Resume() override {
    EXPECT_EQ(kStatePaused, state_);
    state_ = kStateRunning;
    return true;
  }
  int64_t GetCurrentPts() override { return 0; }
  bool SetPlaybackRate(float rate) override { return true; }

  void LogicalPause() override {}
  void LogicalResume() override {}

  MediaPipelineDeviceParams params() const { return params_; }
  State state() const { return state_; }
  FakeAudioDecoder* audio_decoder() const { return audio_decoder_.get(); }

 private:
  const MediaPipelineDeviceParams params_;
  State state_;
  std::unique_ptr<FakeAudioDecoder> audio_decoder_;
};

class CastAudioOutputStreamTest : public ::testing::Test {
 public:
  CastAudioOutputStreamTest()
      : audio_thread_("CastAudioThread"),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        format_(::media::AudioParameters::AUDIO_PCM_LINEAR),
        channel_layout_config_(::media::ChannelLayoutConfig::Mono()),
        sample_rate_(::media::AudioParameters::kAudioCDSampleRate),
        frames_per_buffer_(256) {}

  void SetUp() override {
    CreateAudioManagerForTesting();
    SetUpCmaBackendFactory();
  }

  void TearDown() override {
    RunThreadsUntilIdle();
    audio_manager_->Shutdown();
    audio_thread_.Stop();
  }

 protected:
  CmaBackendFactory* GetCmaBackendFactory() {
    return mock_backend_factory_.get();
  }

  void CreateAudioManagerForTesting(bool use_mixer = false) {
    // Only one AudioManager may exist at a time, so destroy the one we're
    // currently holding before creating a new one.
    // Flush the message loop to run any shutdown tasks posted by AudioManager.
    if (audio_manager_) {
      audio_manager_->Shutdown();
      audio_manager_.reset();
    }

    if (audio_thread_.IsRunning())
      audio_thread_.Stop();
    CHECK(audio_thread_.StartAndWaitForTesting());
    mock_backend_factory_ = std::make_unique<MockCmaBackendFactory>();
    audio_manager_ = base::WrapUnique(new CastAudioManager(
        std::make_unique<::media::TestAudioThread>(), nullptr, &delegate_,
        base::BindRepeating(&CastAudioOutputStreamTest::GetCmaBackendFactory,
                            base::Unretained(this)),
        task_environment_.GetMainThreadTaskRunner(),
        audio_thread_.task_runner(), use_mixer,
        true /* force_use_cma_backend_for_output*/));
    // A few AudioManager implementations post initialization tasks to
    // audio thread. Flush the thread to ensure that |audio_manager_| is
    // initialized and ready to use before returning from this function.
    // TODO(alokp): We should perhaps do this in AudioManager::Create().
    RunThreadsUntilIdle();
  }

  void SetUpCmaBackendFactory() {
    EXPECT_CALL(*mock_backend_factory_, CreateBackend(_))
        .WillRepeatedly(Invoke([this](const MediaPipelineDeviceParams& params) {
          auto fake_cma_backend = std::make_unique<FakeCmaBackend>(params);
          cma_backend_ = fake_cma_backend.get();
          return fake_cma_backend;
        }));
    EXPECT_EQ(mock_backend_factory_.get(),
              audio_manager_->helper_.GetCmaBackendFactory());
  }

  void RunThreadsUntilIdle() {
    task_environment_.RunUntilIdle();
    audio_thread_.FlushForTesting();
  }

  static void PauseAndWait(base::WaitableEvent* pause_event,
                           base::WaitableEvent* resume_event) {
    pause_event->Signal();
    resume_event->Wait();
  }

  // Synchronously pause the audio thread. This function guarantees that
  // the audio thread will be paused before it returns.
  void PauseAudioThread() {
    audio_thread_pause_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    audio_thread_resume_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    audio_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PauseAndWait, audio_thread_pause_.get(),
                                  audio_thread_resume_.get()));
    audio_thread_pause_->Wait();
  }

  // Resume a paused audio thread by signalling to it.
  void ResumeAudioThreadAsync() {
    if (audio_thread_resume_) {
      audio_thread_resume_->Signal();
    }
  }

  ::media::AudioParameters GetAudioParams() {
    return ::media::AudioParameters(format_, channel_layout_config_,
                                    sample_rate_, frames_per_buffer_);
  }

  FakeAudioDecoder* GetAudioDecoder() {
    return (cma_backend_ ? cma_backend_->audio_decoder() : nullptr);
  }

  ::media::AudioOutputStream* CreateStream() {
    return audio_manager_->MakeAudioOutputStream(
        GetAudioParams(), kDefaultDeviceId,
        ::media::AudioManager::LogCallback());
  }

  base::Thread audio_thread_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::WaitableEvent> audio_thread_pause_;
  std::unique_ptr<base::WaitableEvent> audio_thread_resume_;
  std::unique_ptr<MockCmaBackendFactory> mock_backend_factory_;

  FakeCmaBackend* cma_backend_ = nullptr;
  MockCastAudioManagerHelperDelegate delegate_;
  std::unique_ptr<CastAudioManager> audio_manager_;

  // AudioParameters used to create AudioOutputStream.
  // Tests can modify these parameters before calling CreateStream.
  ::media::AudioParameters::Format format_;
  ::media::ChannelLayoutConfig channel_layout_config_;
  int sample_rate_;
  int frames_per_buffer_;
};

TEST_F(CastAudioOutputStreamTest, CloseWithoutStart) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, CloseWithoutStop) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  stream->Close();
  RunThreadsUntilIdle();
}

TEST_F(CastAudioOutputStreamTest, StartImmediatelyAfterOpen) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  EXPECT_EQ(FakeCmaBackend::kStateRunning, cma_backend_->state());

  stream->Stop();
  RunThreadsUntilIdle();
  stream->Close();
  RunThreadsUntilIdle();
}

TEST_F(CastAudioOutputStreamTest, SetVolumeImmediatelyAfterOpen) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  stream->SetVolume(0.5);

  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  double volume = 0.0;
  stream->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);
  EXPECT_EQ(0.5f, audio_decoder->volume());

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, StartStopStart) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  stream->Stop();
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _)).Times(0);
  RunThreadsUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&source_callback);

  // Ensure we fetch new data when restarting.
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  int last_on_more_data_call_count = on_more_data_call_count_;
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  EXPECT_GT(on_more_data_call_count_, last_on_more_data_call_count);

  EXPECT_EQ(FakeCmaBackend::kStateRunning, cma_backend_->state());

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, StopPreventsCallbacks) {
  // Stream API details that Stop is synchronous and prevents calls to callback.
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  stream->SetVolume(0.5);
  stream->Stop();
  // TODO(steinbock) Make this fail more reliably when stream->Stop() returns
  // asynchronously.
  stream->Close();
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _)).Times(0);
  RunThreadsUntilIdle();
}

TEST_F(CastAudioOutputStreamTest, ClosePreventsCallbacks) {
  // Stream API details that Close is synchronous and prevents calls to
  // callback.
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  // Pause the audio thread from running tasks before calling Close() to
  // prevent it from processing OnMoreData() calls after setting the
  // expectation and before calling Close().
  PauseAudioThread();
  // Once the audio thread resumes work, push/fill calls posted to the audio
  // thread should no longer call OnMoreData().
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _)).Times(0);
  stream->Close();
  ResumeAudioThreadAsync();
  RunThreadsUntilIdle();
}

TEST_F(CastAudioOutputStreamTest, Format) {
  ::media::AudioParameters::Format format[] = {
      ::media::AudioParameters::AUDIO_PCM_LINEAR,
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY};
  for (size_t i = 0; i < std::size(format); ++i) {
    format_ = format[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(stream->Open());
    RunThreadsUntilIdle();

    FakeAudioDecoder* audio_decoder = GetAudioDecoder();
    ASSERT_TRUE(audio_decoder);
    const AudioConfig& audio_config = audio_decoder->config();
    EXPECT_EQ(kCodecPCM, audio_config.codec);
    EXPECT_EQ(kSampleFormatS16, audio_config.sample_format);
    EXPECT_EQ(audio_config.encryption_scheme, EncryptionScheme::kUnencrypted);

    stream->Close();
  }
}

TEST_F(CastAudioOutputStreamTest, ChannelLayout) {
  ::media::ChannelLayoutConfig layout[] = {
      ::media::ChannelLayoutConfig::Mono(),
      ::media::ChannelLayoutConfig::Stereo()};
  for (size_t i = 0; i < std::size(layout); ++i) {
    channel_layout_config_ = layout[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(stream->Open());
    RunThreadsUntilIdle();

    FakeAudioDecoder* audio_decoder = GetAudioDecoder();
    ASSERT_TRUE(audio_decoder);
    const AudioConfig& audio_config = audio_decoder->config();
    EXPECT_EQ(::media::ChannelLayoutToChannelCount(
                  channel_layout_config_.channel_layout()),
              audio_config.channel_number);

    stream->Close();
  }
}

TEST_F(CastAudioOutputStreamTest, SampleRate) {
  sample_rate_ = ::media::AudioParameters::kAudioCDSampleRate;
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  const AudioConfig& audio_config = audio_decoder->config();
  EXPECT_EQ(sample_rate_, audio_config.samples_per_second);
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, DeviceState) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);

  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  ASSERT_TRUE(cma_backend_);
  EXPECT_EQ(FakeCmaBackend::kStateStopped, cma_backend_->state());

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  EXPECT_EQ(FakeCmaBackend::kStateRunning, cma_backend_->state());

  stream->Stop();
  RunThreadsUntilIdle();
  EXPECT_EQ(FakeCmaBackend::kStatePaused, cma_backend_->state());

  stream->Flush();
  RunThreadsUntilIdle();
  EXPECT_EQ(FakeCmaBackend::kStateStopped, cma_backend_->state());

  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, PushFrame) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  // Verify initial state.
  EXPECT_EQ(0u, audio_decoder->pushed_buffer_count());
  EXPECT_FALSE(audio_decoder->last_buffer());

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // No error must be reported to source callback.
  EXPECT_CALL(source_callback, OnError(_)).Times(0);
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  stream->Stop();

  // Verify that the stream pushed frames to the backend.
  EXPECT_LT(0u, audio_decoder->pushed_buffer_count());
  EXPECT_TRUE(audio_decoder->last_buffer());

  // Verify decoder buffer.
  ::media::AudioParameters audio_params = GetAudioParams();
  const size_t expected_frame_size =
      audio_params.GetBytesPerBuffer(::media::kSampleFormatS16);
  const DecoderBufferBase* buffer = audio_decoder->last_buffer();
  EXPECT_TRUE(buffer->data());
  EXPECT_EQ(expected_frame_size, buffer->data_size());
  EXPECT_FALSE(buffer->decrypt_config());  // Null because of raw audio.
  EXPECT_FALSE(buffer->end_of_stream());

  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, PushFrameAfterStop) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // No error must be reported to source callback.
  EXPECT_CALL(source_callback, OnError(_)).Times(0);
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  // Verify that the stream pushed frames to the backend.
  EXPECT_LT(0u, audio_decoder->pushed_buffer_count());
  EXPECT_TRUE(audio_decoder->last_buffer());

  stream->Stop();

  ASSERT_TRUE(cma_backend_);
  base::TimeDelta duration = GetAudioParams().GetBufferDuration() * 2;
  task_environment_.FastForwardBy(duration);
  RunThreadsUntilIdle();

  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, PushFrameAfterClose) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // No error must be reported to source callback.
  EXPECT_CALL(source_callback, OnError(_)).Times(0);
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  // Verify that the stream pushed frames to the backend.
  EXPECT_LT(0u, audio_decoder->pushed_buffer_count());
  EXPECT_TRUE(audio_decoder->last_buffer());

  stream->Close();

  ASSERT_TRUE(cma_backend_);
  base::TimeDelta duration = GetAudioParams().GetBufferDuration() * 2;
  task_environment_.FastForwardBy(duration);
  RunThreadsUntilIdle();
}

// TODO(steinbock) fix test on server and reenable.
TEST_F(CastAudioOutputStreamTest, DISABLED_DeviceBusy) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  audio_decoder->set_pipeline_status(FakeAudioDecoder::PIPELINE_STATUS_BUSY);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // No error must be reported to source callback.
  EXPECT_CALL(source_callback, OnError(_)).Times(0);
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  // Make sure that one frame was pushed.
  EXPECT_EQ(1u, audio_decoder->pushed_buffer_count());

  // Sleep for a few frames and verify that more frames were not pushed
  // because the backend device was busy.
  RunThreadsUntilIdle();
  EXPECT_EQ(1u, audio_decoder->pushed_buffer_count());

  // Unblock the pipeline and verify that PushFrame resumes.
  audio_decoder->set_pipeline_status(FakeAudioDecoder::PIPELINE_STATUS_OK);

  RunThreadsUntilIdle();
  stream->Stop();
  stream->Close();
  RunThreadsUntilIdle();
  EXPECT_LT(1u, audio_decoder->pushed_buffer_count());
}

TEST_F(CastAudioOutputStreamTest, DeviceError) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  audio_decoder->set_pipeline_status(FakeAudioDecoder::PIPELINE_STATUS_ERROR);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // AudioOutputStream must report error to source callback.
  EXPECT_CALL(source_callback, OnError(_));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();
  // Make sure that AudioOutputStream attempted to push the initial frame.
  EXPECT_LT(0u, audio_decoder->pushed_buffer_count());

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, DeviceAsyncError) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  audio_decoder->set_pipeline_status(
      FakeAudioDecoder::PIPELINE_STATUS_ASYNC_ERROR);

  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  // AudioOutputStream must report error to source callback.
  EXPECT_CALL(source_callback, OnError(_)).Times(testing::AtLeast(1));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  // Make sure that one frame was pushed.
  EXPECT_EQ(1u, audio_decoder->pushed_buffer_count());

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, Volume) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();
  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);

  double volume = 0.0;
  stream->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);
  EXPECT_EQ(1.0f, audio_decoder->volume());

  stream->SetVolume(0.5);
  RunThreadsUntilIdle();
  stream->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);
  EXPECT_EQ(0.5f, audio_decoder->volume());
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, InvalidAudioDelay) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  audio_decoder->set_rendering_delay(
      CmaBackend::AudioDecoder::RenderingDelay(-1, 0));

  ::media::MockAudioSourceCallback source_callback;
  const base::TimeDelta delay = base::TimeDelta();
  EXPECT_CALL(source_callback, OnMoreData(delay, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));

  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, AudioDelay) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  FakeAudioDecoder* audio_decoder = GetAudioDecoder();
  ASSERT_TRUE(audio_decoder);
  audio_decoder->set_rendering_delay(
      CmaBackend::AudioDecoder::RenderingDelay(kDelayUs, MonotonicClockNow()));
  ::media::MockAudioSourceCallback source_callback;
  const base::TimeDelta delay(base::Microseconds(kDelayUs));
  // OnMoreData can be called with a shorter delay than the rendering delay in
  // order to prefetch audio data faster.
  EXPECT_CALL(source_callback, OnMoreData(testing::Le(delay), _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));

  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, SessionId) {
  format_ = ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      GetAudioParams(), "DummyGroupId", ::media::AudioManager::LogCallback());
  EXPECT_CALL(delegate_, GetSessionId(_)).WillOnce(Return(kSessionId));
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  // We will start/stop the stream, because as a test, we do not care about
  // whether the info was fetched during Open() or Start() so we test across
  // both.
  ::media::MockAudioSourceCallback source_callback;
  EXPECT_CALL(source_callback, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  stream->Start(&source_callback);
  RunThreadsUntilIdle();

  // TODO(awolter, b/111669896): Verify that the session id is correct after
  // piping has been added. For now, we want to verify that the session id is
  // empty, so that basic MZ continues to work.
  ASSERT_TRUE(cma_backend_);
  MediaPipelineDeviceParams params = cma_backend_->params();
  EXPECT_EQ(params.session_id, kSessionId);

  stream->Stop();
  stream->Close();
}

TEST_F(CastAudioOutputStreamTest, CommunicationsDeviceId) {
  format_ = ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      GetAudioParams(),
      ::media::AudioDeviceDescription::kCommunicationsDeviceId,
      ::media::AudioManager::LogCallback());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  RunThreadsUntilIdle();

  ASSERT_TRUE(cma_backend_);
  MediaPipelineDeviceParams params = cma_backend_->params();
  EXPECT_EQ(params.content_type, AudioContentType::kCommunication);
  EXPECT_EQ(params.device_id,
            ::media::AudioDeviceDescription::kCommunicationsDeviceId);

  stream->Stop();
  stream->Close();
}

}  // namespace media
}  // namespace chromecast
