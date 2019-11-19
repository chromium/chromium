// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class MultizoneBackendTest;

namespace {

// Total length of test, in microseconds.
const int64_t kPushTimeUs = 2 * base::Time::kMicrosecondsPerSecond;
const int64_t kStartPts = 0;
const int64_t kMaxRenderingDelayErrorUs = 200;
const int kNumEffectsStreams = 1;
const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

void IgnoreEos() {}

class BufferFeeder : public MediaPipelineBackend::Decoder::Delegate {
 public:
  BufferFeeder(const AudioConfig& config,
               bool effects_only,
               base::OnceClosure eos_cb,
               int playback_rate_change_count);
  ~BufferFeeder() override {}

  void Initialize();
  void Start(float playback_rate);
  void Stop();

  int64_t GetMaxRenderingDelayErrorUs() {
    int64_t max = 0;
    for (int64_t error : errors_) {
      if (error == kNoTimestamp) {
        continue;
      }
      max = std::max(max, std::abs(error));
    }
    return max;
  }

 private:
  void FeedBuffer();

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (effects_only_) {
      feeding_completed_ = true;
    } else {
      ASSERT_TRUE(false);
    }
  }
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    ASSERT_TRUE(false);
  }
  void OnVideoResolutionChanged(const Size& size) override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  const AudioConfig config_;
  const bool effects_only_;
  base::OnceClosure eos_cb_;
  const int64_t push_limit_us_;
  const int playback_rate_change_count_;
  const int64_t playback_rate_change_interval_us_;
  float original_playback_rate_;
  float playback_rate_;
  std::vector<int64_t> errors_;
  bool feeding_completed_;
  TaskRunnerImpl task_runner_;
  std::unique_ptr<MediaPipelineBackend> backend_;
  MediaPipelineBackend::AudioDecoder* decoder_;
  int64_t last_push_length_us_;
  int64_t pushed_us_;
  int64_t pushed_us_when_rate_changed_;
  int64_t next_push_playback_timestamp_;
  scoped_refptr<DecoderBufferBase> pending_buffer_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BufferFeeder);
};

}  // namespace

using TestParams = std::tuple<int /* sample rate */,
                              float /* playback rate */,
                              bool /* change_playback_rate */>;

class MultizoneBackendTest : public testing::TestWithParam<TestParams> {
 public:
  MultizoneBackendTest();
  ~MultizoneBackendTest() override;

  void SetUp() override {
    srand(12345);
    CastMediaShlib::Initialize(base::CommandLine::ForCurrentProcess()->argv());
    VolumeControl::Initialize(base::CommandLine::ForCurrentProcess()->argv());
  }

  void TearDown() override {
    // Pipeline must be destroyed before finalizing media shlib.
    audio_feeder_.reset();
    effects_feeders_.clear();
    VolumeControl::Finalize();
    CastMediaShlib::Finalize();
  }

  void AddEffectsStreams();

  void Initialize(int sample_rate, int playback_rate_change_count);
  void Start(float playback_rate);
  void OnEndOfStream();

 private:
  base::test::TaskEnvironment task_environment_;
  std::vector<std::unique_ptr<BufferFeeder>> effects_feeders_;
  std::unique_ptr<BufferFeeder> audio_feeder_;

  DISALLOW_COPY_AND_ASSIGN(MultizoneBackendTest);
};

namespace {

BufferFeeder::BufferFeeder(const AudioConfig& config,
                           bool effects_only,
                           base::OnceClosure eos_cb,
                           int playback_rate_change_count)
    : config_(config),
      effects_only_(effects_only),
      eos_cb_(std::move(eos_cb)),
      push_limit_us_(effects_only_ ? 0 : kPushTimeUs),
      playback_rate_change_count_(playback_rate_change_count),
      playback_rate_change_interval_us_(push_limit_us_ /
                                        (playback_rate_change_count + 1)),
      original_playback_rate_(1.0f),
      playback_rate_(1.0f),
      feeding_completed_(false),
      decoder_(nullptr),
      last_push_length_us_(0),
      pushed_us_(0),
      pushed_us_when_rate_changed_(0),
      next_push_playback_timestamp_(kNoTimestamp) {
  CHECK(eos_cb_);
}

void BufferFeeder::Initialize() {
  MediaPipelineDeviceParams params(
      MediaPipelineDeviceParams::kModeIgnorePts,
      effects_only_ ? MediaPipelineDeviceParams::kAudioStreamSoundEffects
                    : MediaPipelineDeviceParams::kAudioStreamNormal,
      &task_runner_, AudioContentType::kMedia,
      ::media::AudioDeviceDescription::kDefaultDeviceId);
  backend_.reset(CastMediaShlib::CreateMediaPipelineBackend(params));
  CHECK(backend_);

  decoder_ = backend_->CreateAudioDecoder();
  CHECK(decoder_);
  ASSERT_TRUE(decoder_->SetConfig(config_));
  decoder_->SetDelegate(this);

  ASSERT_TRUE(backend_->Initialize());
}

void BufferFeeder::Start(float playback_rate) {
  // AMP devices only support playback rates between 0.5 and 2.0.
  ASSERT_GE(playback_rate, 0.5f);
  ASSERT_LE(playback_rate, 2.0f);
  original_playback_rate_ = playback_rate_ = playback_rate;
  ASSERT_TRUE(backend_->Start(kStartPts));
  ASSERT_TRUE(backend_->SetPlaybackRate(playback_rate));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

void BufferFeeder::Stop() {
  feeding_completed_ = true;
  backend_->Stop();
}

void BufferFeeder::FeedBuffer() {
  CHECK(decoder_);
  if (feeding_completed_)
    return;

  if (playback_rate_change_count_ > 1 && !effects_only_ &&
      pushed_us_ >
          pushed_us_when_rate_changed_ + playback_rate_change_interval_us_) {
    pushed_us_when_rate_changed_ = pushed_us_;
    if (playback_rate_ != original_playback_rate_) {
      playback_rate_ = original_playback_rate_;
    } else if (original_playback_rate_ < 1.0) {
      playback_rate_ = original_playback_rate_ * 2;
    } else {
      playback_rate_ = original_playback_rate_ / 2;
    }
    ASSERT_TRUE(backend_->SetPlaybackRate(playback_rate_));
    // Changing the playback rate will change the rendering delay on devices
    // where playback rate changes apply to audio that has already been pushed.
    // Ignore the next rendering delay.
    next_push_playback_timestamp_ = kNoTimestamp;
  }

  if (!effects_only_ && pushed_us_ >= push_limit_us_) {
    pending_buffer_ = new media::DecoderBufferAdapter(
        ::media::DecoderBuffer::CreateEOSBuffer());
    feeding_completed_ = true;
    last_push_length_us_ = 0;
  } else {
    int size_bytes = (rand() % 96 + 32) * 16;
    int num_samples =
        size_bytes / (config_.bytes_per_channel * config_.channel_number);
    last_push_length_us_ = num_samples * base::Time::kMicrosecondsPerSecond /
                           (config_.samples_per_second * playback_rate_);
    scoped_refptr<::media::DecoderBuffer> silence_buffer(
        new ::media::DecoderBuffer(size_bytes));
    memset(silence_buffer->writable_data(), 0, silence_buffer->data_size());
    pending_buffer_ = new media::DecoderBufferAdapter(silence_buffer);
    pending_buffer_->set_timestamp(
        base::TimeDelta::FromMicroseconds(pushed_us_));
  }
  BufferStatus status = decoder_->PushBuffer(pending_buffer_.get());
  ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);
  if (status == MediaPipelineBackend::kBufferPending)
    return;
  OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

void BufferFeeder::OnEndOfStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(eos_cb_).Run();
}

void BufferFeeder::OnPushBufferComplete(BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_buffer_ = nullptr;

  if (!effects_only_) {
    ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);
    MediaPipelineBackend::AudioDecoder::RenderingDelay delay =
        decoder_->GetRenderingDelay();

    int64_t error = kNoTimestamp;
    if (delay.timestamp_microseconds == kNoTimestamp) {
      next_push_playback_timestamp_ = kNoTimestamp;
    } else {
      if (next_push_playback_timestamp_ == kNoTimestamp) {
        next_push_playback_timestamp_ =
            delay.timestamp_microseconds + delay.delay_microseconds;
      } else {
        int64_t expected_next_push_playback_timestamp =
            next_push_playback_timestamp_ + last_push_length_us_;
        next_push_playback_timestamp_ =
            delay.timestamp_microseconds + delay.delay_microseconds;
        error = next_push_playback_timestamp_ -
                expected_next_push_playback_timestamp;
      }
    }
    errors_.push_back(error);
  }
  pushed_us_ += last_push_length_us_;

  if (feeding_completed_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

}  // namespace

MultizoneBackendTest::MultizoneBackendTest() {}

MultizoneBackendTest::~MultizoneBackendTest() {}

void MultizoneBackendTest::Initialize(int sample_rate,
                                      int playback_rate_change_count) {
  AudioConfig config;
  config.codec = kCodecPCM;
  config.channel_layout = ChannelLayout::STEREO;
  config.sample_format = kSampleFormatPlanarF32;
  config.channel_number = 2;
  config.bytes_per_channel = 4;
  config.samples_per_second = sample_rate;

  audio_feeder_ = std::make_unique<BufferFeeder>(
      config, false /* effects_only */,
      base::BindOnce(&MultizoneBackendTest::OnEndOfStream,
                     base::Unretained(this)),
      playback_rate_change_count);
  audio_feeder_->Initialize();
}

void MultizoneBackendTest::AddEffectsStreams() {
  AudioConfig effects_config;
  effects_config.codec = kCodecPCM;
  effects_config.channel_layout = ChannelLayout::STEREO;
  effects_config.sample_format = kSampleFormatS16;
  effects_config.channel_number = 2;
  effects_config.bytes_per_channel = 2;
  effects_config.samples_per_second = 48000;

  for (int i = 0; i < kNumEffectsStreams; ++i) {
    auto feeder = std::make_unique<BufferFeeder>(
        effects_config, true /* effects_only */, base::BindOnce(&IgnoreEos), 0);
    feeder->Initialize();
    effects_feeders_.push_back(std::move(feeder));
  }
}

void MultizoneBackendTest::Start(float playback_rate) {
  for (auto& feeder : effects_feeders_)
    feeder->Start(1.0f);
  CHECK(audio_feeder_);
  audio_feeder_->Start(playback_rate);
  base::RunLoop().Run();
}

void MultizoneBackendTest::OnEndOfStream() {
  audio_feeder_->Stop();
  for (auto& feeder : effects_feeders_)
    feeder->Stop();

  base::RunLoop::QuitCurrentWhenIdleDeprecated();

  EXPECT_LT(audio_feeder_->GetMaxRenderingDelayErrorUs(),
            kMaxRenderingDelayErrorUs);
}

TEST_P(MultizoneBackendTest, RenderingDelay) {
  const TestParams& params = GetParam();
  int sample_rate = testing::get<0>(params);
  float playback_rate = testing::get<1>(params);
  bool change_playback_rate = testing::get<2>(params);
  int playback_rate_change_count = (change_playback_rate ? 1 : 0);

  Initialize(sample_rate, playback_rate_change_count);
  AddEffectsStreams();
  Start(playback_rate);
}

TEST_F(MultizoneBackendTest, RenderingDelayWithMultipleRateChanges) {
  Initialize(48000 /* sample_rate */, 10 /* playback_rate_change_count */);
  AddEffectsStreams();
  Start(1.0f /* playback_rate */);
}

INSTANTIATE_TEST_SUITE_P(
    Required,
    MultizoneBackendTest,
    testing::Combine(::testing::Values(8000,
                                       11025,
                                       12000,
                                       16000,
                                       22050,
                                       24000,
                                       32000,
                                       44100,
                                       48000),
                     ::testing::Values(0.5f, 0.99f, 1.0f, 1.01f, 2.0f),
                     ::testing::Values(true)));

INSTANTIATE_TEST_SUITE_P(
    Optional,
    MultizoneBackendTest,
    testing::Combine(::testing::Values(64000, 88200, 96000),
                     ::testing::Values(1.0f),
                     ::testing::Values(false)));

}  // namespace media
}  // namespace chromecast
