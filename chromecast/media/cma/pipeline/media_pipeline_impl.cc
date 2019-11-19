// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/base/buffering_controller.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/cma_pipeline_buildflags.h"
#include "chromecast/media/cma/pipeline/media_pipeline_observer.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

namespace {

// Buffering parameters when load_type is kLoadTypeUrl.
constexpr base::TimeDelta kLowBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(2000));
constexpr base::TimeDelta kHighBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(6000));

// Buffering parameters when load_type is kLoadTypeMediaSource.
constexpr base::TimeDelta kLowBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(0));
constexpr base::TimeDelta kHighBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(1000));

// Interval between two updates of the media time.
constexpr base::TimeDelta kTimeUpdateInterval(
    base::TimeDelta::FromMilliseconds(250));

// Interval between two updates of the statistics is equal to:
// kTimeUpdateInterval * kStatisticsUpdatePeriod.
const int kStatisticsUpdatePeriod = 4;

// Stall duration threshold that triggers a playback stall event.
constexpr int kPlaybackStallEventThresholdMs = 2500;

void LogEstimatedBitrate(int decoded_bytes,
                         base::TimeDelta elapsed_time,
                         const char* tag,
                         const char* metric) {
  int estimated_bitrate_in_kbps =
      8 * decoded_bytes / elapsed_time.InMilliseconds();

  if (estimated_bitrate_in_kbps <= 0)
    return;

  LOG(INFO) << "Estimated " << tag << " bitrate is "
            << estimated_bitrate_in_kbps << " kbps";
  metrics::CastMetricsHelper* metrics_helper =
      metrics::CastMetricsHelper::GetInstance();
  metrics_helper->RecordApplicationEventWithValue(metric,
                                                  estimated_bitrate_in_kbps);
}

}  // namespace

struct MediaPipelineImpl::FlushTask {
  bool audio_flushed;
  bool video_flushed;
  base::OnceClosure done_cb;
};

MediaPipelineImpl::MediaPipelineImpl()
    : cdm_context_(nullptr),
      backend_state_(BACKEND_STATE_UNINITIALIZED),
      playback_rate_(0),
      audio_decoder_(nullptr),
      video_decoder_(nullptr),
      pending_time_update_task_(false),
      last_media_time_(::media::kNoTimestamp),
      statistics_rolling_counter_(0),
      audio_bytes_for_bitrate_estimation_(0),
      video_bytes_for_bitrate_estimation_(0),
      playback_stalled_(false),
      playback_stalled_notification_sent_(false),
      media_time_interpolator_(base::DefaultTickClock::GetInstance()),
      weak_factory_(this) {
  LOG(INFO) << __FUNCTION__;
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

MediaPipelineImpl::~MediaPipelineImpl() {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(b/67112414): Do something better than this.
  MediaPipelineObserver::NotifyPipelineDestroyed(this);

  if (backend_state_ != BACKEND_STATE_UNINITIALIZED &&
      backend_state_ != BACKEND_STATE_INITIALIZED)
    metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
        "Cast.Platform.Ended");
}

void MediaPipelineImpl::Initialize(
    LoadType load_type,
    std::unique_ptr<CmaBackend> media_pipeline_backend) {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_backend_ = std::move(media_pipeline_backend);

  if (load_type == kLoadTypeURL || load_type == kLoadTypeMediaSource) {
    base::TimeDelta low_threshold(kLowBufferThresholdURL);
    base::TimeDelta high_threshold(kHighBufferThresholdURL);
    if (load_type == kLoadTypeMediaSource) {
      low_threshold = kLowBufferThresholdMediaSource;
      high_threshold = kHighBufferThresholdMediaSource;
    }
    scoped_refptr<BufferingConfig> buffering_config(
        new BufferingConfig(low_threshold, high_threshold));
    buffering_controller_.reset(new BufferingController(
        buffering_config,
        base::Bind(&MediaPipelineImpl::OnBufferingNotification, weak_this_)));
  }
}

void MediaPipelineImpl::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client.error_cb.is_null());
  DCHECK(!client.buffering_state_cb.is_null());
  client_ = client;
}

void MediaPipelineImpl::SetCdm(int cdm_id) {
  LOG(INFO) << __FUNCTION__ << " cdm_id=" << cdm_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(gunsch): SetCdm(int) is not implemented.
  // One possibility would be a GetCdmByIdCB that's passed in.
}

void MediaPipelineImpl::SetCdm(CastCdmContext* cdm_context) {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  cdm_context_ = cdm_context;
  if (audio_pipeline_)
    audio_pipeline_->SetCdm(cdm_context);
  if (video_pipeline_)
    video_pipeline_->SetCdm(cdm_context);
}

::media::PipelineStatus MediaPipelineImpl::InitializeAudio(
    const ::media::AudioDecoderConfig& config,
    const AvPipelineClient& client,
    std::unique_ptr<CodedFrameProvider> frame_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_decoder_);

  audio_decoder_ = media_pipeline_backend_->CreateAudioDecoder();
  if (!audio_decoder_) {
    return ::media::PIPELINE_ERROR_ABORT;
  }
  audio_pipeline_ = std::make_unique<AudioPipelineImpl>(audio_decoder_, client);
  if (cdm_context_)
    audio_pipeline_->SetCdm(cdm_context_);
  ::media::PipelineStatus status =
      audio_pipeline_->Initialize(config, std::move(frame_provider));

  if (status == ::media::PipelineStatus::PIPELINE_OK) {
    // TODO(b/67112414): Do something better than this.
    MediaPipelineObserver::NotifyAudioPipelineInitialized(this, config);
  }

  return status;
}

::media::PipelineStatus MediaPipelineImpl::InitializeVideo(
    const std::vector<::media::VideoDecoderConfig>& configs,
    const VideoPipelineClient& client,
    std::unique_ptr<CodedFrameProvider> frame_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!video_decoder_);

  video_decoder_ = media_pipeline_backend_->CreateVideoDecoder();
  if (!video_decoder_) {
    return ::media::PIPELINE_ERROR_ABORT;
  }
  video_pipeline_.reset(new VideoPipelineImpl(video_decoder_, client));
  if (cdm_context_)
    video_pipeline_->SetCdm(cdm_context_);
  return video_pipeline_->Initialize(configs, std::move(frame_provider));
}

void MediaPipelineImpl::StartPlayingFrom(base::TimeDelta time) {
  LOG(INFO) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_task_);

  // Lazy initialize.
  if (backend_state_ == BACKEND_STATE_UNINITIALIZED) {
    if (!media_pipeline_backend_->Initialize()) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
    backend_state_ = BACKEND_STATE_INITIALIZED;
  }

  // Start the backend.
  if (!media_pipeline_backend_->Start(time.InMicroseconds())) {
    OnError(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  backend_state_ = BACKEND_STATE_PLAYING;
  ResetBitrateState();
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
      "Cast.Platform.Playing");

  // Enable time updates.
  start_media_time_ = time;
  last_media_time_ = ::media::kNoTimestamp;
  statistics_rolling_counter_ = 0;
  if (!pending_time_update_task_) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineImpl::UpdateMediaTime, weak_this_));
  }

  waiting_for_first_have_enough_data_ = true;

  media_time_interpolator_.SetBounds(time, time, base::TimeTicks::Now());
  media_time_interpolator_.StartInterpolating();

  // Setup the audio and video pipeline for the new timeline.
  if (audio_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("audio");
    if (!audio_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
  if (video_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("video");
    if (!video_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
}

void MediaPipelineImpl::Flush(base::OnceClosure flush_cb) {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_task_);

  media_time_interpolator_.StopInterpolating();

  buffering_controller_->Reset();

  // Flush both audio and video pipeline. This will flush the frame
  // provider and stop feeding buffers to the backend.
  // MediaPipelineImpl::OnFlushDone will stop the backend once flush completes.
  pending_flush_task_.reset(new FlushTask);
  pending_flush_task_->audio_flushed = !audio_pipeline_;
  pending_flush_task_->video_flushed = !video_pipeline_;
  pending_flush_task_->done_cb = std::move(flush_cb);
  if (audio_pipeline_) {
    audio_pipeline_->Flush(
        base::Bind(&MediaPipelineImpl::OnFlushDone, weak_this_, true));
  }
  if (video_pipeline_) {
    video_pipeline_->Flush(
        base::Bind(&MediaPipelineImpl::OnFlushDone, weak_this_, false));
  }
}

void MediaPipelineImpl::SetPlaybackRate(double rate) {
  LOG(INFO) << __FUNCTION__ << " rate=" << rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));

  playback_rate_ = rate;
  if (buffering_controller_ && buffering_controller_->IsBuffering())
    return;

  if (rate != 0.0f) {
    media_pipeline_backend_->SetPlaybackRate(rate);
    media_time_interpolator_.SetPlaybackRate(rate);
    if (backend_state_ == BACKEND_STATE_PAUSED) {
      media_pipeline_backend_->Resume();
      backend_state_ = BACKEND_STATE_PLAYING;
      ResetBitrateState();
      metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
          "Cast.Platform.Playing");
    }
  } else if (backend_state_ == BACKEND_STATE_PLAYING) {
    media_pipeline_backend_->Pause();
    media_time_interpolator_.SetPlaybackRate(0.f);
    backend_state_ = BACKEND_STATE_PAUSED;
    metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
        "Cast.Platform.Pause");
  }
}

void MediaPipelineImpl::SetVolume(float volume) {
  LOG(INFO) << __FUNCTION__ << " vol=" << volume;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_pipeline_)
    audio_pipeline_->SetVolume(volume);
}

base::TimeDelta MediaPipelineImpl::GetMediaTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if BUILDFLAG(CMA_USE_ACCURATE_MEDIA_TIME)
  base::TimeDelta time = base::TimeDelta::FromMicroseconds(
      media_pipeline_backend_->GetCurrentPts());
#else
  base::TimeDelta time = last_media_time_;
#endif
  return (time == ::media::kNoTimestamp ? start_media_time_ : time);
}

bool MediaPipelineImpl::HasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return audio_pipeline_ != nullptr;
}

bool MediaPipelineImpl::HasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return video_pipeline_ != nullptr;
}

void MediaPipelineImpl::OnFlushDone(bool is_audio_stream) {
  LOG(INFO) << __FUNCTION__ << " is_audio_stream=" << is_audio_stream;
  DCHECK(pending_flush_task_);

  if (is_audio_stream) {
    DCHECK(!pending_flush_task_->audio_flushed);
    pending_flush_task_->audio_flushed = true;
  } else {
    DCHECK(!pending_flush_task_->video_flushed);
    pending_flush_task_->video_flushed = true;
  }

  if (pending_flush_task_->audio_flushed &&
      pending_flush_task_->video_flushed) {
    // Stop the backend, so that the backend won't push their pending buffer,
    // which may be invalidated later, to hardware. (b/25342604)
    media_pipeline_backend_->Stop();
    backend_state_ = BACKEND_STATE_INITIALIZED;
    metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
        "Cast.Platform.Ended");

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(pending_flush_task_->done_cb));
    pending_flush_task_.reset();
  }
}

void MediaPipelineImpl::OnBufferingNotification(bool is_buffering) {
  LOG(INFO) << __FUNCTION__ << " is_buffering=" << is_buffering;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));
  DCHECK(buffering_controller_);
  DCHECK_EQ(is_buffering, buffering_controller_->IsBuffering());

  if (waiting_for_first_have_enough_data_) {
    waiting_for_first_have_enough_data_ = is_buffering;
  }

  if (!waiting_for_first_have_enough_data_ && client_.buffering_state_cb) {
    ::media::BufferingState state = is_buffering
                                        ? ::media::BUFFERING_HAVE_NOTHING
                                        : ::media::BUFFERING_HAVE_ENOUGH;
    // Reports buffering state to WMPI. WMPI will change HTMLMediaElement ready
    // state:
    // HAVE_NOTHING -> HAVE_CURRENT_DATA
    // HAVE_ENOUGH -> HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA
    // DEMUXER_UNDERFLOW is the only possible reason. We pass encoded audio to
    // the vendor-specific backend. Our buffering controller only reports a
    // buffering state change based on based on the difference between the
    // current playout PTS reported by the vendor backed and the most recent
    // encoded buffer.
    client_.buffering_state_cb.Run(state, ::media::DEMUXER_UNDERFLOW);
  }

  if (is_buffering && (backend_state_ == BACKEND_STATE_PLAYING)) {
    media_pipeline_backend_->Pause();
    media_time_interpolator_.SetPlaybackRate(0.f);
    backend_state_ = BACKEND_STATE_PAUSED;
    metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
        "Cast.Platform.Pause");
  } else if (!is_buffering && (backend_state_ == BACKEND_STATE_PAUSED)) {
    // Once we finish buffering, we need to honour the desired playback rate
    // (rather than just resuming). This way, if playback was paused while
    // buffering, it will remain paused rather than incorrectly resuming.
    SetPlaybackRate(playback_rate_);
  }
}

void MediaPipelineImpl::CheckForPlaybackStall(base::TimeDelta media_time,
                                              base::TimeTicks current_stc) {
  DCHECK(media_time != ::media::kNoTimestamp);

  // A playback stall is defined as a scenario where the underlying media
  // pipeline has unexpectedly stopped making forward progress. The pipeline is
  // NOT stalled if:
  //
  // 1. Media time is progressing
  // 2. The backend is paused
  // 3. We are currently buffering (this is captured in a separate event)
  if (media_time != last_media_time_ ||
      backend_state_ != BACKEND_STATE_PLAYING ||
      (buffering_controller_ && buffering_controller_->IsBuffering())) {
    if (playback_stalled_) {
      // Transition out of the stalled condition.
      base::TimeDelta stall_duration = current_stc - playback_stalled_time_;
      LOG(INFO) << "Transitioning out of stalled state. Stall duration was "
                << stall_duration.InMilliseconds() << " ms";
      playback_stalled_ = false;
      playback_stalled_notification_sent_ = false;
    }
    return;
  }

  // Check to see if this is a new stall condition.
  if (!playback_stalled_) {
    playback_stalled_ = true;
    playback_stalled_time_ = current_stc;
    return;
  }

  // If we are in an existing stall, check to see if we've been stalled for more
  // than 2.5 s. If so, send a single notification of the stall event.
  if (!playback_stalled_notification_sent_) {
    base::TimeDelta current_stall_duration =
        current_stc - playback_stalled_time_;
    if (current_stall_duration.InMilliseconds() >=
        kPlaybackStallEventThresholdMs) {
      LOG(INFO) << "Playback stalled";
      metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
          "Cast.Platform.PlaybackStall");
      playback_stalled_notification_sent_ = true;
    }
    return;
  }
}

void MediaPipelineImpl::UpdateMediaTime() {
  pending_time_update_task_ = false;
  if ((backend_state_ != BACKEND_STATE_PLAYING) &&
      (backend_state_ != BACKEND_STATE_PAUSED))
    return;

  if (statistics_rolling_counter_ == 0) {
    if (audio_pipeline_)
      audio_pipeline_->UpdateStatistics();
    if (video_pipeline_)
      video_pipeline_->UpdateStatistics();

    if (backend_state_ == BACKEND_STATE_PLAYING) {
      base::TimeTicks current_time = base::TimeTicks::Now();
      if (audio_pipeline_)
        audio_bytes_for_bitrate_estimation_ +=
            audio_pipeline_->bytes_decoded_since_last_update();
      if (video_pipeline_)
        video_bytes_for_bitrate_estimation_ +=
            video_pipeline_->bytes_decoded_since_last_update();
      elapsed_time_delta_ += current_time - last_sample_time_;
      if (elapsed_time_delta_.InMilliseconds() > 5000) {
        if (audio_pipeline_)
          LogEstimatedBitrate(audio_bytes_for_bitrate_estimation_,
                              elapsed_time_delta_, "audio",
                              "Cast.Platform.AudioBitrate");
        if (video_pipeline_)
          LogEstimatedBitrate(video_bytes_for_bitrate_estimation_,
                              elapsed_time_delta_, "video",
                              "Cast.Platform.VideoBitrate");
        ResetBitrateState();
      }
      last_sample_time_ = current_time;
    }
  }

  statistics_rolling_counter_ =
      (statistics_rolling_counter_ + 1) % kStatisticsUpdatePeriod;

  // Wait until the first available timestamp returned from backend, which means
  // the actual playback starts. Some of the rest of the logic, mainly media
  // time interpolating, expects a valid timestamp as baseline.
  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(
      media_pipeline_backend_->GetCurrentPts());
  if (media_time == ::media::kNoTimestamp &&
      (last_media_time_ == ::media::kNoTimestamp ||
       !media_time_interpolator_.interpolating())) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
        kTimeUpdateInterval);
    return;
  }

  base::TimeTicks stc = base::TimeTicks::Now();

  if (media_time == ::media::kNoTimestamp) {
    DCHECK(media_time_interpolator_.interpolating());
    media_time = media_time_interpolator_.GetInterpolatedTime();

    LOG(WARNING) << "Backend returns invalid timestamp. Estimated time is "
                 << media_time;
  } else {
    // It's safe to use kInfiniteDuration as upper bound. When pipeline
    // rebuffers, time interpolator is also paused, in which case it returns
    // the timestamp when pausing it.
    media_time_interpolator_.SetBounds(media_time, ::media::kInfiniteDuration,
                                       stc);
  }

  CheckForPlaybackStall(media_time, stc);

  base::TimeDelta max_rendering_time = media_time;
  if (buffering_controller_) {
    buffering_controller_->SetMediaTime(media_time);

    // Receiving the same time twice in a row means playback isn't moving,
    // so don't interpolate ahead.
    if (media_time != last_media_time_) {
      max_rendering_time = buffering_controller_->GetMaxRenderingTime();
      if (max_rendering_time == ::media::kNoTimestamp)
        max_rendering_time = media_time;

      // Cap interpolation time to avoid interpolating too far ahead.
      max_rendering_time =
          std::min(max_rendering_time, media_time + 2 * kTimeUpdateInterval);
    }
  }

  last_media_time_ = media_time;
  if (!client_.time_update_cb.is_null())
    client_.time_update_cb.Run(media_time, max_rendering_time, stc);

  pending_time_update_task_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
      kTimeUpdateInterval);
}

void MediaPipelineImpl::OnError(::media::PipelineStatus error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(error, ::media::PIPELINE_OK) << "PIPELINE_OK is not an error!";

  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Cast.Platform.Error", error);

  if (!client_.error_cb.is_null())
    client_.error_cb.Run(error);
}

void MediaPipelineImpl::ResetBitrateState() {
  elapsed_time_delta_ = base::TimeDelta::FromSeconds(0);
  audio_bytes_for_bitrate_estimation_ = 0;
  video_bytes_for_bitrate_estimation_ = 0;
  last_sample_time_ = base::TimeTicks::Now();
}

}  // namespace media
}  // namespace chromecast
