// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_pipeline_backend_starboard.h"

#include <cast_starboard_api_adapter.h>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

MediaPipelineBackendStarboard::MediaPipelineBackendStarboard(
    const MediaPipelineDeviceParams& params,
    StarboardVideoPlane* video_plane)
    : starboard_(GetStarboardApiWrapper()),
      video_plane_(video_plane),
      is_streaming_(
          params.sync_type ==
              MediaPipelineDeviceParams::MediaSyncType::kModeIgnorePts ||
          params.sync_type == MediaPipelineDeviceParams::MediaSyncType::
                                  kModeIgnorePtsAndVSync) {
  DCHECK(video_plane_);
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  media_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  video_plane_callback_token_ =
      video_plane_->RegisterCallback(base::BindPostTask(
          media_task_runner_,
          base::BindRepeating(&MediaPipelineBackendStarboard::OnGeometryChanged,
                              weak_factory_.GetWeakPtr())));

  LOG(INFO) << "Constructed a MediaPipelineBackendStarboard"
            << (is_streaming_ ? " for streaming" : "");
}

MediaPipelineBackendStarboard::~MediaPipelineBackendStarboard() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  video_plane_->UnregisterCallback(video_plane_callback_token_);
  if (player_) {
    starboard_->DestroyPlayer(player_);
  }
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendStarboard::CreateAudioDecoder() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (audio_decoder_) {
    return nullptr;
  }
  audio_decoder_.emplace(starboard_.get());
  return &*audio_decoder_;
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendStarboard::CreateVideoDecoder() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (video_decoder_) {
    return nullptr;
  }
  video_decoder_.emplace(starboard_.get());
  return &*video_decoder_;
}

bool MediaPipelineBackendStarboard::Initialize() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(starboard_->EnsureInitialized());
  state_ = State::kInitialized;
  CreatePlayer();
  return true;
}

bool MediaPipelineBackendStarboard::Start(int64_t start_pts) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, State::kInitialized);
  state_ = State::kPlaying;
  last_seek_pts_ = start_pts;

  if (!player_) {
    LOG(WARNING) << "Start was called before starboard initialization "
                    "finished. Deferring start.";
    return true;
  }

  DoSeek();
  DoPlay();
  return true;
}

void MediaPipelineBackendStarboard::DoSeek() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(player_);
  DCHECK_GE(last_seek_pts_, 0);

  if (audio_decoder_ && audio_decoder_->IsInitialized()) {
    audio_decoder_->Stop();
  }
  if (video_decoder_ && video_decoder_->IsInitialized()) {
    video_decoder_->Stop();
  }

  ++seek_ticket_;
  starboard_->SeekTo(player_, last_seek_pts_, seek_ticket_);
}

void MediaPipelineBackendStarboard::DoPlay() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(player_);

  starboard_->SetPlaybackRate(player_, playback_rate_);
}

void MediaPipelineBackendStarboard::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kPlaying || state_ == State::kPaused)
      << "Cannot call MediaPipelineBackend::Stop when in state "
      << static_cast<int>(state_);

  if (state_ == State::kPlaying) {
    DoPause();
  }
  playback_rate_ = 1.0;
  state_ = State::kInitialized;
}

bool MediaPipelineBackendStarboard::Pause() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, State::kPlaying);
  state_ = State::kPaused;

  if (!player_) {
    LOG(WARNING) << "Pause was called before starboard initialization "
                    "finished. Deferring pause.";
    return true;
  }

  DoPause();
  return true;
}

void MediaPipelineBackendStarboard::DoPause() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(player_);

  StarboardPlayerInfo info = {};
  starboard_->GetPlayerInfo(player_, &info);
  playback_rate_ = info.playback_rate;

  // Setting playback rate to 0 signifies that playback is paused.
  starboard_->SetPlaybackRate(player_, 0);
}

bool MediaPipelineBackendStarboard::Resume() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, State::kPaused);
  state_ = State::kPlaying;

  if (player_) {
    DoPlay();
  }
  return true;
}

int64_t MediaPipelineBackendStarboard::GetCurrentPts() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (player_) {
    StarboardPlayerInfo info = {};
    starboard_->GetPlayerInfo(player_, &info);
    return info.current_media_timestamp_micros;
  } else {
    return std::numeric_limits<int64_t>::min();
  }
}

bool MediaPipelineBackendStarboard::SetPlaybackRate(float rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kPlaying || state_ == State::kPaused)
      << "Cannot call MediaPipelineBackend::SetPlaybackRate when in state "
      << static_cast<int>(state_);
  DCHECK(player_);

  if (rate <= 0.0) {
    LOG(ERROR) << "Invalid playback rate: " << rate;
    return false;
  }

  if (starboard_->SetPlaybackRate(player_, rate)) {
    // Success case.
    playback_rate_ = rate;
    return true;
  }

  LOG(ERROR) << "Failed to set the playback rate in Starboard to " << rate;
  return false;
}

void MediaPipelineBackendStarboard::OnGeometryChanged(
    const RectF& display_rect,
    StarboardVideoPlane::Transform transform) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!player_) {
    LOG(INFO) << "Player was not created before OnGeometryChanged was called. "
                 "Queueing geometry change.";
    pending_geometry_change_ = display_rect;
    return;
  }

  LOG(INFO) << "Setting SbPlayer's bounds to z=0, x=" << display_rect.x
            << ", y=" << display_rect.y << ", width=" << display_rect.width
            << ", height=" << display_rect.height;
  starboard_->SetPlayerBounds(
      player_, /*z_index=*/0, static_cast<int>(display_rect.x),
      static_cast<int>(display_rect.y), static_cast<int>(display_rect.width),
      static_cast<int>(display_rect.height));
}

void MediaPipelineBackendStarboard::CreatePlayer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  StarboardPlayerCreationParam params = {};
  if (audio_decoder_) {
    const std::optional<StarboardAudioSampleInfo>& audio_info =
        audio_decoder_->GetAudioSampleInfo();
    CHECK(audio_info);
    params.audio_sample_info = *audio_info;
  }
  if (video_decoder_) {
    const std::optional<StarboardVideoSampleInfo>& video_info =
        video_decoder_->GetVideoSampleInfo();
    CHECK(video_info);
    params.video_sample_info = *video_info;
    if (is_streaming_) {
      // Note: this is not part of the official starboard API. We are using this
      // arbitrary string value to inform the starboard impl that they should
      // prioritize minimizing latency (render the frames as soon as possible).
      params.video_sample_info.max_video_capabilities = "streaming=1";
    }
  }
  params.output_mode = kStarboardPlayerOutputModePunchOut;
  player_ =
      starboard_->CreatePlayer(&params,
                               /*callback_handler=*/&player_callback_handler_);
  CHECK(player_);

  if (pending_geometry_change_) {
    OnGeometryChanged(*pending_geometry_change_,
                      StarboardVideoPlane::Transform::TRANSFORM_NONE);
    pending_geometry_change_ = std::nullopt;
  }
}

void MediaPipelineBackendStarboard::OnSampleDecoded(
    void* player,
    StarboardMediaType type,
    StarboardDecoderState decoder_state,
    int ticket) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineBackendStarboard::OnSampleDecoded,
                       weak_factory_.GetWeakPtr(), player, type, decoder_state,
                       ticket));
    return;
  }

  DCHECK_EQ(player, player_);

  if (ticket != seek_ticket_) {
    // TODO(antoniori): we may still need to trigger a
    // Delegate::OnPushBufferComplete callback here for both decoders, even if
    // there was a seek. Need to verify the expected behavior.

    LOG(INFO) << "Seek ticket mismatch";
    return;
  }

  // If a decoder is not initialized, it means this is the first OnSampleDecoded
  // call after a seek. So no buffer was pushed, but we still need to initialize
  // the decoder.
  if (type == kStarboardMediaTypeAudio) {
    DCHECK(audio_decoder_);
    if (audio_decoder_->IsInitialized()) {
      audio_decoder_->OnBufferWritten();
    } else {
      LOG(INFO) << "Initializing audio decoder";
      audio_decoder_->Initialize(player_);
    }
  } else {
    DCHECK(video_decoder_);
    if (video_decoder_->IsInitialized()) {
      video_decoder_->OnBufferWritten();
    } else {
      LOG(INFO) << "Initializing video decoder";
      video_decoder_->Initialize(player_);
    }
  }
}

void MediaPipelineBackendStarboard::DeallocateSample(
    void* player,
    const void* sample_buffer) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineBackendStarboard::DeallocateSample,
                       weak_factory_.GetWeakPtr(), player, sample_buffer));
    return;
  }

  // Unfortunately, the starboard SbPlayer API does not report the type of the
  // buffer, so we let both decoders attempt to deallocate it. Each decoder
  // knows which buffers it owns, so they will not attempt to deallocate buffers
  // that they do not own.
  if (audio_decoder_) {
    audio_decoder_->Deallocate(static_cast<const uint8_t*>(sample_buffer));
  }
  if (video_decoder_) {
    video_decoder_->Deallocate(static_cast<const uint8_t*>(sample_buffer));
  }
}

void MediaPipelineBackendStarboard::CallOnSampleDecoded(
    void* player,
    void* context,
    StarboardMediaType type,
    StarboardDecoderState decoder_state,
    int ticket) {
  static_cast<MediaPipelineBackendStarboard*>(context)->OnSampleDecoded(
      player, type, decoder_state, ticket);
}

void MediaPipelineBackendStarboard::CallDeallocateSample(
    void* player,
    void* context,
    const void* sample_buffer) {
  static_cast<MediaPipelineBackendStarboard*>(context)->DeallocateSample(
      player, sample_buffer);
}

void MediaPipelineBackendStarboard::CallOnPlayerStatus(
    void* player,
    void* context,
    StarboardPlayerState state,
    int ticket) {
  static_cast<MediaPipelineBackendStarboard*>(context)->OnPlayerStatus(
      player, state, ticket);
}

void MediaPipelineBackendStarboard::CallOnPlayerError(
    void* player,
    void* context,
    StarboardPlayerError error,
    const char* message) {
  static_cast<MediaPipelineBackendStarboard*>(context)->OnPlayerError(
      player, error, message);
}

void MediaPipelineBackendStarboard::OnPlayerStatus(void* player,
                                                   StarboardPlayerState state,
                                                   int ticket) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineBackendStarboard::OnPlayerStatus,
                       weak_factory_.GetWeakPtr(), player, state, ticket));
    return;
  }
  DCHECK_EQ(player, player_);

  LOG(INFO) << "Received starboard player status: " << state;
  if (state == kStarboardPlayerStateEndOfStream) {
    // Since playback has stopped, the decoders' delegates must be notified.
    if (audio_decoder_) {
      audio_decoder_->OnSbPlayerEndOfStream();
    }
    if (video_decoder_) {
      video_decoder_->OnSbPlayerEndOfStream();
    }
  }
}

void MediaPipelineBackendStarboard::OnPlayerError(void* player,
                                                  StarboardPlayerError error,
                                                  const std::string& message) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaPipelineBackendStarboard::OnPlayerError,
                       weak_factory_.GetWeakPtr(), player, error, message));
    return;
  }

  LOG(ERROR) << "Received starboard player error: " << error
             << ", with message " << message;
  if (error == kStarboardPlayerErrorDecode) {
    if (audio_decoder_) {
      audio_decoder_->OnStarboardDecodeError();
    }
    if (video_decoder_) {
      video_decoder_->OnStarboardDecodeError();
    }
  }
}

void MediaPipelineBackendStarboard::TestOnlySetStarboardApiWrapper(
    std::unique_ptr<StarboardApiWrapper> starboard) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << "Replacing the StarboardApiWrapper used by "
               "MediaPipelineBackendStarboard and decoders. This should only "
               "happen in tests";
  starboard_ = std::move(starboard);
}

}  // namespace media
}  // namespace chromecast
