// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "media/audio/audio_device_thread.h"
#include "media/audio/audio_output_device_thread_callback.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace readaloud {

ReadAloudPlaybackController::AudioResources::AudioResources() = default;
ReadAloudPlaybackController::AudioResources::AudioResources(AudioResources&&) =
    default;
ReadAloudPlaybackController::AudioResources&
ReadAloudPlaybackController::AudioResources::operator=(AudioResources&&) =
    default;
ReadAloudPlaybackController::AudioResources::~AudioResources() = default;

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
  decoder_sequencer_.SetAudioQueue(nullptr);
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

  prefetch_manager_.SetRequestSynthesisCallback(base::BindRepeating(
      &ReadAloudPlaybackController::OnPrefetchSynthesisRequest,
      session_weak_factory_.GetWeakPtr()));
  prefetch_manager_.SetOnTextChunkedCallback(
      base::BindRepeating(&ReadAloudPlaybackController::OnTextChunked,
                          session_weak_factory_.GetWeakPtr()));
}

void ReadAloudPlaybackController::InitializeAudio(
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe,
    const media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Immediately tear down any active audio resources and threads before
  // initializing new ones or validating parameters.
  decoder_sequencer_.SetAudioQueue(nullptr);
  audio_resources_.reset();

  if (!stream.is_valid()) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid audio output stream remote");
    return;
  }

  if (!params.IsValid() || params.IsBitstreamFormat()) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid audio parameters");
    return;
  }

  if (!data_pipe || !data_pipe->socket.is_valid() ||
      !data_pipe->shared_memory.IsValid()) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid data pipe or handles");
    return;
  }

  size_t required_buffer_size = media::ComputeAudioOutputBufferSize(params);
  if (data_pipe->shared_memory.GetSize() < required_buffer_size) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Shared memory size is too small");
    return;
  }

  AudioResources resources;
  resources.audio_segment_queue = std::make_unique<AudioSegmentQueue>();
  resources.audio_renderer = std::make_unique<ReadAloudAudioRenderer>();

  if (!resources.audio_renderer->Initialize(
          params, resources.audio_segment_queue.get())) {
    return;
  }
  resources.audio_renderer->SetPlaybackRate(playback_rate_);

  resources.audio_output_stream.Bind(std::move(stream));

  base::ScopedPlatformFile socket_file =
      std::move(data_pipe->socket).TakePlatformFile();
  if (!socket_file.is_valid()) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Failed to take socket handle");
    return;
  }

  resources.audio_callback =
      std::make_unique<media::AudioOutputDeviceThreadCallback>(
          params, std::move(data_pipe->shared_memory),
          resources.audio_renderer.get());
  resources.audio_callback->InitializePlayStartTime();

  resources.audio_thread = std::make_unique<media::AudioDeviceThread>(
      resources.audio_callback.get(), std::move(socket_file),
      "ReadAloudAudioPlayback", base::ThreadType::kRealtimeAudio);

  audio_resources_ = std::move(resources);
  decoder_sequencer_.SetAudioQueue(audio_resources_->audio_segment_queue.get());
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
  if (audio_resources_ && audio_resources_->audio_output_stream.is_bound()) {
    audio_resources_->audio_output_stream->Play();
  }
  decoder_sequencer_.StartPumping();
}

void ReadAloudPlaybackController::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_resources_ && audio_resources_->audio_output_stream.is_bound()) {
    audio_resources_->audio_output_stream->Pause();
  }
  decoder_sequencer_.StopPumping();
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
  decoder_sequencer_.SetNextChunkToDecode(segment_index);
}

void ReadAloudPlaybackController::SeekToTime(base::TimeDelta position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (position.is_negative() || position.is_max()) {
    mojo::ReportBadMessage(
        "ReadAloudPlaybackController: Invalid position in SeekToTime");
    return;
  }
}

void ReadAloudPlaybackController::SetVoice(const std::string& voice_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (voice_id.size() > kMaxVoiceIdLength) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Voice ID exceeds maximum allowed length");
    return;
  }
}

void ReadAloudPlaybackController::SetPlaybackRate(float rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!std::isfinite(rate) || rate <= 0.0f) {
    controller_receiver_.ReportBadMessage(
        "ReadAloudPlaybackController: Invalid playback rate (must be finite "
        "and > 0.0)");
    return;
  }
  playback_rate_ = std::clamp(rate, kMinPlaybackRate, kMaxPlaybackRate);
  if (audio_resources_ && audio_resources_->audio_renderer) {
    audio_resources_->audio_renderer->SetPlaybackRate(playback_rate_);
  }
}

void ReadAloudPlaybackController::FlushBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_resources_ && audio_resources_->audio_segment_queue) {
    audio_resources_->audio_segment_queue->Clear(
        base::PassKey<ReadAloudPlaybackController>());
  }
  prefetch_manager_.ClearCache();
  decoder_sequencer_.Reset();
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
  prefetch_manager_.ResetSession();
  decoder_sequencer_.Reset();
  decoder_sequencer_.SetAudioQueue(nullptr);
  audio_resources_.reset();
  segments_.clear();
  playback_rate_ = 1.0f;
  session_weak_factory_.InvalidateWeakPtrs();
}

void ReadAloudPlaybackController::OnPrefetchSynthesisRequest(
    uint32_t chunk_index,
    std::u16string_view text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t sequence_id = prefetch_manager_.GetCurrentSequenceId();
  if (!client_.is_bound()) {
    prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index, nullptr,
                                          {});
    return;
  }
  client_->RequestSpeechSynthesis(
      std::u16string(text), sequence_id,
      base::BindOnce(&ReadAloudPlaybackController::OnSpeechSynthesisResponse,
                     session_weak_factory_.GetWeakPtr(), sequence_id,
                     chunk_index));
}

void ReadAloudPlaybackController::OnTextChunked(
    const std::vector<std::u16string>& chunks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_.is_bound()) {
    client_->OnTextChunked(chunks);
  }
}

void ReadAloudPlaybackController::OnSpeechSynthesisResponse(
    uint64_t sequence_id,
    uint32_t chunk_index,
    mojo_base::BigBuffer response_bytes,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success || response_bytes.size() == 0) {
    prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index, nullptr,
                                          {});
    return;
  }
  scoped_refptr<media::DecoderBuffer> opus_buffer =
      media::DecoderBuffer::CopyFrom(base::span(response_bytes));
  prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index,
                                        std::move(opus_buffer), {});

  decoder_sequencer_.ReplenishBuffer();
}

}  // namespace readaloud
