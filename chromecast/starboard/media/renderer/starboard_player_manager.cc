// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/starboard_player_manager.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/renderer/chromium_starboard_conversions.h"

namespace chromecast {
namespace media {

std::unique_ptr<StarboardPlayerManager> StarboardPlayerManager::Create(
    StarboardApiWrapper* starboard,
    ::media::DemuxerStream* audio_stream,
    ::media::DemuxerStream* video_stream,
    ::media::RendererClient* client,
    chromecast::metrics::CastMetricsHelper* cast_metrics_helper,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    bool enable_buffering) {
  if ((!audio_stream && !video_stream) || !starboard || !client ||
      !cast_metrics_helper || !media_task_runner) {
    return nullptr;
  }

  // These objects need to outlive the call to CreatePlayer, since
  // creation_param might reference pointers derived from vectors stored in
  // these objects (for extra_data).
  ::media::AudioDecoderConfig audio_config;
  ::media::VideoDecoderConfig video_config;

  std::optional<StarboardAudioSampleInfo> audio_sample_info;
  std::optional<StarboardVideoSampleInfo> video_sample_info;

  chromecast::media::StarboardPlayerCreationParam creation_param = {};
  creation_param.output_mode =
      StarboardPlayerOutputMode::kStarboardPlayerOutputModePunchOut;

  bool stream_encrypted = false;
  if (audio_stream) {
    audio_stream->EnableBitstreamConverter();
    audio_config = audio_stream->audio_decoder_config();
    audio_sample_info = ToStarboardAudioSampleInfo(audio_config);
    if (!audio_sample_info) {
      LOG(ERROR) << "Invalid or unsupported audio config: "
                 << audio_config.AsHumanReadableString();
      return nullptr;
    }

    LOG(INFO) << "Initial audio config: "
              << audio_config.AsHumanReadableString();
    creation_param.audio_sample_info = *audio_sample_info;

    if (audio_config.is_encrypted()) {
      stream_encrypted = true;
    }
  }

  if (video_stream) {
    // Convert to H264 and HEVC content to annex-b form, since that's the form
    // that Starboard requires.
    video_stream->EnableBitstreamConverter();
    video_config = video_stream->video_decoder_config();
    video_sample_info = ToStarboardVideoSampleInfo(video_config);
    if (!video_sample_info) {
      LOG(ERROR) << "Invalid or unsupported video config: "
                 << video_config.AsHumanReadableString();
      return nullptr;
    }

    LOG(INFO) << "Initial video config: "
              << video_config.AsHumanReadableString();
    creation_param.video_sample_info = *video_sample_info;

    if (!enable_buffering) {
      // Note: this is not part of the official starboard API. We are using this
      // arbitrary string value to inform the starboard impl that they should
      // prioritize minimizing latency (render the frames as soon as possible).
      creation_param.video_sample_info.max_video_capabilities = "streaming=1";
    }

    if (video_config.is_encrypted()) {
      stream_encrypted = true;
    }
  }

  std::optional<StarboardDrmWrapper::DrmSystemResource> drm_resource;
  const bool cdm_exists = StarboardDrmWrapper::GetInstance().HasClients();
  if (stream_encrypted || cdm_exists) {
    if (stream_encrypted && !cdm_exists) {
      // This case might happen if there's a race between the JS app creating a
      // MediaKeys object and playback starting.
      LOG(WARNING) << "Content is encrypted, but no CDM exists. Passing an "
                      "SbDrmSystem to SbPlayerCreate regardless";
    } else if (!stream_encrypted && cdm_exists) {
      // This case might happen if ads play before the main content.
      LOG(WARNING) << "Content is not encrypted, but a CDM exists. Passing an "
                      "SbDrmSystem to SbPlayerCreate regardless";
    } else {
      // Standard case.
      LOG(INFO)
          << "Content is encrypted and a CDM exists. Using an SbDrmSystem.";
    }

    drm_resource.emplace();
    creation_param.drm_system =
        StarboardDrmWrapper::GetInstance().GetDrmSystem();
  } else {
    LOG(INFO) << "Content is not encrypted and no CDM exists. Passing a null "
                 "SbDrmSystem to SbPlayerCreate";
    creation_param.drm_system = nullptr;
  }

  // base::WrapUnique is necessary because we're calling a private ctor.
  auto starboard_player_manager = base::WrapUnique(new StarboardPlayerManager(
      std::move(drm_resource), starboard, audio_stream, video_stream,
      std::move(audio_sample_info), std::move(video_sample_info), client,
      cast_metrics_helper, std::move(media_task_runner)));

  starboard->EnsureInitialized();
  void* sb_player = starboard->CreatePlayer(
      &creation_param, &starboard_player_manager->callback_handler_);

  if (!sb_player) {
    LOG(ERROR) << "Could not create SbPlayer";
    return nullptr;
  }

  base::RepeatingCallback<base::TimeDelta()> get_media_time =
      base::BindRepeating(
          [](StarboardApiWrapper* starboard, void* sb_player) {
            StarboardPlayerInfo player_info = {};
            starboard->GetPlayerInfo(sb_player, &player_info);
            return base::Microseconds(
                player_info.current_media_timestamp_micros);
          },
          starboard, sb_player);

  starboard_player_manager->player_ = sb_player;
  starboard_player_manager->buffering_tracker_.emplace(
      std::move(get_media_time), cast_metrics_helper);

  return starboard_player_manager;
}

StarboardPlayerManager::StarboardPlayerManager(
    std::optional<StarboardDrmWrapper::DrmSystemResource> drm_resource,
    StarboardApiWrapper* starboard,
    ::media::DemuxerStream* audio_stream,
    ::media::DemuxerStream* video_stream,
    std::optional<StarboardAudioSampleInfo> audio_sample_info,
    std::optional<StarboardVideoSampleInfo> video_sample_info,
    ::media::RendererClient* client,
    chromecast::metrics::CastMetricsHelper* cast_metrics_helper,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    :  // base::Unretained(this) is safe here because demuxer_stream_reader_
       // will be destroyed before `this`.
      drm_resource_(std::move(drm_resource)),
      starboard_(starboard),
      client_(client),
      stats_tracker_(client, cast_metrics_helper),
      task_runner_(std::move(media_task_runner)),
      demuxer_stream_reader_(
          audio_stream,
          video_stream,
          std::move(audio_sample_info),
          std::move(video_sample_info),
          /*handle_buffer_cb=*/
          base::BindRepeating(&StarboardPlayerManager::PushBuffer,
                              base::Unretained(this)),
          base::BindRepeating(&StarboardPlayerManager::PushEos,
                              base::Unretained(this)),
          client_,
          cast_metrics_helper),
      cast_metrics_helper_(cast_metrics_helper) {
  CHECK(starboard_);
  CHECK(client_);
  CHECK(task_runner_);
  CHECK(cast_metrics_helper_);
  // player_ is set later in the factory function to create
  // StarboardPlayerManager.
}

StarboardPlayerManager::~StarboardPlayerManager() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (player_) {
    starboard_->DestroyPlayer(player_);
  }
}

void StarboardPlayerManager::PushBuffer(
    int seek_ticket,
    StarboardSampleInfo sample_info,
    scoped_refptr<::media::DecoderBuffer> buffer) {
  CHECK(player_);
  if (seek_ticket != seek_ticket_) {
    LOG(INFO) << "Ignoring buffer for old seek ticket (expected "
              << seek_ticket_ << ", got " << seek_ticket << ")";
    return;
  }

  starboard_->WriteSample(player_,
                          static_cast<StarboardMediaType>(sample_info.type),
                          base::span_from_ref(sample_info));
  CHECK(addr_to_buffer_.insert({sample_info.buffer, std::move(buffer)}).second)
      << "Attempted to insert a buffer that already exists, at address: "
      << sample_info.buffer;

  buffering_tracker_->OnBufferPush();
  UpdateStats(sample_info);
}

void StarboardPlayerManager::UpdateStats(
    const StarboardSampleInfo& sample_info) {
  StarboardPlayerInfo player_info = {};
  starboard_->GetPlayerInfo(player_, &player_info);

  stats_tracker_.UpdateStats(player_info, sample_info);
}

void StarboardPlayerManager::PushEos(int seek_ticket, StarboardMediaType type) {
  CHECK(player_);
  if (seek_ticket != seek_ticket_) {
    LOG(INFO) << "Ignoring end of stream for old seek ticket (expected "
              << seek_ticket_ << ", got " << seek_ticket << ")";
    return;
  }
  starboard_->WriteEndOfStream(player_, type);
}

void StarboardPlayerManager::StartPlayingFrom(base::TimeDelta time) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(player_);
  LOG(INFO) << "StartPlayingFrom: " << time;
  flushing_ = false;
  LOG(INFO) << "Setting playback rate to " << playback_rate_;
  // In case this is the first call to StartPlayingFrom, or if this is called
  // after a flush, ensure that we have the correct rate set before seeking.
  starboard_->SetPlaybackRate(player_, playback_rate_);
  starboard_->SeekTo(player_, time.InMicroseconds(), ++seek_ticket_);
  buffering_tracker_->SetPlaybackRate(playback_rate_);
}

void StarboardPlayerManager::Flush() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(player_);
  LOG(INFO) << "StarboardPlayerManager::Flush";
  flushing_ = true;

  // Setting the playback rate to 0 pauses playback.
  starboard_->SetPlaybackRate(player_, 0.0);
  buffering_tracker_->SetPlaybackRate(0.0);

  StarboardPlayerInfo player_info = {};
  starboard_->GetPlayerInfo(player_, &player_info);

  // Seeking causes starboard to flush its pipeline.
  starboard_->SeekTo(player_, player_info.current_media_timestamp_micros,
                     ++seek_ticket_);
}

void StarboardPlayerManager::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(player_);
  LOG(INFO) << "SetPlaybackRate: " << playback_rate;
  playback_rate_ = playback_rate;
  starboard_->SetPlaybackRate(player_, playback_rate);
  buffering_tracker_->SetPlaybackRate(playback_rate);
}

void StarboardPlayerManager::SetVolume(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(player_);
  LOG(INFO) << "StarboardPlayerManager::SetVolume: " << volume;
  starboard_->SetVolume(player_, volume);
}

base::TimeDelta StarboardPlayerManager::GetMediaTime() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(player_);
  StarboardPlayerInfo player_info = {};
  starboard_->GetPlayerInfo(player_, &player_info);
  return base::Microseconds(player_info.current_media_timestamp_micros);
}

void* StarboardPlayerManager::GetSbPlayer() {
  return player_;
}

void StarboardPlayerManager::OnDecoderStatus(
    void* player,
    StarboardMediaType type,
    StarboardDecoderState decoder_state,
    int ticket) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardPlayerManager::OnDecoderStatus,
                                  weak_factory_.GetWeakPtr(), player, type,
                                  decoder_state, ticket));
    return;
  }

  if (flushing_) {
    LOG(INFO) << "Ignoring call for data from Starboard, because the pipeline "
                 "is flushing.";
    return;
  }
  if (ticket != seek_ticket_) {
    LOG(INFO) << "Ignoring call for data from Starboard, because the seek "
                 "ticket does not match ("
              << ticket << " vs expected " << seek_ticket_ << ")";
    return;
  }

  demuxer_stream_reader_.ReadBuffer(seek_ticket_, type);
}

void StarboardPlayerManager::DeallocateSample(void* player,
                                              const void* sample_buffer) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardPlayerManager::DeallocateSample,
                       weak_factory_.GetWeakPtr(), player, sample_buffer));
    return;
  }

  addr_to_buffer_.erase(sample_buffer);
}

void StarboardPlayerManager::OnPlayerStatus(
    void* player,
    chromecast::media::StarboardPlayerState state,
    int ticket) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardPlayerManager::OnPlayerStatus,
                       weak_factory_.GetWeakPtr(), player, state, ticket));
    return;
  }

  DCHECK_EQ(player, player_);
  LOG(INFO) << "Received SbPlayer state: " << state;
  switch (state) {
    case StarboardPlayerState::kStarboardPlayerStateEndOfStream: {
      client_->OnEnded();
      break;
    }
    case StarboardPlayerState::kStarboardPlayerStatePresenting: {
      client_->OnBufferingStateChange(
          ::media::BufferingState::BUFFERING_HAVE_ENOUGH,
          ::media::BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
      break;
    }
    default: {
      break;
    }
  }

  buffering_tracker_->OnPlayerStatus(state);
}

void StarboardPlayerManager::OnPlayerError(
    void* player,
    chromecast::media::StarboardPlayerError error,
    std::string message) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardPlayerManager::OnPlayerError,
                       weak_factory_.GetWeakPtr(), player, error, message));
    return;
  }

  DCHECK_EQ(player, player_);
  LOG(ERROR) << "Received SbPlayer error " << error
             << ", with message: " << message;

  // PIPELINE_ERROR_HARDWARE_CONTEXT_RESET roughly corresponds to Starboard's
  // kSbPlayerErrorCapabilityChanged. It signifies that the system should
  // recreate the renderer instead of just giving up (since capabilities
  // changed, e.g. due to a hardware change like unplugging an external GPU).
  //
  // See
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/pipeline_impl.cc;l=986;drc=1fb4c56b03b105b03c45627871b15b8933ed8a11
  // and
  // https://github.com/youtube/cobalt/blob/6a2df0d123c68a3a29555dedf25fdbc5d161b3c9/starboard/player.h#L69
  const ::media::PipelineStatusCodes pipeline_error =
      error == StarboardPlayerError::kStarboardPlayerErrorDecode
          ? ::media::PIPELINE_ERROR_DECODE
          : ::media::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET;
  cast_metrics_helper_->RecordApplicationEventWithValue("Cast.Platform.Error",
                                                        pipeline_error);
  client_->OnError(pipeline_error);
}

void StarboardPlayerManager::CallOnDecoderStatus(
    void* player,
    void* context,
    chromecast::media::StarboardMediaType type,
    chromecast::media::StarboardDecoderState decoder_state,
    int ticket) {
  reinterpret_cast<StarboardPlayerManager*>(context)->OnDecoderStatus(
      player, type, decoder_state, ticket);
}

void StarboardPlayerManager::CallDeallocateSample(void* player,
                                                  void* context,
                                                  const void* sample_buffer) {
  reinterpret_cast<StarboardPlayerManager*>(context)->DeallocateSample(
      player, sample_buffer);
}

void StarboardPlayerManager::CallOnPlayerStatus(
    void* player,
    void* context,
    chromecast::media::StarboardPlayerState state,
    int ticket) {
  reinterpret_cast<StarboardPlayerManager*>(context)->OnPlayerStatus(
      player, state, ticket);
}

void StarboardPlayerManager::CallOnPlayerError(
    void* player,
    void* context,
    chromecast::media::StarboardPlayerError error,
    std::string message) {
  reinterpret_cast<StarboardPlayerManager*>(context)->OnPlayerError(
      player, error, message);
}

}  // namespace media
}  // namespace chromecast
