// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"  // nogncheck
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"  // nogncheck
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"  // nogncheck
#endif
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/test/frame_segmenter_for_test.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class AudioVideoPipelineDeviceTest;

namespace {

const base::TimeDelta kMonitorLoopDelay = base::TimeDelta::FromMilliseconds(20);
// Call Start() with an initial PTS of 0.5 seconds, to test the behaviour if
// we push buffers with a PTS before the start PTS. In this case the backend
// should report the PTS as no later than the last pushed buffers.
const int64_t kStartPts = 500 * 1000;

// Amount that PTS is allowed to progress past the time that Pause() was called.
const int kPausePtsSlackMs = 75;
// Number of effects streams to open simultaneously when also playing a
// non-effects stream.
const int kNumEffectsStreams = 1;

void IgnoreEos() {}

AudioConfig DefaultAudioConfig() {
  AudioConfig default_config;
  default_config.codec = kCodecPCM;
  default_config.channel_layout = ChannelLayout::STEREO;
  default_config.sample_format = kSampleFormatS16;
  default_config.channel_number = 2;
  default_config.bytes_per_channel = 2;
  default_config.samples_per_second = 48000;
  return default_config;
}

VideoConfig DefaultVideoConfig() {
  VideoConfig default_config;
  default_config.codec = kCodecH264;
  default_config.profile = kH264Main;
  default_config.additional_config = nullptr;
  default_config.encryption_scheme = EncryptionScheme::kUnencrypted;
  return default_config;
}

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                  .Append(FILE_PATH_LITERAL("test"))
                  .Append(FILE_PATH_LITERAL("data"))
                  .AppendASCII(name);
  return file_path;
}

class BufferFeeder : public MediaPipelineBackend::Decoder::Delegate {
 public:
  explicit BufferFeeder(const base::Closure& eos_cb);
  ~BufferFeeder() override {}

  static std::unique_ptr<BufferFeeder> LoadAudio(MediaPipelineBackend* backend,
                                                 const std::string& filename,
                                                 const base::Closure& eos_cb);
  static std::unique_ptr<BufferFeeder> LoadVideo(MediaPipelineBackend* backend,
                                                 const std::string& filename,
                                                 bool raw_h264,
                                                 const base::Closure& eos_cb);

  bool eos() const { return eos_; }
  MediaPipelineBackend::Decoder* decoder() const { return decoder_; }
  int64_t last_pushed_pts() const { return last_pushed_pts_; }

  void SetAudioConfig(const AudioConfig& config) { audio_config_ = config; }
  void SetVideoConfig(const VideoConfig& config) { video_config_ = config; }

  void FeedContinuousPcm();
  void PauseBeforeEos();
  void Initialize(MediaPipelineBackend* backend,
                  MediaPipelineBackend::Decoder* decoder,
                  const BufferList& buffers);
  void Start();
  void Stop();

  void ScheduleConfigTest();

  void TestAudioConfigs();
  void TestAudioVolume();
  void TestVideoConfigs();

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

 private:
  void FeedBuffer();
  void FeedPcm();
  void TestConfigs();

  base::Closure eos_cb_;
  bool within_push_buffer_call_;
  bool expecting_buffer_complete_;
  bool feeding_completed_;
  bool eos_;
  bool pause_before_eos_;
  bool test_config_after_next_push_;
  bool feed_continuous_pcm_;
  MediaPipelineBackend* backend_;
  MediaPipelineBackend::Decoder* decoder_;
  BufferList buffers_;
  BufferList buffers_copy_;
  scoped_refptr<DecoderBufferBase> pending_buffer_;
  base::ThreadChecker thread_checker_;
  AudioConfig audio_config_;
  VideoConfig video_config_;
  int64_t last_pushed_pts_;
  std::unique_ptr<::media::AudioTimestampHelper> timestamp_helper_;

  DISALLOW_COPY_AND_ASSIGN(BufferFeeder);
};

}  // namespace

class AudioVideoPipelineDeviceTest : public testing::Test {
 public:
  struct PauseInfo {
    PauseInfo() {}
    PauseInfo(base::TimeDelta d, base::TimeDelta l) : delay(d), length(l) {}
    ~PauseInfo() {}

    base::TimeDelta delay;
    base::TimeDelta length;
  };

  AudioVideoPipelineDeviceTest();
  ~AudioVideoPipelineDeviceTest() override;

  MediaPipelineBackend* backend() const { return backend_.get(); }
  void set_sync_type(MediaPipelineDeviceParams::MediaSyncType sync_type) {
    sync_type_ = sync_type;
  }
  void set_audio_type(MediaPipelineDeviceParams::AudioStreamType audio_type) {
    audio_type_ = audio_type;
  }

  void SetUp() override {
    CastMediaShlib::Initialize(
        base::CommandLine::ForCurrentProcess()->argv());
    VolumeControl::Initialize(base::CommandLine::ForCurrentProcess()->argv());
  }

  void TearDown() override {
    // Pipeline must be destroyed before finalizing media shlib.
    backend_.reset();
    effects_backends_.clear();
    VolumeControl::Finalize();
    CastMediaShlib::Finalize();
  }

  void ConfigureForFile(const std::string& filename);
  void ConfigureForAudioOnly(const std::string& filename);
  void ConfigureForVideoOnly(const std::string& filename, bool raw_h264);

  // Pattern loops, waiting >= pattern[i].delay against media clock between
  // pauses, then pausing for >= pattern[i].length against MessageLoop
  // A pause with delay <0 signals to stop sequence and do not loop
  void SetPausePattern(const std::vector<PauseInfo> pattern);

  // Adds a pause to the end of pause pattern
  void AddPause(base::TimeDelta delay, base::TimeDelta length);
  void PauseBeforeEos();
  void AddEffectsStreams();

  void Initialize();
  void Start();
  void OnEndOfStream();

  void SetAudioFeeder(std::unique_ptr<BufferFeeder> audio_feeder) {
    audio_feeder_ = std::move(audio_feeder);
  }
  void SetVideoFeeder(std::unique_ptr<BufferFeeder> video_feeder) {
    video_feeder_ = std::move(video_feeder);
  }

  void RunStoppedChecks();
  void RunPlaybackChecks();
  void TestBackendStates();
  void StartImmediateEosTest();
  void EndImmediateEosTest();

 private:
  void MonitorLoop();

  void OnPauseCompleted();

  base::test::TaskEnvironment task_environment_;
  MediaPipelineDeviceParams::MediaSyncType sync_type_;
  MediaPipelineDeviceParams::AudioStreamType audio_type_;
  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<MediaPipelineBackend> backend_;
  std::vector<std::unique_ptr<MediaPipelineBackend>> effects_backends_;
  std::vector<std::unique_ptr<BufferFeeder>> effects_feeders_;
  std::unique_ptr<BufferFeeder> audio_feeder_;
  std::unique_ptr<BufferFeeder> video_feeder_;
  bool stopped_;
  bool ran_playing_playback_checks_;
  bool backwards_pts_change_;
  int64_t last_pts_;

  // Current media time.
  base::TimeDelta pause_time_;

  // Pause settings
  std::vector<PauseInfo> pause_pattern_;
  size_t pause_pattern_idx_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineDeviceTest);
};

namespace {

BufferFeeder::BufferFeeder(const base::Closure& eos_cb)
    : eos_cb_(eos_cb),
      within_push_buffer_call_(false),
      expecting_buffer_complete_(false),
      feeding_completed_(false),
      eos_(false),
      pause_before_eos_(false),
      test_config_after_next_push_(false),
      feed_continuous_pcm_(false),
      backend_(nullptr) {
  CHECK(!eos_cb_.is_null());
}

void BufferFeeder::FeedContinuousPcm() {
  feed_continuous_pcm_ = true;
}

void BufferFeeder::PauseBeforeEos() {
  pause_before_eos_ = true;
}

void BufferFeeder::Initialize(MediaPipelineBackend* backend,
                              MediaPipelineBackend::Decoder* decoder,
                              const BufferList& buffers) {
  CHECK(backend);
  CHECK(decoder);
  backend_ = backend;
  decoder_ = decoder;
  decoder_->SetDelegate(this);
  buffers_ = buffers;
  buffers_.push_back(scoped_refptr<DecoderBufferBase>(
      new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer())));
}

void BufferFeeder::Start() {
  if (feed_continuous_pcm_) {
    timestamp_helper_.reset(
        new ::media::AudioTimestampHelper(audio_config_.samples_per_second));
    timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
  }
  last_pushed_pts_ = std::numeric_limits<int64_t>::min();
  buffers_copy_ = buffers_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

void BufferFeeder::Stop() {
  feeding_completed_ = true;
}

void BufferFeeder::ScheduleConfigTest() {
#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
  // On mixer backends, the config test causes the mixer to remove/add the
  // mixer input, which causes it to return invalid PTS to AV sync. Disable for
  // now.
  // TODO(almasrymina): re-enable this. b/110961816.
  return;
#endif
  if (expecting_buffer_complete_) {
    test_config_after_next_push_ = true;
  } else {
    TestConfigs();
  }
}

void BufferFeeder::FeedBuffer() {
  CHECK(decoder_);
  if (feeding_completed_)
    return;
  if (feed_continuous_pcm_) {
    FeedPcm();
    return;
  }
  // Possibly feed one buffer.
  CHECK(!buffers_.empty());

  pending_buffer_ = buffers_.front();
  if (pending_buffer_->end_of_stream()) {
    if (pause_before_eos_)
      ASSERT_TRUE(backend_->Pause());
  } else {
    last_pushed_pts_ = pending_buffer_->timestamp();
  }
  expecting_buffer_complete_ = true;
  within_push_buffer_call_ = true;
  BufferStatus status = decoder_->PushBuffer(pending_buffer_.get());
  within_push_buffer_call_ = false;
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);
  buffers_.pop_front();

  if (pending_buffer_->end_of_stream() && pause_before_eos_)
    ASSERT_TRUE(backend_->Resume());

  // Feeding is done, just wait for the end of stream callback.
  if (pending_buffer_->end_of_stream() || buffers_.empty()) {
    if (buffers_.empty() && !pending_buffer_->end_of_stream())
      LOG(WARNING) << "Stream emptied without feeding EOS frame";
    if (!buffers_.empty())
      LOG(WARNING) << "Stream has more buffers after EOS frame";

    feeding_completed_ = true;
  }

  if (status == MediaPipelineBackend::kBufferPending)
    return;

  OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

void BufferFeeder::FeedPcm() {
  const int num_frames = 512;
  scoped_refptr<::media::DecoderBuffer> silence_buffer(
      new ::media::DecoderBuffer(num_frames * audio_config_.channel_number *
                                 audio_config_.bytes_per_channel));
  memset(silence_buffer->writable_data(), 0, silence_buffer->data_size());
  pending_buffer_ = new media::DecoderBufferAdapter(silence_buffer);
  pending_buffer_->set_timestamp(timestamp_helper_->GetTimestamp());
  timestamp_helper_->AddFrames(num_frames);

  expecting_buffer_complete_ = true;
  within_push_buffer_call_ = true;
  BufferStatus status = decoder_->PushBuffer(pending_buffer_.get());
  within_push_buffer_call_ = false;
  ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);
  if (status == MediaPipelineBackend::kBufferPending)
    return;
  OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

void BufferFeeder::OnEndOfStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
  EXPECT_FALSE(expecting_buffer_complete_)
      << "Got OnEndOfStream() before the EOS buffer completed";
  eos_ = true;
  eos_cb_.Run();
}

void BufferFeeder::OnPushBufferComplete(BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_buffer_ = nullptr;
  EXPECT_FALSE(within_push_buffer_call_)
      << "OnPushBufferComplete() called during a call to PushBuffer()";
  EXPECT_TRUE(expecting_buffer_complete_)
      << "OnPushBufferComplete() called unexpectedly";
  expecting_buffer_complete_ = false;
  if (!feed_continuous_pcm_ || !feeding_completed_) {
    ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);
  }
  EXPECT_FALSE(eos_) << "Got OnPushBufferComplete() after OnEndOfStream()";

  if (test_config_after_next_push_) {
    test_config_after_next_push_ = false;
    TestConfigs();
  }

  if (feeding_completed_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

void BufferFeeder::OnDecoderError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (feed_continuous_pcm_) {
    feeding_completed_ = true;
  } else {
    ASSERT_TRUE(false);
  }
}

void BufferFeeder::OnKeyStatusChanged(const std::string& key_id,
                                      CastKeyStatus key_status,
                                      uint32_t system_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_TRUE(false);
}

void BufferFeeder::OnVideoResolutionChanged(const Size& size) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BufferFeeder::TestConfigs() {
  if (IsValidConfig(audio_config_))
    TestAudioConfigs();
  if (IsValidConfig(video_config_))
    TestVideoConfigs();
}

void BufferFeeder::TestAudioConfigs() {
  MediaPipelineBackend::AudioDecoder* audio_decoder =
      static_cast<MediaPipelineBackend::AudioDecoder*>(decoder_);
  AudioConfig config;
  // First, make sure that kAudioCodecUnknown is not accepted.
  config.codec = kAudioCodecUnknown;
  config.channel_layout = ChannelLayout::STEREO;
  config.sample_format = kSampleFormatS16;
  config.channel_number = 2;
  config.bytes_per_channel = 2;
  config.samples_per_second = 48000;
  // Set invalid config first, to test that the decoder still accepts valid
  // config after an invalid config.
  audio_decoder->SetConfig(config);

  // Next, test required sample formats.
  config.codec = kCodecPCM;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecPCM";

  config.sample_format = kSampleFormatPlanarF32;
  config.bytes_per_channel = 4;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecPCM with "
      << "planar float (required for multiroom audio)";

  config.codec = kCodecAAC;
  // TODO(kmackay) Determine required sample formats/channel numbers.
  config.sample_format = kSampleFormatS16;
  config.bytes_per_channel = 2;
  config.codec = kCodecPCM;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecPCM";
  config.codec = kCodecPCM_S16BE;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecPCM_S16BE";
  config.codec = kCodecAAC;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecAAC";
  config.codec = kCodecMP3;
  EXPECT_TRUE(audio_decoder->SetConfig(config))
      << "Audio decoder does not accept kCodecMP3";

  // Test supported sample rates.
  const int kRequiredSampleRates[] = {8000,  11025, 12000, 16000, 22050,
                                      24000, 32000, 44100, 48000};
  const int kHiResSampleRates[] = {64000, 88200, 96000};
  config.codec = kCodecPCM;
  for (int rate : kRequiredSampleRates) {
    config.samples_per_second = rate;
    EXPECT_TRUE(audio_decoder->SetConfig(config))
        << "Audio decoder does not accept sample rate " << rate;
  }
  for (int rate : kHiResSampleRates) {
    config.samples_per_second = rate;
    if (!audio_decoder->SetConfig(config))
      LOG(INFO) << "Audio decoder does not accept hi-res sample rate " << rate;
  }
  EXPECT_TRUE(audio_decoder->SetConfig(audio_config_));
}

void BufferFeeder::TestAudioVolume() {
  MediaPipelineBackend::AudioDecoder* audio_decoder =
      static_cast<MediaPipelineBackend::AudioDecoder*>(decoder_);
  EXPECT_TRUE(audio_decoder->SetVolume(1.0))
      << "Failed to set audio volume to 1.0";
  EXPECT_TRUE(audio_decoder->SetVolume(0.0))
      << "Failed to set audio volume to 0.0";
  EXPECT_TRUE(audio_decoder->SetVolume(0.2))
      << "Failed to set audio volume to 0.2";
}

void BufferFeeder::TestVideoConfigs() {
  MediaPipelineBackend::VideoDecoder* video_decoder =
      static_cast<MediaPipelineBackend::VideoDecoder*>(decoder_);
  // Set invalid config first, to test that the decoder still accepts valid
  // config after an invalid config.
  VideoConfig config;
  config.codec = kVideoCodecUnknown;
  video_decoder->SetConfig(config);

  EXPECT_TRUE(video_decoder->SetConfig(video_config_));
}

// static
std::unique_ptr<BufferFeeder> BufferFeeder::LoadAudio(
    MediaPipelineBackend* backend,
    const std::string& filename,
    const base::Closure& eos_cb) {
  CHECK(backend);
  base::FilePath file_path = GetTestDataFilePath(filename);
  DemuxResult demux_result = FFmpegDemuxForTest(file_path, true /* audio */);

  MediaPipelineBackend::AudioDecoder* decoder = backend->CreateAudioDecoder();
  CHECK(decoder);
  AudioConfig config = DecoderConfigAdapter::ToCastAudioConfig(
      kPrimary, demux_result.audio_config);
  bool success = decoder->SetConfig(config);
  CHECK(success);

  LOG(INFO) << "Got " << demux_result.frames.size() << " audio input frames";
  std::unique_ptr<BufferFeeder> feeder(new BufferFeeder(eos_cb));
  feeder->audio_config_ = config;
  feeder->Initialize(backend, decoder, demux_result.frames);
  return feeder;
}

// static
std::unique_ptr<BufferFeeder> BufferFeeder::LoadVideo(
    MediaPipelineBackend* backend,
    const std::string& filename,
    bool raw_h264,
    const base::Closure& eos_cb) {
  CHECK(backend);

  VideoConfig video_config;
  BufferList buffers;
  if (raw_h264) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    base::MemoryMappedFile video_stream;
    CHECK(video_stream.Initialize(file_path)) << "Couldn't open stream file: "
                                              << file_path.MaybeAsASCII();
    buffers = H264SegmenterForTest(video_stream.data(), video_stream.length());

    // TODO(erickung): Either pull data from stream or make caller specify value
    video_config.codec = kCodecH264;
    video_config.profile = kH264Main;
    video_config.additional_config = nullptr;
    video_config.encryption_scheme = EncryptionScheme::kUnencrypted;
  } else {
    base::FilePath file_path = GetTestDataFilePath(filename);
    DemuxResult demux_result = FFmpegDemuxForTest(file_path, false /* audio */);
    buffers = demux_result.frames;
    video_config = DecoderConfigAdapter::ToCastVideoConfig(
        kPrimary, demux_result.video_config);
  }

  MediaPipelineBackend::VideoDecoder* decoder = backend->CreateVideoDecoder();
  CHECK(decoder);

  bool success = decoder->SetConfig(video_config);
  CHECK(success);

  LOG(INFO) << "Got " << buffers.size() << " video input frames";
  std::unique_ptr<BufferFeeder> feeder(new BufferFeeder(eos_cb));
  feeder->video_config_ = video_config;
  feeder->Initialize(backend, decoder, buffers);
  return feeder;
}

}  // namespace

AudioVideoPipelineDeviceTest::AudioVideoPipelineDeviceTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      sync_type_(MediaPipelineDeviceParams::kModeSyncPts),
      audio_type_(MediaPipelineDeviceParams::kAudioStreamNormal),
      stopped_(false),
      ran_playing_playback_checks_(false),
      backwards_pts_change_(false),
      pause_pattern_() {}

AudioVideoPipelineDeviceTest::~AudioVideoPipelineDeviceTest() {}

void AudioVideoPipelineDeviceTest::Initialize() {
#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
  VideoDecoderForMixer::InitializeGraphicsForTesting();
#endif

  // Create the media device.
  task_runner_.reset(new TaskRunnerImpl());
  MediaPipelineDeviceParams params(
      sync_type_, audio_type_, task_runner_.get(), AudioContentType::kMedia,
      ::media::AudioDeviceDescription::kDefaultDeviceId);
  backend_.reset(CastMediaShlib::CreateMediaPipelineBackend(params));
  CHECK(backend_);
}

void AudioVideoPipelineDeviceTest::AddPause(base::TimeDelta delay,
                                            base::TimeDelta length) {
  DCHECK_EQ(MediaPipelineDeviceParams::kModeSyncPts, sync_type_);
  pause_pattern_.push_back(PauseInfo(delay, length));
}

void AudioVideoPipelineDeviceTest::PauseBeforeEos() {
  if (audio_feeder_)
    audio_feeder_->PauseBeforeEos();
  if (video_feeder_)
    video_feeder_->PauseBeforeEos();
}

void AudioVideoPipelineDeviceTest::AddEffectsStreams() {
  for (int i = 0; i < kNumEffectsStreams; ++i) {
    MediaPipelineDeviceParams params(
        MediaPipelineDeviceParams::kModeIgnorePts,
        MediaPipelineDeviceParams::kAudioStreamSoundEffects, task_runner_.get(),
        AudioContentType::kMedia,
        ::media::AudioDeviceDescription::kDefaultDeviceId);
    MediaPipelineBackend* effects_backend =
        CastMediaShlib::CreateMediaPipelineBackend(params);
    CHECK(effects_backend);
    effects_backends_.push_back(base::WrapUnique(effects_backend));

    MediaPipelineBackend::AudioDecoder* audio_decoder =
        effects_backend->CreateAudioDecoder();
    audio_decoder->SetConfig(DefaultAudioConfig());

    std::unique_ptr<BufferFeeder> feeder(
        new BufferFeeder(base::Bind(&IgnoreEos)));
    feeder->FeedContinuousPcm();
    feeder->Initialize(effects_backend, audio_decoder, BufferList());
    feeder->SetAudioConfig(DefaultAudioConfig());
    effects_feeders_.push_back(std::move(feeder));
    ASSERT_TRUE(effects_backend->Initialize());
  }
}

void AudioVideoPipelineDeviceTest::SetPausePattern(
    const std::vector<PauseInfo> pattern) {
  DCHECK_EQ(MediaPipelineDeviceParams::kModeSyncPts, sync_type_);
  pause_pattern_ = pattern;
}

void AudioVideoPipelineDeviceTest::ConfigureForAudioOnly(
    const std::string& filename) {
  Initialize();
  audio_feeder_ = BufferFeeder::LoadAudio(
      backend_.get(), filename,
      base::Bind(&AudioVideoPipelineDeviceTest::OnEndOfStream,
                 base::Unretained(this)));
  ASSERT_TRUE(backend_->Initialize());
}

void AudioVideoPipelineDeviceTest::ConfigureForVideoOnly(
    const std::string& filename,
    bool raw_h264) {
  Initialize();
  video_feeder_ = BufferFeeder::LoadVideo(
      backend_.get(), filename, raw_h264,
      base::Bind(&AudioVideoPipelineDeviceTest::OnEndOfStream,
                 base::Unretained(this)));
  ASSERT_TRUE(backend_->Initialize());
}

void AudioVideoPipelineDeviceTest::ConfigureForFile(
    const std::string& filename) {
  Initialize();
  base::Closure eos_cb = base::Bind(
      &AudioVideoPipelineDeviceTest::OnEndOfStream, base::Unretained(this));
  video_feeder_ = BufferFeeder::LoadVideo(backend_.get(), filename,
                                          false /* raw_h264 */, eos_cb);
  audio_feeder_ = BufferFeeder::LoadAudio(backend_.get(), filename, eos_cb);
  ASSERT_TRUE(backend_->Initialize());
}

void AudioVideoPipelineDeviceTest::Start() {
  pause_time_ = base::TimeDelta();
  pause_pattern_idx_ = 0;
  stopped_ = false;
  ran_playing_playback_checks_ = false;
  last_pts_ = std::numeric_limits<int64_t>::min();

  if (audio_feeder_)
    audio_feeder_->Start();
  if (video_feeder_)
    video_feeder_->Start();

  for (auto& feeder : effects_feeders_)
    feeder->Start();
  for (auto& backend : effects_backends_)
    backend->Start(kStartPts);

  RunStoppedChecks();

  backend_->Start(kStartPts);
  // The buffering logic in the media pipeline always starts playback in a
  // paused state. For AV sync reasons, the AV sync media backend now depends on
  // this behavior and doesn't start playback without it, so it needs to be
  // added to the test.
  backend_->Pause();
  backend_->Resume();
  int64_t current_pts = backend()->GetCurrentPts();
  EXPECT_TRUE(current_pts == kStartPts ||
              current_pts == std::numeric_limits<int64_t>::min());
  last_pts_ = current_pts;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AudioVideoPipelineDeviceTest::MonitorLoop,
                                base::Unretained(this)));
}

void AudioVideoPipelineDeviceTest::RunStoppedChecks() {
  if (audio_feeder_) {
    audio_feeder_->ScheduleConfigTest();
    audio_feeder_->TestAudioVolume();
  }
  if (video_feeder_)
    video_feeder_->ScheduleConfigTest();
}

void AudioVideoPipelineDeviceTest::RunPlaybackChecks() {
  if (sync_type_ != MediaPipelineDeviceParams::kModeSyncPts)
    return;
  RunStoppedChecks();

  EXPECT_TRUE(backend_->SetPlaybackRate(1.0f));
  if (!backend_->SetPlaybackRate(0.1f))
    LOG(INFO) << "Playback rate 0.1 not supported";
  if (!backend_->SetPlaybackRate(0.5f))
    LOG(INFO) << "Playback rate 0.5 not supported";
  if (!backend_->SetPlaybackRate(1.5f))
    LOG(INFO) << "Playback rate 1.5 not supported";
  EXPECT_TRUE(backend_->SetPlaybackRate(1.0f));
}

void AudioVideoPipelineDeviceTest::OnEndOfStream() {
  if ((!audio_feeder_ || audio_feeder_->eos()) &&
      (!video_feeder_ || video_feeder_->eos())) {
    RunPlaybackChecks();
    backend_->Stop();
    stopped_ = true;
    RunStoppedChecks();

    for (auto& feeder : effects_feeders_)
      feeder->Stop();

    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

void AudioVideoPipelineDeviceTest::MonitorLoop() {
  // Backend is stopped, no need to monitor the loop any more.
  if (stopped_ || !backend_)
    return;

  // Run checks while playing (once).
  if (!ran_playing_playback_checks_) {
    RunPlaybackChecks();
    ran_playing_playback_checks_ = true;
  }

  int64_t pts = backend_->GetCurrentPts();
  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(pts);
  if (sync_type_ == MediaPipelineDeviceParams::kModeSyncPts) {
    // Check that the current PTS is no more than 100ms past the last pushed
    // PTS.
    if (audio_feeder_ &&
        audio_feeder_->last_pushed_pts() !=
            std::numeric_limits<int64_t>::min()) {
      EXPECT_LE(pts, std::max(kStartPts,
                              audio_feeder_->last_pushed_pts() + 100 * 1000));
    }
    if (video_feeder_ &&
        video_feeder_->last_pushed_pts() !=
            std::numeric_limits<int64_t>::min()) {
      EXPECT_LE(pts, std::max(kStartPts,
                              video_feeder_->last_pushed_pts() + 100 * 1000));
    }
    // PTS is allowed to move backwards once to allow for updates when the first
    // buffers are pushed.
    if (!backwards_pts_change_) {
      if (pts < last_pts_)
        backwards_pts_change_ = true;
    } else {
      EXPECT_GE(pts, last_pts_);
    }
    last_pts_ = pts;
  }

#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
  // Do AV sync checks.
  MediaPipelineBackendForMixer* backend_for_mixer =
      static_cast<MediaPipelineBackendForMixer*>(backend_.get());
  DCHECK(backend_for_mixer);

  int64_t playback_start_time =
      backend_for_mixer->GetPlaybackStartTimeForTesting();

  // The playback start time will be INT64_MIN until the audio and video are
  // ready for playback, so just defer all the AV sync checks till then.
  if (playback_start_time == INT64_MIN) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AudioVideoPipelineDeviceTest::MonitorLoop,
                       base::Unretained(this)),
        kMonitorLoopDelay);
    return;
  }

  int64_t playback_start_pts =
      backend_for_mixer->GetPlaybackStartPtsForTesting();

  if (backend_for_mixer->audio_decoder() &&
      backend_for_mixer->video_decoder() &&
      backend_for_mixer->MonotonicClockNow() > playback_start_time + 50000) {
    int64_t vpts = 0;
    int64_t vpts_timestamp = 0;
    EXPECT_TRUE(backend_for_mixer->video_decoder()->GetCurrentPts(
        &vpts_timestamp, &vpts))
        << "Getting VPTS failed at current time="
        << backend_for_mixer->MonotonicClockNow()
        << " playback should have started at=" << playback_start_time;

    // Check video started at the correct time.
    EXPECT_LT(abs((vpts_timestamp - vpts) -
                  (playback_start_time - playback_start_pts)),
              16000);

    // Check AV sync.
    int64_t apts = 0;
    int64_t apts_timestamp = 0;
    if (backend_for_mixer->audio_decoder()->GetTimestampedPts(&apts_timestamp,
                                                              &apts)) {
      int64_t av_sync_difference =
          (apts - apts_timestamp) - (vpts - vpts_timestamp);

      EXPECT_LT(abs(av_sync_difference), 16000);
    }
  }
#endif

  if (!pause_pattern_.empty() && pause_pattern_idx_ < pause_pattern_.size() &&
      pause_pattern_[pause_pattern_idx_].delay >= base::TimeDelta() &&
      media_time >= pause_time_ + pause_pattern_[pause_pattern_idx_].delay) {
    // Do Pause
    backend_->Pause();
    pause_time_ = base::TimeDelta::FromMicroseconds(backend_->GetCurrentPts());
    RunPlaybackChecks();

    LOG(INFO) << "Pausing at " << pause_time_.InMilliseconds() << "ms for "
              << pause_pattern_[pause_pattern_idx_].length.InMilliseconds()
              << "ms";

    // Wait for pause finish
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AudioVideoPipelineDeviceTest::OnPauseCompleted,
                       base::Unretained(this)),
        pause_pattern_[pause_pattern_idx_].length);
    return;
  }

  // Check state again in a little while
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioVideoPipelineDeviceTest::MonitorLoop,
                     base::Unretained(this)),
      kMonitorLoopDelay);
}

void AudioVideoPipelineDeviceTest::OnPauseCompleted() {
  // Make sure the media time didn't move during that time.
  base::TimeDelta media_time =
      base::TimeDelta::FromMicroseconds(backend_->GetCurrentPts());

  // Make sure that the PTS did not advance too much while paused.
  EXPECT_LT(media_time,
            pause_time_ + base::TimeDelta::FromMilliseconds(kPausePtsSlackMs));

#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
  // Do AV sync checks.
  MediaPipelineBackendForMixer* backend_for_mixer =
      static_cast<MediaPipelineBackendForMixer*>(backend_.get());
  DCHECK(backend_for_mixer);

  int64_t playback_start_time =
      backend_for_mixer->GetPlaybackStartTimeForTesting();

  if (backend_for_mixer->audio_decoder() &&
      backend_for_mixer->video_decoder() &&
      backend_for_mixer->MonotonicClockNow() > playback_start_time + 50000) {
    // Check the audio time.
    base::TimeDelta audio_time = base::TimeDelta::FromMicroseconds(
        backend_for_mixer->audio_decoder()->GetCurrentPts());
    EXPECT_LT(audio_time, pause_time_ + base::TimeDelta::FromMilliseconds(
                                            kPausePtsSlackMs));

    // Check the video time.
    int64_t timestamp = 0;
    int64_t pts = 0;
    EXPECT_TRUE(
        backend_for_mixer->video_decoder()->GetCurrentPts(&timestamp, &pts))
        << "Getting VPTS failed at current time="
        << backend_for_mixer->MonotonicClockNow()
        << " playback should have started at=" << playback_start_time;

    base::TimeDelta video_time = base::TimeDelta::FromMicroseconds(pts);

    EXPECT_LT(video_time, pause_time_ + base::TimeDelta::FromMilliseconds(
                                            kPausePtsSlackMs));
  }
#endif

  pause_time_ = media_time;
  ++pause_pattern_idx_;

  LOG(INFO) << "Pause complete, restarting media clock";
  RunPlaybackChecks();

  // Resume playback and frame feeding.
  backend_->Resume();
  RunPlaybackChecks();

  MonitorLoop();
}

void AudioVideoPipelineDeviceTest::TestBackendStates() {
  ASSERT_TRUE(backend()->Initialize());
  base::RunLoop().RunUntilIdle();

  RunStoppedChecks();
  base::RunLoop().RunUntilIdle();

  const int64_t start_pts = 222;
  ASSERT_TRUE(backend()->Start(start_pts));
  base::RunLoop().RunUntilIdle();
  RunPlaybackChecks();

  ASSERT_TRUE(backend()->Pause());
  base::RunLoop().RunUntilIdle();
  RunPlaybackChecks();

  ASSERT_TRUE(backend()->Resume());
  base::RunLoop().RunUntilIdle();
  RunPlaybackChecks();

  backend()->Stop();
  base::RunLoop().RunUntilIdle();

  RunStoppedChecks();
  base::RunLoop().RunUntilIdle();
}

void AudioVideoPipelineDeviceTest::StartImmediateEosTest() {
  RunStoppedChecks();

  ASSERT_TRUE(backend()->Initialize());
  base::RunLoop().RunUntilIdle();

  Start();
}

void AudioVideoPipelineDeviceTest::EndImmediateEosTest() {
  RunPlaybackChecks();

  ASSERT_TRUE(backend_->Pause());
  base::RunLoop().RunUntilIdle();

  RunPlaybackChecks();

  backend_->Stop();
  base::RunLoop().RunUntilIdle();

  RunStoppedChecks();

  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

TEST_F(AudioVideoPipelineDeviceTest, PcmPlayback) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForAudioOnly("bear_pcm.wav");
  PauseBeforeEos();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, Mp3Playback) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForAudioOnly("sfx.mp3");
  PauseBeforeEos();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, AacPlayback) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForAudioOnly("sfx.m4a");
  PauseBeforeEos();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, VorbisPlayback) {
  set_sync_type(MediaPipelineDeviceParams::kModeIgnorePts);
  ConfigureForAudioOnly("sfx.ogg");
  Start();
  base::RunLoop().Run();
}

// TODO(kmackay) FFmpegDemuxForTest can't handle AC3 or EAC3.

TEST_F(AudioVideoPipelineDeviceTest, H264Playback) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForVideoOnly("bear.h264", true /* raw_h264 */);
  PauseBeforeEos();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlaybackWithPause) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  // Setup to pause for 100ms every 500ms
  AddPause(base::TimeDelta::FromMilliseconds(500),
           base::TimeDelta::FromMilliseconds(100));

  ConfigureForVideoOnly("bear-640x360.webm", false /* raw_h264 */);
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, Vp8Playback) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForVideoOnly("bear-vp8a.webm", false /* raw_h264 */);
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlayback) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForFile("bear-640x360.webm");
  PauseBeforeEos();
  Start();
  base::RunLoop().Run();
}

// TODO(kmackay) FFmpegDemuxForTest can't handle HEVC or VP9.

TEST_F(AudioVideoPipelineDeviceTest, AudioBackendStates) {
  Initialize();
  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend()->CreateAudioDecoder();

  // Test setting config before Initialize().
  std::unique_ptr<BufferFeeder> feeder(
      new BufferFeeder(base::Bind(&IgnoreEos)));
  feeder->Initialize(backend(), audio_decoder, BufferList());
  feeder->SetAudioConfig(DefaultAudioConfig());
  feeder->TestAudioConfigs();

  SetAudioFeeder(std::move(feeder));
  TestBackendStates();
}

TEST_F(AudioVideoPipelineDeviceTest, AudioEffectsBackendStates) {
  set_audio_type(MediaPipelineDeviceParams::kAudioStreamSoundEffects);
  set_sync_type(MediaPipelineDeviceParams::kModeIgnorePts);
  Initialize();
  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend()->CreateAudioDecoder();

  std::unique_ptr<BufferFeeder> feeder(
      new BufferFeeder(base::Bind(&IgnoreEos)));
  feeder->Initialize(backend(), audio_decoder, BufferList());
  feeder->SetAudioConfig(DefaultAudioConfig());
  ASSERT_TRUE(audio_decoder->SetConfig(DefaultAudioConfig()));

  SetAudioFeeder(std::move(feeder));
  TestBackendStates();
}

TEST_F(AudioVideoPipelineDeviceTest, VideoBackendStates) {
  Initialize();
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend()->CreateVideoDecoder();

  // Test setting config before Initialize().
  std::unique_ptr<BufferFeeder> feeder(
      new BufferFeeder(base::Bind(&IgnoreEos)));
  feeder->Initialize(backend(), video_decoder, BufferList());
  feeder->SetVideoConfig(DefaultVideoConfig());
  feeder->TestVideoConfigs();

  SetVideoFeeder(std::move(feeder));
  TestBackendStates();
}

TEST_F(AudioVideoPipelineDeviceTest, AudioImmediateEos) {
  Initialize();
  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend()->CreateAudioDecoder();

  std::unique_ptr<BufferFeeder> feeder(new BufferFeeder(
      base::Bind(&AudioVideoPipelineDeviceTest::EndImmediateEosTest,
                 base::Unretained(this))));
  feeder->Initialize(backend(), audio_decoder, BufferList());
  feeder->SetAudioConfig(DefaultAudioConfig());
  SetAudioFeeder(std::move(feeder));

  ASSERT_TRUE(audio_decoder->SetConfig(DefaultAudioConfig()));
  StartImmediateEosTest();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioVideoPipelineDeviceTest, VideoImmediateEos) {
  Initialize();
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend()->CreateVideoDecoder();
  std::unique_ptr<BufferFeeder> video_feeder(new BufferFeeder(
      base::Bind(&AudioVideoPipelineDeviceTest::EndImmediateEosTest,
                 base::Unretained(this))));
  video_feeder->Initialize(backend(), video_decoder, BufferList());
  video_feeder->SetVideoConfig(DefaultVideoConfig());
  SetVideoFeeder(std::move(video_feeder));

  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend()->CreateAudioDecoder();
  std::unique_ptr<BufferFeeder> audio_feeder(
      new BufferFeeder(base::Bind(&IgnoreEos)));
  audio_feeder->Initialize(backend(), audio_decoder, BufferList());
  audio_feeder->SetAudioConfig(DefaultAudioConfig());
  SetAudioFeeder(std::move(audio_feeder));

  ASSERT_TRUE(audio_decoder->SetConfig(DefaultAudioConfig()));
  ASSERT_TRUE(video_decoder->SetConfig(DefaultVideoConfig()));
  StartImmediateEosTest();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioVideoPipelineDeviceTest, Mp3Playback_WithEffectsStreams) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForAudioOnly("sfx.mp3");
  PauseBeforeEos();
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, AacPlayback_WithEffectsStreams) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForAudioOnly("sfx.m4a");
  PauseBeforeEos();
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, VorbisPlayback_WithEffectsStreams) {
  set_sync_type(MediaPipelineDeviceParams::kModeIgnorePts);
  ConfigureForAudioOnly("sfx.ogg");
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

// TODO(kmackay) FFmpegDemuxForTest can't handle AC3 or EAC3.

TEST_F(AudioVideoPipelineDeviceTest, H264Playback_WithEffectsStreams) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForVideoOnly("bear.h264", true /* raw_h264 */);
  PauseBeforeEos();
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlaybackWithPause_WithEffectsStreams) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  // Setup to pause for 100ms every 500ms
  AddPause(base::TimeDelta::FromMilliseconds(500),
           base::TimeDelta::FromMilliseconds(100));

  ConfigureForVideoOnly("bear-640x360.webm", false /* raw_h264 */);
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, Vp8Playback_WithEffectsStreams) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForVideoOnly("bear-vp8a.webm", false /* raw_h264 */);
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlayback_WithEffectsStreams) {
  if (!MediaCapabilitiesShlib::IsSupportedVideoConfig(kCodecVP8,
                                                      kVideoProfileUnknown, 1))
    return;
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForFile("bear-640x360.webm");
  PauseBeforeEos();
  AddEffectsStreams();
  Start();
  base::RunLoop().Run();
}

TEST_F(AudioVideoPipelineDeviceTest, Mp4Playback) {
  set_sync_type(MediaPipelineDeviceParams::kModeSyncPts);
  ConfigureForFile("bear.mp4");
  Start();
  base::RunLoop().Run();
}

}  // namespace media
}  // namespace chromecast
