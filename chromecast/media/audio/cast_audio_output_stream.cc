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
#include "base/numerics/safe_conversions.h"
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
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"

#define POST_TO_CMA_WRAPPER(method, ...)                                      \
  do {                                                                        \
    DCHECK(cma_wrapper_);                                                     \
    audio_manager_->media_task_runner()->PostTask(                            \
        FROM_HERE,                                                            \
        base::BindOnce(&CmaAudioOutputStream::method,                         \
                       base::Unretained(cma_wrapper_.get()), ##__VA_ARGS__)); \
  } while (0)

namespace chromecast {
namespace media {

CastAudioOutputStream::CastAudioOutputStream(
    CastAudioManagerHelper* audio_manager,
    const ::media::AudioParameters& audio_params,
    const std::string& device_id_or_group_id)
    : volume_(1.0),
      audio_thread_state_(AudioOutputState::kClosed),
      audio_manager_(audio_manager),
      audio_params_(audio_params),
      device_id_(IsValidDeviceId(device_id_or_group_id)
                     ? device_id_or_group_id
                     : ::media::AudioDeviceDescription::kDefaultDeviceId),
      group_id_(GetGroupId(device_id_or_group_id)),
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

  cma_wrapper_ = std::make_unique<CmaAudioOutputStream>(
      audio_params_, audio_params_.GetBufferDuration(), device_id_,
      audio_manager_->GetCmaBackendFactory());
  POST_TO_CMA_WRAPPER(Initialize, application_session_id);

  audio_thread_state_ = AudioOutputState::kOpened;

  return true;
}

void CastAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(1) << this << ": " << __func__;

  audio_thread_state_ = AudioOutputState::kPendingClose;
  base::OnceClosure finish_callback = BindToCurrentThread(
      base::BindOnce(&CastAudioOutputStream::FinishClose, audio_weak_this_));

  if (cma_wrapper_) {
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

  DCHECK(cma_wrapper_);
  POST_TO_CMA_WRAPPER(Start, source_callback);
}

void CastAudioOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(2) << this << ": " << __func__;
  // We allow calls to stop even in the unstarted/unopened state.
  if (audio_thread_state_ != AudioOutputState::kStarted)
    return;
  audio_thread_state_ = AudioOutputState::kOpened;

  base::WaitableEvent finished;
  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(Stop, &finished);
  } else {
    finished.Signal();
  }
  finished.Wait();
}

void CastAudioOutputStream::Flush() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DVLOG(2) << this << ": " << __func__;

  if (cma_wrapper_) {
    // Make sure this is not on the same thread as CMA_WRAPPER to prevent
    // deadlock.
    DCHECK(!audio_manager_->media_task_runner()->BelongsToCurrentThread());

    base::WaitableEvent finished;
    POST_TO_CMA_WRAPPER(Flush, base::Unretained(&finished));
    finished.Wait();
  }
}

void CastAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK_NE(audio_thread_state_, AudioOutputState::kPendingClose);
  DVLOG(2) << this << ": " << __func__ << "(" << volume << ")";
  volume_ = volume;

  if (cma_wrapper_) {
    POST_TO_CMA_WRAPPER(SetVolume, volume);
  }
}

void CastAudioOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  *volume = volume_;
}

}  // namespace media
}  // namespace chromecast
