// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_renderer.h"

#include <stdint.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer_client.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "net/base/io_buffer.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

#define RUN_ON_MAIN_THREAD(method, ...)                     \
  main_task_runner_->PostTask(                              \
      FROM_HERE, base::BindOnce(&CastAudioRenderer::method, \
                                weak_factory_.GetWeakPtr(), ##__VA_ARGS__));

#define MAKE_SURE_MAIN_THREAD(method, ...)                \
  if (!main_task_runner_->RunsTasksInCurrentSequence()) { \
    RUN_ON_MAIN_THREAD(method, ##__VA_ARGS__)             \
    return;                                               \
  }

namespace chromecast {
namespace media {

namespace {

audio_output_service::AudioDecoderConfig ConvertAudioDecoderConfig(
    const ::media::AudioDecoderConfig& audio_decoder_config) {
  chromecast::media::AudioConfig audio_config =
      DecoderConfigAdapter::ToCastAudioConfig(StreamId::kPrimary,
                                              audio_decoder_config);
  audio_output_service::AudioDecoderConfig proto_audio_config;
  proto_audio_config.set_audio_codec(
      audio_service::ConvertAudioCodec(audio_config.codec));
  proto_audio_config.set_channel_layout(
      audio_service::ConvertChannelLayout(audio_config.channel_layout));
  proto_audio_config.set_sample_format(
      audio_service::ConvertSampleFormat(audio_config.sample_format));
  proto_audio_config.set_num_channels(audio_config.channel_number);
  proto_audio_config.set_sample_rate(audio_config.samples_per_second);
  proto_audio_config.set_extra_data(
      reinterpret_cast<const char*>(audio_config.extra_data.data()),
      audio_config.extra_data.size());
  return proto_audio_config;
}

}  // namespace

CastAudioRenderer::CastAudioRenderer(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    ::media::MediaLog* media_log,
    blink::BrowserInterfaceBrokerProxy* interface_broker)
    : media_task_runner_(std::move(media_task_runner)), media_log_(media_log) {
  DCHECK(interface_broker);
  DETACH_FROM_SEQUENCE(sequence_checker_);

  interface_broker->GetInterface(application_media_info_manager_pending_remote_
                                     .InitWithNewPipeAndPassReceiver());
  interface_broker->GetInterface(
      audio_socket_broker_pending_remote_.InitWithNewPipeAndPassReceiver());
}

CastAudioRenderer::~CastAudioRenderer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CastAudioRenderer::Initialize(::media::DemuxerStream* stream,
                                   ::media::CdmContext* cdm_context,
                                   ::media::RendererClient* client,
                                   ::media::PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!demuxer_stream_);
  DCHECK(!init_cb_);
  DCHECK(!application_media_info_manager_remote_);

  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  init_cb_ = std::move(init_cb);

  demuxer_stream_ = stream;
  renderer_client_ = client;
  if (demuxer_stream_->audio_decoder_config().is_encrypted()) {
    if (!cdm_context) {
      LOG(ERROR) << "No CDM context for encrypted stream.";
      OnError(::media::AUDIO_RENDERER_ERROR);
      return;
    }
    decrypting_stream_ = std::make_unique<::media::DecryptingDemuxerStream>(
        media_task_runner_, media_log_,
        base::BindRepeating(&CastAudioRenderer::OnWaiting,
                            weak_factory_.GetWeakPtr()));
    decrypting_stream_->Initialize(
        demuxer_stream_, cdm_context,
        base::BindOnce(&CastAudioRenderer::OnDecryptingDemuxerInitialized,
                       weak_factory_.GetWeakPtr()));
    demuxer_stream_ = decrypting_stream_.get();
  }
  demuxer_stream_->EnableBitstreamConverter();

  application_media_info_manager_remote_.Bind(
      std::move(application_media_info_manager_pending_remote_));
  application_media_info_manager_remote_->GetCastApplicationMediaInfo(
      base::BindOnce(&CastAudioRenderer::OnApplicationMediaInfoReceived,
                     weak_factory_.GetWeakPtr()));
}

::media::TimeSource* CastAudioRenderer::GetTimeSource() {
  return this;
}

void CastAudioRenderer::Flush(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  renderer_started_ = false;

  DCHECK(!flush_cb_);
  if (is_pending_demuxer_read_) {
    flush_cb_ = std::move(callback);
    return;
  }

  FlushInternal();

  LOG(INFO) << "Flush done.";
  std::move(callback).Run();
}

void CastAudioRenderer::FlushInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_connection_);

  if (last_pushed_timestamp_.is_min()) {
    return;
  }

  DCHECK(CurrentPlaybackStateEquals(PlaybackState::kStopped) ||
         is_at_end_of_stream_);

  // At this point, there is no more pending demuxer read,
  // so all the previous tasks associated with the current timeline
  // can be cancelled.
  weak_factory_.InvalidateWeakPtrs();

  output_connection_.AsyncCall(
      &audio_output_service::OutputStreamConnection::StopPlayback);

  SetBufferState(::media::BUFFERING_HAVE_NOTHING);
  last_pushed_timestamp_ = base::TimeDelta::Min();
  read_timer_.Stop();
  is_at_end_of_stream_ = false;
}

void CastAudioRenderer::StartPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  renderer_started_ = true;
  base::TimeDelta media_pos;
  {
    base::AutoLock lock(timeline_lock_);
    media_pos = media_pos_;
    if (GetPlaybackState() != PlaybackState::kStopped) {
      output_connection_.AsyncCall(
          &audio_output_service::OutputStreamConnection::StopPlayback);
    }
    SetPlaybackState(PlaybackState::kStarting);
  }

  output_connection_
      .AsyncCall(
          &audio_output_service::OutputStreamConnection::StartPlayingFrom)
      .WithArgs(media_pos.InMicroseconds());
  ScheduleFetchNextBuffer();
}

void CastAudioRenderer::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  volume_ = volume;
  if (!output_connection_) {
    return;
  }
  output_connection_
      .AsyncCall(&audio_output_service::OutputStreamConnection::SetVolume)
      .WithArgs(volume);
}

void CastAudioRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  NOTIMPLEMENTED();
}

void CastAudioRenderer::SetPreservesPitch(bool preverves_pitch) {
  NOTIMPLEMENTED();
}

void CastAudioRenderer::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  NOTIMPLEMENTED();
}

void CastAudioRenderer::StartTicking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_connection_);
  DCHECK(!ticking_);

  ticking_ = true;
  float playback_rate = 0.0f;
  {
    base::AutoLock lock(timeline_lock_);
    if (playback_rate_ == 0.0f) {
      // Wait for SetPlaybackRate() to start the playback.
      return;
    }
    playback_rate = playback_rate_;
  }
  output_connection_
      .AsyncCall(&audio_output_service::OutputStreamConnection::SetPlaybackRate)
      .WithArgs(playback_rate);
}

void CastAudioRenderer::StopTicking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!CurrentPlaybackStateEquals(PlaybackState::kStopped));
  DCHECK(output_connection_);
  DCHECK(ticking_);

  ticking_ = false;
  output_connection_.AsyncCall(
      &audio_output_service::OutputStreamConnection::StopPlayback);

  base::AutoLock lock(timeline_lock_);
  UpdateTimelineOnStop();
  SetPlaybackState(PlaybackState::kStopped);
}

void CastAudioRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_connection_);

  playback_rate = base::ranges::clamp(playback_rate, 0.0, 2.0);
  {
    base::AutoLock lock(timeline_lock_);
    if (playback_rate == 0.0) {
      UpdateTimelineOnStop();
    }

    if (ticking_ && playback_rate_ == 0.0f && playback_rate > 0.0) {
      // It is necessary to set the playback state in the `Resume` case since
      // `playback_rate_` is changed immediately but the media time should not
      // move forward until we received a timestamp update in
      // OnNextBuffer().
      SetPlaybackState(PlaybackState::kStarting);
    }
    playback_rate_ = playback_rate;
  }
  if (!ticking_) {
    // Wait until StartTicking() is called to set the playback rate.
    return;
  }
  output_connection_
      .AsyncCall(&audio_output_service::OutputStreamConnection::SetPlaybackRate)
      .WithArgs(playback_rate);
}

void CastAudioRenderer::SetBufferState(::media::BufferingState buffer_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer_state == buffer_state_) {
    return;
  }
  buffer_state_ = buffer_state;
  renderer_client_->OnBufferingStateChange(
      buffer_state_, ::media::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void CastAudioRenderer::SetMediaTime(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CurrentPlaybackStateEquals(PlaybackState::kStopped) ||
         CurrentPlaybackStateEquals(PlaybackState::kStarting));

  {
    base::AutoLock lock(timeline_lock_);
    media_pos_ = time;
    reference_time_ = base::TimeTicks();
  }

  FlushInternal();
}

base::TimeDelta CastAudioRenderer::CurrentMediaTime() {
  base::AutoLock lock(timeline_lock_);

  if (!IsTimeMoving()) {
    return media_pos_;
  }

  return CurrentMediaTimeLocked();
}

bool CastAudioRenderer::IsTimeMoving() {
  return state_ == PlaybackState::kPlaying && playback_rate_ > 0.0f;
}

void CastAudioRenderer::UpdateTimelineOnStop() {
  if (!IsTimeMoving()) {
    return;
  }

  media_pos_ = CurrentMediaTimeLocked();
  reference_time_ = base::TimeTicks::Now();
}

void CastAudioRenderer::UpdateAudioDecoderConfig(
    const ::media::AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  audio_output_service::CmaBackendParams backend_params;
  *(backend_params.mutable_audio_decoder_config()) =
      ConvertAudioDecoderConfig(config);
  output_connection_
      .AsyncCall(
          &audio_output_service::OutputStreamConnection::UpdateAudioConfig)
      .WithArgs(backend_params);
}

base::TimeDelta CastAudioRenderer::CurrentMediaTimeLocked() {
  DCHECK(IsTimeMoving());
  return media_pos_ +
         (base::TimeTicks::Now() - reference_time_) * playback_rate_;
}

bool CastAudioRenderer::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  wall_clock_times->reserve(media_timestamps.size());
  auto now = base::TimeTicks::Now();

  base::AutoLock lock(timeline_lock_);

  const bool is_time_moving = IsTimeMoving();

  if (media_timestamps.empty()) {
    wall_clock_times->push_back(is_time_moving ? now : reference_time_);
    return is_time_moving;
  }

  base::TimeTicks wall_clock_base = is_time_moving ? reference_time_ : now;

  for (base::TimeDelta timestamp : media_timestamps) {
    base::TimeTicks wall_clock_time;

    auto relative_pos = timestamp - media_pos_;
    if (is_time_moving) {
      relative_pos = relative_pos / playback_rate_;
    }
    wall_clock_time = wall_clock_base + relative_pos;
    wall_clock_times->push_back(wall_clock_time);
  }

  return is_time_moving;
}

void CastAudioRenderer::ScheduleFetchNextBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!renderer_started_ || !demuxer_stream_ || read_timer_.IsRunning() ||
      is_pending_demuxer_read_ || is_at_end_of_stream_) {
    return;
  }

  base::TimeDelta next_read_delay;
  if (!last_pushed_timestamp_.is_min()) {
    std::vector<base::TimeTicks> wall_clock_times;
    bool is_time_moving =
        GetWallClockTimes({last_pushed_timestamp_}, &wall_clock_times);
    base::TimeDelta relative_buffer_pos =
        wall_clock_times[0] - base::TimeTicks::Now();

    if (relative_buffer_pos >= min_lead_time_) {
      SetBufferState(::media::BUFFERING_HAVE_ENOUGH);
    }

    if (relative_buffer_pos >= max_lead_time_) {
      if (!is_time_moving) {
        return;
      }
      next_read_delay = relative_buffer_pos - max_lead_time_;
    }
  }

  read_timer_.Start(FROM_HERE, next_read_delay, this,
                    &CastAudioRenderer::FetchNextBuffer);
}

CastAudioRenderer::PlaybackState CastAudioRenderer::GetPlaybackState() {
  return state_;
}

bool CastAudioRenderer::CurrentPlaybackStateEquals(
    CastAudioRenderer::PlaybackState playback_state) {
  base::AutoLock lock(timeline_lock_);
  return state_ == playback_state;
}

void CastAudioRenderer::SetPlaybackState(PlaybackState state) {
  state_ = state;
}

void CastAudioRenderer::FetchNextBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(demuxer_stream_);

  DCHECK(!is_pending_demuxer_read_);
  is_pending_demuxer_read_ = true;
  demuxer_stream_->Read(1, base::BindOnce(&CastAudioRenderer::OnNewBuffersRead,
                                          weak_factory_.GetWeakPtr()));
}

void CastAudioRenderer::OnNewBuffersRead(
    ::media::DemuxerStream::Status status,
    ::media::DemuxerStream::DecoderBufferVector buffers_queue) {
  CHECK_LE(buffers_queue.size(), 1u)
      << "CastAudioRenderer only reads a single-buffer.";
  OnNewBuffer(status,
              buffers_queue.empty() ? nullptr : std::move(buffers_queue[0]));
}

void CastAudioRenderer::OnNewBuffer(
    ::media::DemuxerStream::Status status,
    scoped_refptr<::media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_connection_);

  is_pending_demuxer_read_ = false;

  // Just discard the buffer in the flush stage.
  if (flush_cb_) {
    FlushInternal();
    std::move(flush_cb_).Run();
    return;
  }

  if (status == ::media::DemuxerStream::kAborted) {
    DCHECK(!buffer);
    return;
  }

  if (status == ::media::DemuxerStream::kError) {
    OnError(::media::PIPELINE_ERROR_READ);
    return;
  }

  if (status == ::media::DemuxerStream::kConfigChanged) {
    DCHECK(!buffer);
    UpdateAudioDecoderConfig(demuxer_stream_->audio_decoder_config());
    renderer_client_->OnAudioConfigChange(
        demuxer_stream_->audio_decoder_config());
    return;
  }

  DCHECK_EQ(status, ::media::DemuxerStream::kOk);

  size_t filled_bytes = buffer->end_of_stream() ? 0 : buffer->size();
  size_t io_buffer_size =
      audio_output_service::OutputSocket::kAudioMessageHeaderSize +
      filled_bytes;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(io_buffer_size);
  if (buffer->end_of_stream()) {
    OnEndOfStream();
    return;
  }

  last_pushed_timestamp_ = buffer->timestamp() + buffer->duration();
  memcpy(io_buffer->data() +
             audio_output_service::OutputSocket::kAudioMessageHeaderSize,
         buffer->data(), buffer->size());

  output_connection_
      .AsyncCall(&audio_output_service::OutputStreamConnection::SendAudioBuffer)
      .WithArgs(std::move(io_buffer), filled_bytes,
                buffer->timestamp().InMicroseconds());
}

void CastAudioRenderer::OnBackendInitialized(
    const audio_output_service::BackendInitializationStatus& status) {
  MAKE_SURE_MAIN_THREAD(OnBackendInitialized, status);
  DCHECK(init_cb_);

  if (!status.has_status() ||
      status.status() !=
          audio_output_service::BackendInitializationStatus::SUCCESS) {
    std::move(init_cb_).Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  SetVolume(volume_);
  std::move(init_cb_).Run(::media::PIPELINE_OK);
}

void CastAudioRenderer::OnNextBuffer(int64_t media_timestamp_microseconds,
                                     int64_t reference_timestamp_microseconds,
                                     int64_t delay_microseconds,
                                     int64_t delay_timestamp_microseconds) {
  base::AutoLock lock(timeline_lock_);
  if (GetPlaybackState() == PlaybackState::kStopped) {
    return;
  }
  if (reference_timestamp_microseconds >= 0l &&
      media_timestamp_microseconds >= 0l) {
    media_pos_ = base::Microseconds(media_timestamp_microseconds);
    reference_time_ =
        base::TimeTicks::FromInternalValue(reference_timestamp_microseconds);
    if (GetPlaybackState() == PlaybackState::kStarting) {
      SetPlaybackState(PlaybackState::kPlaying);
    }
  }
  RUN_ON_MAIN_THREAD(ScheduleFetchNextBuffer);
}

void CastAudioRenderer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  renderer_client_->OnEnded();
  is_at_end_of_stream_ = true;
  SetBufferState(::media::BUFFERING_HAVE_ENOUGH);
}

void CastAudioRenderer::OnError(::media::PipelineStatus pipeline_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_connection_.Reset();
  if (init_cb_) {
    std::move(init_cb_).Run(pipeline_status);
    return;
  }
  if (renderer_client_) {
    renderer_client_->OnError(pipeline_status);
  }
}

void CastAudioRenderer::OnDecryptingDemuxerInitialized(
    ::media::PipelineStatus pipeline_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pipeline_status != ::media::PIPELINE_OK) {
    OnError(pipeline_status);
  }
}

void CastAudioRenderer::OnWaiting(::media::WaitingReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  renderer_client_->OnWaiting(reason);
}

void CastAudioRenderer::OnApplicationMediaInfoReceived(
    ::media::mojom::CastApplicationMediaInfoPtr application_media_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  audio_output_service::CmaBackendParams backend_params;
  backend_params.mutable_application_media_info()->set_application_session_id(
      application_media_info->application_session_id);
  *(backend_params.mutable_audio_decoder_config()) =
      ConvertAudioDecoderConfig(demuxer_stream_->audio_decoder_config());

  output_connection_ =
      base::SequenceBound<audio_output_service::OutputStreamConnection>(
          AudioIoThread::Get()->task_runner(), this, std::move(backend_params),
          std::move(audio_socket_broker_pending_remote_));
  output_connection_.AsyncCall(
      &audio_output_service::OutputStreamConnection::Connect);
}

}  // namespace media
}  // namespace chromecast
