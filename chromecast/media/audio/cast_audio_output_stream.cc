// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_service_connection.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/decoder_buffer.h"

#define POST_TO_CMA_WRAPPER(method, ...)                                      \
  do {                                                                        \
    DCHECK(cma_wrapper_);                                                     \
    audio_manager_->media_task_runner()->PostTask(                            \
        FROM_HERE,                                                            \
        base::BindOnce(&CmaWrapper::method,                                   \
                       base::Unretained(cma_wrapper_.get()), ##__VA_ARGS__)); \
  } while (0)

#define POST_TO_MIXER_SERVICE_WRAPPER(method, ...)                     \
  do {                                                                 \
    DCHECK(mixer_service_wrapper_);                                    \
    mixer_service_wrapper_->io_task_runner()->PostTask(                \
        FROM_HERE,                                                     \
        base::BindOnce(&MixerServiceWrapper::method,                   \
                       base::Unretained(mixer_service_wrapper_.get()), \
                       ##__VA_ARGS__));                                \
  } while (0)

namespace {
const int64_t kInvalidTimestamp = std::numeric_limits<int64_t>::min();
// Below are settings for MixerService and the DirectAudio it uses.
constexpr base::TimeDelta kFadeTime = base::TimeDelta::FromMilliseconds(5);
constexpr base::TimeDelta kMixerStartThreshold =
    base::TimeDelta::FromMilliseconds(60);
constexpr base::TimeDelta kMixerBufferSizeInTime =
    base::TimeDelta::FromMilliseconds(25);
}  // namespace

namespace chromecast {
namespace media {

class CastAudioOutputStream::CmaWrapper : public CmaBackend::Decoder::Delegate {
 public:
  CmaWrapper(scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
             const ::media::AudioParameters& audio_params,
             CmaBackendFactory* cma_backend_factory);

  void Initialize(const std::string& application_session_id,
                  chromecast::mojom::MultiroomInfoPtr multiroom_info);
  void Start(AudioSourceCallback* source_callback);
  void Stop();
  void Close(base::OnceClosure closure);
  void SetVolume(double volume);

 private:
  void PushBuffer();

  // CmaBackend::Decoder::Delegate implementation:
  void OnEndOfStream() override {}
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {}
  void OnVideoResolutionChanged(const Size& size) override {}
  void OnPushBufferComplete(BufferStatus status) override;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  const ::media::AudioParameters audio_params_;
  CmaBackendFactory* const cma_backend_factory_;

  AudioOutputState media_thread_state_;
  ::media::AudioTimestampHelper timestamp_helper_;
  const base::TimeDelta buffer_duration_;
  std::unique_ptr<TaskRunnerImpl> cma_backend_task_runner_;
  std::unique_ptr<CmaBackend> cma_backend_;
  std::unique_ptr<::media::AudioBus> audio_bus_;
  scoped_refptr<media::DecoderBufferBase> decoder_buffer_;
  base::OneShotTimer push_timer_;
  bool push_in_progress_;
  bool encountered_error_;
  base::TimeTicks next_push_time_;
  CmaBackend::AudioDecoder* audio_decoder_;
  AudioSourceCallback* source_callback_;

  THREAD_CHECKER(media_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CmaWrapper);
};

CastAudioOutputStream::CmaWrapper::CmaWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    const ::media::AudioParameters& audio_params,
    CmaBackendFactory* cma_backend_factory)
    : audio_task_runner_(audio_task_runner),
      audio_params_(audio_params),
      cma_backend_factory_(cma_backend_factory),
      media_thread_state_(kClosed),
      timestamp_helper_(audio_params_.sample_rate()),
      buffer_duration_(audio_params_.GetBufferDuration()) {
  DETACH_FROM_THREAD(media_thread_checker_);
  DCHECK(audio_task_runner_);
  DCHECK(cma_backend_factory_);

  // Set the default state.
  push_in_progress_ = false;
  encountered_error_ = false;
  audio_decoder_ = nullptr;
  source_callback_ = nullptr;
}

void CastAudioOutputStream::CmaWrapper::Initialize(
    const std::string& application_session_id,
    chromecast::mojom::MultiroomInfoPtr multiroom_info) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(cma_backend_factory_);

  if (media_thread_state_ != kClosed)
    return;
  media_thread_state_ = kOpened;

  MediaPipelineDeviceParams::AudioStreamType stream_type =
      MediaPipelineDeviceParams::kAudioStreamSoundEffects;
  if (audio_params_.effects() & ::media::AudioParameters::MULTIZONE) {
    stream_type = MediaPipelineDeviceParams::kAudioStreamNormal;
  }

  cma_backend_task_runner_ = std::make_unique<TaskRunnerImpl>();
  MediaPipelineDeviceParams device_params(
      MediaPipelineDeviceParams::kModeIgnorePts, stream_type,
      cma_backend_task_runner_.get(), AudioContentType::kMedia,
      ::media::AudioDeviceDescription::kDefaultDeviceId);
  device_params.session_id = application_session_id;
  device_params.multiroom = multiroom_info->multiroom;
  device_params.audio_channel = multiroom_info->audio_channel;
  device_params.output_delay_us = multiroom_info->output_delay.InMicroseconds();
  cma_backend_ = cma_backend_factory_->CreateBackend(device_params);
  if (!cma_backend_) {
    encountered_error_ = true;
    return;
  }

  audio_decoder_ = cma_backend_->CreateAudioDecoder();
  if (!audio_decoder_) {
    encountered_error_ = true;
    return;
  }
  audio_decoder_->SetDelegate(this);

  AudioConfig audio_config;
  audio_config.codec = kCodecPCM;
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = 2;
  audio_config.channel_number = audio_params_.channels();
  audio_config.samples_per_second = audio_params_.sample_rate();
  if (!audio_decoder_->SetConfig(audio_config)) {
    encountered_error_ = true;
    return;
  }

  if (!cma_backend_->Initialize()) {
    encountered_error_ = true;
    return;
  }

  audio_bus_ = ::media::AudioBus::Create(audio_params_);
  decoder_buffer_ =
      base::MakeRefCounted<DecoderBufferAdapter>(new ::media::DecoderBuffer(
          audio_params_.GetBytesPerBuffer(::media::kSampleFormatS16)));
  timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
}

void CastAudioOutputStream::CmaWrapper::Start(
    AudioSourceCallback* source_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (media_thread_state_ == kPendingClose)
    return;

  source_callback_ = source_callback;
  if (encountered_error_) {
    source_callback_->OnError();
    return;
  }

  if (media_thread_state_ == kOpened) {
    media_thread_state_ = kStarted;
    cma_backend_->Start(0);
  }

  next_push_time_ = base::TimeTicks::Now();
  if (!push_in_progress_) {
    push_in_progress_ = true;
    PushBuffer();
  }
}

void CastAudioOutputStream::CmaWrapper::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (cma_backend_ && media_thread_state_ == kStarted) {
    cma_backend_->Stop();
  }
  push_in_progress_ = false;
  media_thread_state_ = kOpened;
  source_callback_ = nullptr;
}

void CastAudioOutputStream::CmaWrapper::Close(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  // Only stop the backend if it was started.
  if (cma_backend_ && media_thread_state_ == kStarted) {
    cma_backend_->Stop();
  }
  push_in_progress_ = false;
  media_thread_state_ = kPendingClose;

  cma_backend_task_runner_.reset();
  cma_backend_.reset();
  audio_bus_.reset();

  audio_task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void CastAudioOutputStream::CmaWrapper::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!audio_decoder_) {
    return;
  }
  if (encountered_error_) {
    return;
  }
  audio_decoder_->SetVolume(volume);
}

void CastAudioOutputStream::CmaWrapper::PushBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);

  // It is possible that this function is called when we are stopped.
  // Return quickly if so.
  if (!source_callback_ || encountered_error_ ||
      media_thread_state_ != kStarted) {
    push_in_progress_ = false;
    return;
  }
  DCHECK(push_in_progress_);

  CmaBackend::AudioDecoder::RenderingDelay rendering_delay =
      audio_decoder_->GetRenderingDelay();
  base::TimeDelta delay =
      base::TimeDelta::FromMicroseconds(rendering_delay.delay_microseconds);
  base::TimeTicks delay_timestamp =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(rendering_delay.timestamp_microseconds);

  // The delay must be greater than zero, and if the timestamp is invalid, we
  // cannot trust the current delay.
  if (rendering_delay.timestamp_microseconds == kInvalidTimestamp ||
      rendering_delay.delay_microseconds < 0) {
    delay = base::TimeDelta();
  }

  int frame_count =
      source_callback_->OnMoreData(delay, delay_timestamp, 0, audio_bus_.get());
  VLOG(3) << "frames_filled=" << frame_count << " with latency=" << delay;

  DCHECK_EQ(frame_count, audio_bus_->frames());
  DCHECK_EQ(static_cast<int>(decoder_buffer_->data_size()),
            audio_params_.GetBytesPerBuffer(::media::kSampleFormatS16));
  audio_bus_->ToInterleaved<::media::SignedInt16SampleTypeTraits>(
      frame_count,
      reinterpret_cast<int16_t*>(decoder_buffer_->writable_data()));
  decoder_buffer_->set_timestamp(timestamp_helper_.GetTimestamp());
  timestamp_helper_.AddFrames(frame_count);

  BufferStatus status = audio_decoder_->PushBuffer(decoder_buffer_.get());
  if (status != CmaBackend::BufferStatus::kBufferPending)
    OnPushBufferComplete(status);
}

void CastAudioOutputStream::CmaWrapper::OnPushBufferComplete(
    BufferStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK_NE(status, CmaBackend::BufferStatus::kBufferPending);

  DCHECK(push_in_progress_);
  push_in_progress_ = false;

  if (!source_callback_ || encountered_error_)
    return;

  if (status != CmaBackend::BufferStatus::kBufferSuccess) {
    source_callback_->OnError();
    return;
  }

  // Schedule next push buffer.
  const base::TimeTicks now = base::TimeTicks::Now();
  next_push_time_ = std::max(now, next_push_time_ + buffer_duration_);

  base::TimeDelta delay = next_push_time_ - now;
  push_timer_.Start(FROM_HERE, delay, this, &CmaWrapper::PushBuffer);
  push_in_progress_ = true;
}

void CastAudioOutputStream::CmaWrapper::OnDecoderError() {
  VLOG(1) << this << ": " << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);

  encountered_error_ = true;
  if (source_callback_)
    source_callback_->OnError();
}

class CastAudioOutputStream::MixerServiceWrapper
    : public chromecast::media::MixerServiceConnection::Delegate {
 public:
  MixerServiceWrapper(
      const ::media::AudioParameters& audio_params,
      MixerServiceConnectionFactory* mixer_service_connection_factory);
  ~MixerServiceWrapper() override = default;

  void Start(AudioSourceCallback* source_callback);
  void Stop();
  void Close(base::OnceClosure closure);
  void SetVolume(double volume);

  base::SingleThreadTaskRunner* io_task_runner() {
    return io_task_runner_.get();
  }

 private:
  // media::MixerServiceConnection::Delegate implementation:
  void FillNextBuffer(void* buffer,
                      int frames,
                      int64_t playout_timestamp) override;
  void OnConnectionError() override;
  // We don't push an EOS buffer.
  void OnEosPlayed() override { NOTREACHED(); }

  const ::media::AudioParameters audio_params_;
  MixerServiceConnectionFactory* mixer_service_connection_factory_;
  std::unique_ptr<::media::AudioBus> audio_bus_;
  AudioSourceCallback* source_callback_;
  std::unique_ptr<media::MixerServiceConnection> mixer_connection_;
  double volume_;

  // MixerServiceWrapper must run on an "io thread".
  base::Thread io_thread_;
  // Task runner on |io_thread_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  THREAD_CHECKER(io_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MixerServiceWrapper);
};

CastAudioOutputStream::MixerServiceWrapper::MixerServiceWrapper(
    const ::media::AudioParameters& audio_params,
    MixerServiceConnectionFactory* mixer_service_connection_factory)
    : audio_params_(audio_params),
      mixer_service_connection_factory_(mixer_service_connection_factory),
      source_callback_(nullptr),
      volume_(1.0f),
      io_thread_("CastAudioOutputStream IO") {
  DCHECK(mixer_service_connection_factory_);
  DETACH_FROM_THREAD(io_thread_checker_);

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  options.priority = base::ThreadPriority::REALTIME_AUDIO;
  CHECK(io_thread_.StartWithOptions(options));
  io_task_runner_ = io_thread_.task_runner();
  DCHECK(io_task_runner_);
}

void CastAudioOutputStream::MixerServiceWrapper::Start(
    AudioSourceCallback* source_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  media::mixer_service::MixerStreamParams params;
  params.set_stream_type(
      media::mixer_service::MixerStreamParams::STREAM_TYPE_SFX);
  params.set_content_type(
      media::mixer_service::MixerStreamParams::CONTENT_TYPE_MEDIA);
  params.set_sample_format(
      media::mixer_service::MixerStreamParams::SAMPLE_FORMAT_FLOAT_P);
  params.set_device_id(::media::AudioDeviceDescription::kDefaultDeviceId);
  params.set_sample_rate(audio_params_.sample_rate());
  params.set_num_channels(audio_params_.channels());
  int64_t start_threshold_frames = ::media::AudioTimestampHelper::TimeToFrames(
      kMixerStartThreshold, audio_params_.sample_rate());
  params.set_start_threshold_frames(start_threshold_frames);

  int32_t fill_size_frames = ::media::AudioTimestampHelper::TimeToFrames(
      kMixerBufferSizeInTime, audio_params_.sample_rate());

  params.set_fill_size_frames(fill_size_frames);
  params.set_use_fader(true);
  params.set_fade_frames(::media::AudioTimestampHelper::TimeToFrames(
      kFadeTime, audio_params_.sample_rate()));

  source_callback_ = source_callback;
  mixer_connection_ =
      mixer_service_connection_factory_->CreateMixerServiceConnection(this,
                                                                      params);
  mixer_connection_->Connect();
  mixer_connection_->SetVolumeMultiplier(volume_);
}

void CastAudioOutputStream::MixerServiceWrapper::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  mixer_connection_.reset();

  source_callback_ = nullptr;
}

void CastAudioOutputStream::MixerServiceWrapper::Close(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  Stop();
  std::move(closure).Run();
}

void CastAudioOutputStream::MixerServiceWrapper::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  volume_ = volume;

  if (mixer_connection_)
    mixer_connection_->SetVolumeMultiplier(volume_);
}

void CastAudioOutputStream::MixerServiceWrapper::FillNextBuffer(
    void* buffer,
    int frames,
    int64_t playout_timestamp) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (playout_timestamp < 0) {
    // Assume any negative timestamp is invalid.
    playout_timestamp = 0;
  }

  // If |audio_bus_| has been created (i.e., this is not the first
  // FillNextBuffer call) and |frames| doesn't change, which is expected behavir
  // from MixerServiceConnection, the |audio_bus_| won't be recreated but be
  // reused.
  if (!audio_bus_ || frames != audio_bus_->frames()) {
    audio_bus_ = ::media::AudioBus::Create(audio_params_.channels(), frames);
  }

  base::TimeDelta delay = kMixerStartThreshold;
  base::TimeTicks delay_timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(playout_timestamp);

  int frames_filled =
      source_callback_->OnMoreData(delay, delay_timestamp, 0, audio_bus_.get());

  float* channel_data = static_cast<float*>(buffer);
  for (int channel = 0; channel < audio_params_.channels(); channel++) {
    std::copy_n(audio_bus_->channel(channel), frames_filled, channel_data);
    channel_data += frames_filled;
  }

  mixer_connection_->SendNextBuffer(frames_filled);
}

void CastAudioOutputStream::MixerServiceWrapper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
}

CastAudioOutputStream::CastAudioOutputStream(
    CastAudioManager* audio_manager,
    service_manager::Connector* connector,
    const ::media::AudioParameters& audio_params,
    MixerServiceConnectionFactory* mixer_service_connection_factory)
    : volume_(1.0),
      audio_thread_state_(kClosed),
      audio_manager_(audio_manager),
      connector_(connector),
      audio_params_(audio_params),
      mixer_service_connection_factory_(mixer_service_connection_factory),
      audio_weak_factory_(this) {
  DCHECK(audio_manager_);
  DCHECK(connector_);
  VLOG(1) << "CastAudioOutputStream " << this << " created with "
          << audio_params_.AsHumanReadableString();
  DETACH_FROM_THREAD(audio_thread_checker_);
}

CastAudioOutputStream::~CastAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
}

bool CastAudioOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  VLOG(1) << this << ": " << __func__;
  if (audio_thread_state_ != kClosed)
    return false;

  // Sanity check the audio parameters.
  ::media::AudioParameters::Format format = audio_params_.format();
  DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
         (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));
  ::media::ChannelLayout channel_layout = audio_params_.channel_layout();
  if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
      (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
    LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
    return false;
  }
  DCHECK_GE(audio_params_.channels(), 1);
  DCHECK_LE(audio_params_.channels(), 2);

  // TODO(awolter, b/111669896): Populate this with the correct session id.
  const std::string application_session_id = "";

  // Connect to the Multiroom interface and fetch the current info.
  connector_->BindInterface(chromecast::mojom::kChromecastServiceName,
                            &multiroom_manager_);
  multiroom_manager_.set_connection_error_handler(
      base::BindOnce(&CastAudioOutputStream::OnGetMultiroomInfo,
                     audio_weak_factory_.GetWeakPtr(), "error",
                     chromecast::mojom::MultiroomInfo::New()));
  multiroom_manager_->GetMultiroomInfo(
      application_session_id,
      base::BindOnce(&CastAudioOutputStream::OnGetMultiroomInfo,
                     audio_weak_factory_.GetWeakPtr(), application_session_id));

  audio_thread_state_ = kOpened;

  // Always return success on the audio thread even though we are unsure at this
  // point if the backend has opened successfully. Errors will be reported via
  // the AudioSourceCallback after Start() has been issued.
  //
  // Failing early is convenient for falling back to other audio stream types,
  // but Cast does not need the fallback, so it is alright to fail late with a
  // callback.
  return true;
}

void CastAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  VLOG(1) << this << ": " << __func__;

  audio_thread_state_ = kPendingClose;
  base::OnceClosure finish_callback = base::BindOnce(
      &CastAudioOutputStream::FinishClose, audio_weak_factory_.GetWeakPtr());

  if (mixer_service_wrapper_) {
    POST_TO_MIXER_SERVICE_WRAPPER(
        Close, BindToTaskRunner(audio_manager_->GetTaskRunner(),
                                std::move(finish_callback)));
  } else if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(Close, std::move(finish_callback));
  } else {
    std::move(finish_callback).Run();
  }
}

void CastAudioOutputStream::FinishClose() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  // Signal to the manager that we're closed and can be removed.
  // This should be the last call during the close process as it deletes "this".
  audio_manager_->ReleaseOutputStream(this);
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  // We allow calls to start even in the unopened state.
  DCHECK(audio_thread_state_ != kPendingClose);
  VLOG(2) << this << ": " << __func__;
  audio_thread_state_ = kStarted;
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstAudio();

  if (!cma_wrapper_ && !mixer_service_wrapper_) {
    // Opening hasn't finished yet, run this Start() later.
    pending_start_ = base::BindOnce(&CastAudioOutputStream::Start,
                                    base::Unretained(this), source_callback);
    return;
  }

  // |cma_wrapper_| and |mixer_service_wrapper_| cannot be both active.
  DCHECK(!(cma_wrapper_ && mixer_service_wrapper_));

  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(Start, source_callback);
  } else {
    POST_TO_MIXER_SERVICE_WRAPPER(Start, source_callback);
  }
}

void CastAudioOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  VLOG(2) << this << ": " << __func__;
  // We allow calls to stop even in the unstarted/unopened state.
  if (audio_thread_state_ != kStarted)
    return;
  audio_thread_state_ = kOpened;
  pending_start_.Reset();
  pending_volume_.Reset();

  // |cma_wrapper_| and |mixer_service_wrapper_| cannot be both active.
  DCHECK(!(cma_wrapper_ && mixer_service_wrapper_));

  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(Stop);
  } else if (mixer_service_wrapper_) {
    POST_TO_MIXER_SERVICE_WRAPPER(Stop);
  }
}

void CastAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(audio_thread_state_ != kPendingClose);
  VLOG(2) << this << ": " << __func__ << "(" << volume << ")";
  volume_ = volume;

  if (!cma_wrapper_ && !mixer_service_wrapper_) {
    pending_volume_ = base::BindOnce(&CastAudioOutputStream::SetVolume,
                                     base::Unretained(this), volume);
    return;
  }
  DCHECK(!(cma_wrapper_ && mixer_service_wrapper_));

  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(SetVolume, volume);
  } else {
    POST_TO_MIXER_SERVICE_WRAPPER(SetVolume, volume);
  }
}

void CastAudioOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  *volume = volume_;
}

void CastAudioOutputStream::OnGetMultiroomInfo(
    const std::string& application_session_id,
    chromecast::mojom::MultiroomInfoPtr multiroom_info) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(multiroom_info);
  CMALOG(kLogControl) << __FUNCTION__ << ": " << this
                      << " session_id=" << application_session_id
                      << ", multiroom=" << multiroom_info->multiroom
                      << ", audio_channel=" << multiroom_info->audio_channel;

  // Close the MultiroomManager message pipe so that a connection error does
  // not trigger a second call to this function.
  multiroom_manager_.reset();
  if (audio_thread_state_ == kPendingClose)
    return;

  if (!mixer_service_connection_factory_) {
    cma_wrapper_ = std::make_unique<CmaWrapper>(
        audio_manager_->GetTaskRunner(), audio_params_,
        audio_manager_->cma_backend_factory());
    POST_TO_CMA_WRAPPER(Initialize, application_session_id,
                        std::move(multiroom_info));
  } else {
    // If direct audio is not available, valid
    // |mixer_service_connection_factory_|
    // shouldn't has been passed in so the CastAudioOutputStream would use
    // CmaBackend.
    DCHECK(!(audio_params_.effects() & ::media::AudioParameters::MULTIZONE) &&
           CastMediaShlib::AddDirectAudioSource);

    mixer_service_wrapper_ = std::make_unique<MixerServiceWrapper>(
        audio_params_, mixer_service_connection_factory_);
  }

  if (pending_start_)
    std::move(pending_start_).Run();
  if (pending_volume_)
    std::move(pending_volume_).Run();
}

}  // namespace media
}  // namespace chromecast
