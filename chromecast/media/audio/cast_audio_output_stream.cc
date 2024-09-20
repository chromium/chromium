// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include <string>
#include <utility>

#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_utils.h"
#include "chromecast/media/audio/cma_audio_output_stream.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/mixer_service/output_stream_connection.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"

#define POST_TO_CMA_WRAPPER(method, ...)                                      \
  do {                                                                        \
    DCHECK(cma_wrapper_);                                                     \
    audio_manager_->media_task_runner()->PostTask(                            \
        FROM_HERE,                                                            \
        base::BindOnce(&CmaAudioOutputStream::method,                         \
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
// Below are settings for MixerService and the DirectAudio it uses.
constexpr base::TimeDelta kFadeTime = base::Milliseconds(5);
constexpr base::TimeDelta kCommunicationsMaxBufferedFrames =
    base::Milliseconds(50);
constexpr base::TimeDelta kMediaMaxBufferedFrames = base::Milliseconds(70);
}  // namespace

namespace chromecast {
namespace media {
namespace {

AudioContentType GetContentType(const std::string& device_id) {
  if (::media::AudioDeviceDescription::IsCommunicationsDevice(device_id)) {
    return AudioContentType::kCommunication;
  }
  return AudioContentType::kMedia;
}

audio_service::ContentType ConvertContentType(AudioContentType content_type) {
  switch (content_type) {
    case AudioContentType::kMedia:
      return audio_service::CONTENT_TYPE_MEDIA;
    case AudioContentType::kCommunication:
      return audio_service::CONTENT_TYPE_COMMUNICATION;
    default:
      NOTREACHED();
  }
}

}  // namespace

class CastAudioOutputStream::MixerServiceWrapper
    : public mixer_service::OutputStreamConnection::Delegate {
 public:
  MixerServiceWrapper(const ::media::AudioParameters& audio_params,
                      const std::string& device_id);

  MixerServiceWrapper(const MixerServiceWrapper&) = delete;
  MixerServiceWrapper& operator=(const MixerServiceWrapper&) = delete;

  ~MixerServiceWrapper() override = default;

  void SetRunning(bool running);
  void Start(AudioSourceCallback* source_callback);
  void Stop(base::WaitableEvent* finished);
  void Close(base::OnceClosure closure);
  void SetVolume(double volume);
  int64_t GetMaxBufferedFrames();
  void Flush();

  base::SingleThreadTaskRunner* io_task_runner() {
    return io_task_runner_.get();
  }

 private:
  // mixer_service::OutputStreamConnection::Delegate implementation:
  void FillNextBuffer(void* buffer,
                      int frames,
                      int64_t delay_timestamp,
                      int64_t delay) override;
  // We don't push an EOS buffer.
  void OnEosPlayed() override { NOTREACHED(); }

  const ::media::AudioParameters audio_params_;
  const std::string device_id_;
  std::unique_ptr<::media::AudioBus> audio_bus_;
  AudioSourceCallback* source_callback_;
  std::unique_ptr<mixer_service::OutputStreamConnection> mixer_connection_;
  double volume_;
  int64_t max_buffered_frames_;

  base::Lock running_lock_;
  bool running_ = true;
  // MixerServiceWrapper must run on an "io thread".
  base::Thread io_thread_;
  // Task runner on |io_thread_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  THREAD_CHECKER(io_thread_checker_);
};

CastAudioOutputStream::MixerServiceWrapper::MixerServiceWrapper(
    const ::media::AudioParameters& audio_params,
    const std::string& device_id)
    : audio_params_(audio_params),
      device_id_(device_id),
      source_callback_(nullptr),
      volume_(1.0f),
      max_buffered_frames_(GetMaxBufferedFrames()),
      io_thread_("CastAudioOutputStream IO") {
  DETACH_FROM_THREAD(io_thread_checker_);

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  options.thread_type = base::ThreadType::kRealtimeAudio;
  CHECK(io_thread_.StartWithOptions(std::move(options)));
  io_task_runner_ = io_thread_.task_runner();
  DCHECK(io_task_runner_);
}

void CastAudioOutputStream::MixerServiceWrapper::SetRunning(bool running) {
  base::AutoLock lock(running_lock_);
  running_ = running;
}

void CastAudioOutputStream::MixerServiceWrapper::Start(
    AudioSourceCallback* source_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  mixer_service::OutputStreamParams params;
  params.set_content_type(audio_service::CONTENT_TYPE_MEDIA);
  params.set_focus_type(ConvertContentType(GetContentType(device_id_)));
  params.set_device_id(device_id_);
  params.set_stream_type(
      mixer_service::OutputStreamParams::STREAM_TYPE_DEFAULT);
  params.set_sample_format(audio_service::SAMPLE_FORMAT_FLOAT_P);
  params.set_sample_rate(audio_params_.sample_rate());
  params.set_num_channels(audio_params_.channels());

  params.set_start_threshold_frames(max_buffered_frames_);
  params.set_max_buffered_frames(max_buffered_frames_);
  params.set_fill_size_frames(audio_params_.frames_per_buffer());

  params.set_fade_frames(::media::AudioTimestampHelper::TimeToFrames(
      kFadeTime, audio_params_.sample_rate()));
  params.set_use_start_timestamp(false);

  source_callback_ = source_callback;
  mixer_connection_ =
      std::make_unique<mixer_service::OutputStreamConnection>(this, params);
  mixer_connection_->Connect();
  mixer_connection_->SetVolumeMultiplier(volume_);
}

void CastAudioOutputStream::MixerServiceWrapper::Stop(
    base::WaitableEvent* finished) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  mixer_connection_.reset();
  source_callback_ = nullptr;
  if (finished) {
    finished->Signal();
  }
}

void CastAudioOutputStream::MixerServiceWrapper::Flush() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  // Nothing to do.
  return;
}

void CastAudioOutputStream::MixerServiceWrapper::Close(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  Stop(nullptr);
  std::move(closure).Run();
}

int64_t CastAudioOutputStream::MixerServiceWrapper::GetMaxBufferedFrames() {
  int fill_size_frames = audio_params_.frames_per_buffer();
  base::TimeDelta target_max_buffered_ms = kMediaMaxBufferedFrames;
  if (GetContentType(device_id_) == AudioContentType::kCommunication) {
    target_max_buffered_ms = kCommunicationsMaxBufferedFrames;
  }

  int64_t target_max_buffered_frames =
      ::media::AudioTimestampHelper::TimeToFrames(target_max_buffered_ms,
                                                  audio_params_.sample_rate());

  // Calculate the buffer size necessary to achieve at least the desired buffer
  // duration, while minimizing latency.
  int64_t max_buffered_frames = 0;
  if (fill_size_frames > target_max_buffered_frames) {
    max_buffered_frames = target_max_buffered_frames;
  } else {
    // Find the largest multiple of |fill_size_frames| that is still no larger
    // than |target_max_buffered_frames|.
    max_buffered_frames =
        (target_max_buffered_frames / fill_size_frames) * fill_size_frames;
  }

  if (max_buffered_frames != target_max_buffered_frames) {
    max_buffered_frames += 1;
  }

  return max_buffered_frames;
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
    int64_t delay_timestamp,
    int64_t delay) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  // Round down to closest multiple of 4 to ensure correct channel alignment.
  frames = base::bits::AlignDownDeprecatedDoNotUse(frames, 4);

  // Acquire running_lock_ for the scope of this fill call to
  // prevent the source callback from closing the output stream
  // mid-fill.
  base::AutoLock lock(running_lock_);

  // Do not fill more buffers if we have stopped running.
  if (!running_)
    return;

  int64_t playout_timestamp =
      (delay_timestamp == INT64_MIN ? INT64_MIN : delay_timestamp + delay);
  if (playout_timestamp < 0) {
    // Assume any negative timestamp is invalid.
    playout_timestamp = 0;
  }

  // Wrap the data buffer so we can write directly into it.
  if (!audio_bus_) {
    audio_bus_ = ::media::AudioBus::CreateWrapper(audio_params_.channels());
  }
  float* channel_data = static_cast<float*>(buffer);
  for (int c = 0; c < audio_params_.channels(); ++c) {
    audio_bus_->SetChannelData(c, channel_data + c * frames);
  }
  audio_bus_->set_frames(frames);

  base::TimeDelta reported_delay = ::media::AudioTimestampHelper::FramesToTime(
      max_buffered_frames_, audio_params_.sample_rate());
  base::TimeTicks reported_delay_timestamp =
      base::TimeTicks() + base::Microseconds(playout_timestamp);

  int frames_filled = source_callback_->OnMoreData(
      reported_delay, reported_delay_timestamp, {}, audio_bus_.get());
  DCHECK_EQ(frames_filled, frames);
  mixer_connection_->SendNextBuffer(frames);
}

CastAudioOutputStream::CastAudioOutputStream(
    CastAudioManagerHelper* audio_manager,
    const ::media::AudioParameters& audio_params,
    const std::string& device_id_or_group_id,
    bool use_mixer_service)
    : volume_(1.0),
      audio_thread_state_(AudioOutputState::kClosed),
      audio_manager_(audio_manager),
      audio_params_(audio_params),
      device_id_(IsValidDeviceId(device_id_or_group_id)
                     ? device_id_or_group_id
                     : ::media::AudioDeviceDescription::kDefaultDeviceId),
      group_id_(GetGroupId(device_id_or_group_id)),
      use_mixer_service_(use_mixer_service),
      audio_weak_factory_(this) {
  DCHECK(audio_manager_);
  DETACH_FROM_THREAD(audio_thread_checker_);
  DVLOG(1) << __func__ << " " << this << " created from group_id=" << group_id_
           << " with audio_params=" << audio_params_.AsHumanReadableString();
  audio_weak_this_ = audio_weak_factory_.GetWeakPtr();
}

CastAudioOutputStream::~CastAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
}

bool CastAudioOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(1) << this << ": " << __func__;
  if (audio_thread_state_ != AudioOutputState::kClosed)
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

  const std::string application_session_id =
      audio_manager_->GetSessionId(group_id_);
  LOG_IF(WARNING, application_session_id.empty()) << "Session id is empty.";
  DVLOG(1) << this << ": " << __func__
           << ", session_id=" << application_session_id;

  if (!use_mixer_service_) {
    cma_wrapper_ = std::make_unique<CmaAudioOutputStream>(
        audio_params_, audio_params_.GetBufferDuration(), device_id_,
        audio_manager_->GetCmaBackendFactory());
    POST_TO_CMA_WRAPPER(Initialize, application_session_id);
  } else {
    DCHECK(!(audio_params_.effects() & ::media::AudioParameters::MULTIZONE));

    mixer_service_wrapper_ =
        std::make_unique<MixerServiceWrapper>(audio_params_, device_id_);
  }

  audio_thread_state_ = AudioOutputState::kOpened;

  return true;
}

void CastAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(1) << this << ": " << __func__;

  audio_thread_state_ = AudioOutputState::kPendingClose;
  base::OnceClosure finish_callback = BindToCurrentThread(
      base::BindOnce(&CastAudioOutputStream::FinishClose, audio_weak_this_));

  if (mixer_service_wrapper_) {
    // Synchronously set running to false to guarantee that
    // AudioSourceCallback::OnMoreData() will not be called anymore.
    mixer_service_wrapper_->SetRunning(false);
    POST_TO_MIXER_SERVICE_WRAPPER(
        Close,
        base::BindPostTask(audio_manager_->audio_manager()->GetTaskRunner(),
                           std::move(finish_callback)));
  } else if (cma_wrapper_) {
    // Synchronously set running to false to guarantee that
    // AudioSourceCallback::OnMoreData() will not be called anymore.
    cma_wrapper_->SetRunning(false);
    POST_TO_CMA_WRAPPER(Close, std::move(finish_callback));
  } else {
    std::move(finish_callback).Run();
  }
}

void CastAudioOutputStream::FinishClose() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  // Signal to the manager that we're closed and can be removed.
  // This should be the last call during the close process as it deletes "this".
  audio_manager_->audio_manager()->ReleaseOutputStream(this);
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  DCHECK(source_callback);
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  // We allow calls to start even in the unopened state.
  DCHECK_NE(audio_thread_state_, AudioOutputState::kPendingClose);
  DVLOG(2) << this << ": " << __func__;
  audio_thread_state_ = AudioOutputState::kStarted;
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstAudio();

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
  DVLOG(2) << this << ": " << __func__;
  // We allow calls to stop even in the unstarted/unopened state.
  if (audio_thread_state_ != AudioOutputState::kStarted)
    return;
  audio_thread_state_ = AudioOutputState::kOpened;

  // |cma_wrapper_| and |mixer_service_wrapper_| cannot be both active.
  DCHECK(!(cma_wrapper_ && mixer_service_wrapper_));

  base::WaitableEvent finished;
  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(Stop, &finished);
  } else if (mixer_service_wrapper_) {
    POST_TO_MIXER_SERVICE_WRAPPER(Stop, &finished);
  } else {
    finished.Signal();
  }
  finished.Wait();
}

void CastAudioOutputStream::Flush() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(2) << this << ": " << __func__;

  // |cma_wrapper_| and |mixer_service_wrapper_| cannot be both active.
  DCHECK(!(cma_wrapper_ && mixer_service_wrapper_));

  if (cma_wrapper_) {
    // Make sure this is not on the same thread as CMA_WRAPPER to prevent
    // deadlock.
    DCHECK(!audio_manager_->media_task_runner()->BelongsToCurrentThread());

    base::WaitableEvent finished;
    POST_TO_CMA_WRAPPER(Flush, base::Unretained(&finished));
    finished.Wait();
  } else if (mixer_service_wrapper_) {
    POST_TO_MIXER_SERVICE_WRAPPER(Flush);
  }
}

void CastAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK_NE(audio_thread_state_, AudioOutputState::kPendingClose);
  DVLOG(2) << this << ": " << __func__ << "(" << volume << ")";
  volume_ = volume;

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

}  // namespace media
}  // namespace chromecast
