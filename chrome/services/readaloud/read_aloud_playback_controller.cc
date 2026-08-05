// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/message.h"

namespace readaloud {

ReadAloudPlaybackController::ReadAloudPlaybackController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
        receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&ReadAloudPlaybackController::OnReceiverDisconnected,
                     factory_weak_factory_.GetWeakPtr()));
}

ReadAloudPlaybackController::~ReadAloudPlaybackController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReadAloudPlaybackController::CreateController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
        controller,
    mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
        client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!controller.is_valid() || !client.is_valid()) {
    receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: CreateController requires both "
        "controller and client handles to be valid");
    return;
  }
  ResetSession();
  controller_receiver_.Bind(std::move(controller));
  controller_receiver_.set_disconnect_handler(
      base::BindOnce(&ReadAloudPlaybackController::OnControllerDisconnected,
                     session_weak_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));
  client_.set_disconnect_handler(
      base::BindOnce(&ReadAloudPlaybackController::OnClientDisconnected,
                     session_weak_factory_.GetWeakPtr()));
}

void ReadAloudPlaybackController::InitializeAudio(
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/527526096): Bind AudioOutputStream and store data_pipe for playback.
}

void ReadAloudPlaybackController::SetTextContent(
    std::vector<read_aloud::mojom::TextSegmentPtr> segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (segments.size() > kMaxTextSegments) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Too many segments in SetTextContent");
    return;
  }
  size_t total_text_bytes = 0;
  uint32_t last_index = 0;
  for (size_t i = 0; i < segments.size(); ++i) {
    const read_aloud::mojom::TextSegmentPtr& segment = segments[i];
    if (!segment) {
      controller_receiver_.ReportBadMessage(
          "ReadAloudPlaybackController: Null TextSegment in SetTextContent");
      return;
    }
    if (segment->text.empty()) {
      continue;
    }
    if (i > 0 && segment->segment_index <= last_index) {
      controller_receiver_.ReportBadMessage(
          "ReadAloudPlaybackController: segment_index must be "
          "monotonically increasing in SetTextContent");
      return;
    }
    last_index = segment->segment_index;
    if (segment->text.size() > kMaxTextLengthPerSegment) {
      controller_receiver_.ReportBadMessage(
          "ReadAloudPlaybackController: TextSegment length exceeds limit in "
          "SetTextContent");
      return;
    }
    total_text_bytes += segment->text.size() *
                        sizeof(std::remove_reference_t<
                               decltype(segment->text)>::value_type);
  }
  if (total_text_bytes > kMaxMojoPayloadSizeBytes) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Total text payload exceeds safety limit "
        "in SetTextContent");
    return;
  }
  segments_ = std::move(segments);
  // Initialize document-bound prefetch cache and canonical sentence timeline.
  prefetch_manager_.SetTextContent(segments_);
  // Setting new text content invalidates pending audio synthesis buffers from
  // the previous document segment, so FlushBuffers() resets internal queues.
  FlushBuffers();
  if (client_.is_bound()) {
    // When new text content is loaded, playback defaults to paused until the
    // user explicitly triggers Play(). Notify client to synchronize UI state.
    client_->OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused);
  }
}

void ReadAloudPlaybackController::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/527526634): Start reading from icu_chunker and pushing audio frames.
}

void ReadAloudPlaybackController::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/527526634): Pause audio stream and suspend playback timer.
}

void ReadAloudPlaybackController::SeekToWord(uint32_t segment_index,
                                             uint32_t character_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::lower_bound(
      segments_.begin(), segments_.end(), segment_index,
      [](const read_aloud::mojom::TextSegmentPtr& segment, uint32_t index) {
        return segment->segment_index < index;
      });
  if (it == segments_.end() || (*it)->segment_index != segment_index) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid segment_index in SeekToWord");
    return;
  }
  const std::u16string_view text = (*it)->text;
  if (character_offset > text.size()) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid character_offset in SeekToWord");
    return;
  }
  // TODO(b/527526634): Update current segment and character pointer in chunker.
}

void ReadAloudPlaybackController::SeekToTime(base::TimeDelta position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (position.is_negative() || position.is_max()) {
    mojo::ReportBadMessage(
        "ReadAloudPlaybackController: Invalid position in SeekToTime");
    return;
  }
  // TODO(b/527525845): Once total audio duration is known, add upper bounds
  // check against total_duration_ (or base::TimeDelta::Max()) before seeking.
  // TODO(b/527525845): Map TimeDelta position to approximate text offset and
  // seek.
}

void ReadAloudPlaybackController::SetVoice(const std::string& voice_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (voice_id.size() > kMaxVoiceIdLength) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Voice ID exceeds maximum allowed length");
    return;
  }
  // TODO(b/527526634): Validate voice_id and configure synthesis engine voice.
}

void ReadAloudPlaybackController::SetPlaybackRate(float rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!std::isfinite(rate) || rate <= 0.0f) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid playback rate (must be finite "
        "and > 0.0)");
    return;
  }
  float clamped_rate = std::clamp(rate, kMinPlaybackRate, kMaxPlaybackRate);
  playback_rate_ = clamped_rate;
  // TODO(b/527526021): Apply playback_rate_ to active audio output stream /
  // synthesis parameters.
}

void ReadAloudPlaybackController::FlushBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/527525940): Flush pending data in data_pipe and reset chunker state.
}

void ReadAloudPlaybackController::OnReceiverDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetSession();
  receiver_.reset();
}

void ReadAloudPlaybackController::OnControllerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetSession();
}

void ReadAloudPlaybackController::OnClientDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetSession();
}

void ReadAloudPlaybackController::ResetSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  controller_receiver_.reset();
  client_.reset();
  // TODO(b/527526096): Reset stored audio stream and data pipe handles:
  // audio_output_stream_.reset();
  // audio_data_pipe_.reset();
  prefetch_manager_.ResetSession();
  segments_.clear();
  playback_rate_ = 1.0f;
  session_weak_factory_.InvalidateWeakPtrs();
}

}  // namespace readaloud
