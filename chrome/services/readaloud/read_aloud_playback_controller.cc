// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/pass_key.h"
#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/synthesis_response_parser.h"
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
        receiver,
    AudioRendererFactory audio_renderer_factory)
    : receiver_(this, std::move(receiver)),
      audio_renderer_factory_(std::move(audio_renderer_factory)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&ReadAloudPlaybackController::OnReceiverDisconnected,
                     factory_weak_factory_.GetWeakPtr()));
  // `decoder_sequencer_` is owned by `this`, so it cannot outlive the callback
  // target.
  decoder_sequencer_.SetPumpStatusCallback(
      base::BindRepeating(&ReadAloudPlaybackController::OnPumpStatusChanged,
                          base::Unretained(this)));
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
  resources.audio_renderer = audio_renderer_factory_
                                 ? audio_renderer_factory_.Run()
                                 : std::make_unique<ReadAloudAudioRenderer>();

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

  MaybePlayOnReady();
}

void ReadAloudPlaybackController::SetPlaybackMode(
    read_aloud::mojom::PlaybackMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  playback_mode_ = mode;
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
    if (i > 0 && segment->segment_index <= last_index) {
      controller_receiver_.ReportBadMessage(
          "ReadAloudPlaybackController: segment_index must be "
          "monotonically increasing in SetTextContent");
      return;
    }
    last_index = segment->segment_index;

    if (segment->text.empty()) {
      continue;
    }
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
  if (!MaybePlayOnReady()) {
    // When new text content is loaded, playback defaults to paused until the
    // user explicitly triggers Play(). Notify client to synchronize UI state.
    SetPlaybackState(read_aloud::mojom::PlaybackState::kPaused);
  }
}

void ReadAloudPlaybackController::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!PlayIfReady()) {
    play_on_ready_ = true;
    base::TimeDelta timeout =
        (playback_mode_ == read_aloud::mojom::PlaybackMode::kOverview)
            ? kOverviewPlayOnReadyTimeout
            : kClassicPlayOnReadyTimeout;
    // Restart watchdog timer on each Play() call to grant a fresh window
    // from the last click.
    play_on_ready_timer_.Start(
        FROM_HERE, timeout,
        base::BindOnce(&ReadAloudPlaybackController::OnPlayOnReadyTimeout,
                       base::Unretained(this)));
  }
}

void ReadAloudPlaybackController::OnPlayOnReadyTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!play_on_ready_) {
    return;
  }
  play_on_ready_ = false;

  // Defensive Guard: Do not emit kPaused if playback is actively pumping audio.
  if (!decoder_sequencer_.is_pumping()) {
    SetPlaybackState(read_aloud::mojom::PlaybackState::kPaused);
  }
}

bool ReadAloudPlaybackController::IsReadyToPlay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsTextSet() && IsAudioInitialized();
}

bool ReadAloudPlaybackController::IsTextSet() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !segments_.empty();
}

bool ReadAloudPlaybackController::IsAudioInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return audio_resources_ && audio_resources_->audio_output_stream.is_bound();
}

bool ReadAloudPlaybackController::PlayIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsReadyToPlay()) {
    return false;
  }
  play_on_ready_ = false;
  play_on_ready_timer_.Stop();
  if (audio_resources_ && audio_resources_->audio_output_stream.is_bound()) {
    audio_resources_->audio_output_stream->Play();
  }
  // Deliberately does not report kPlaying here. Per b/562011435 the utility
  // must not claim playback has started until audio frames are actually ready,
  // otherwise the browser dismisses its loading UI over silence. StartPumping()
  // replenishes synchronously and resolves the state through
  // OnPumpStatusChanged() based on what is genuinely buffered.
  decoder_sequencer_.StartPumping();
  return true;
}

bool ReadAloudPlaybackController::MaybePlayOnReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (play_on_ready_) {
    return PlayIfReady();
  }
  return false;
}

void ReadAloudPlaybackController::SetPlaybackState(
    read_aloud::mojom::PlaybackState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (playback_state_ == state) {
    return;
  }
  playback_state_ = state;
  if (client_.is_bound()) {
    client_->OnPlaybackStateChanged(state);
  }
}

void ReadAloudPlaybackController::OnPumpStatusChanged(
    ReadAloudDecoderSequencer::PumpStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (status) {
    case ReadAloudDecoderSequencer::PumpStatus::kFlowing:
      SetPlaybackState(read_aloud::mojom::PlaybackState::kPlaying);
      return;
    case ReadAloudDecoderSequencer::PumpStatus::kStarved:
      SetPlaybackState(read_aloud::mojom::PlaybackState::kBuffering);
      return;
    case ReadAloudDecoderSequencer::PumpStatus::kDrained:
      // The document finished normally. Nothing is left to decode, so stop
      // pumping. Report kPaused rather than a terminal state so the Read Aloud
      // UI stays open and the user can replay. The output stream is
      // intentionally left running and the queue is not flushed, so the
      // segment the renderer just popped still plays out.
      //
      // TODO(b/565419447): Drive this from a real end-of-stream signal rather
      // than inferring it from the segment queue draining. Draining only means
      // the renderer has taken the last segment, not that it has played it:
      // audio still held in the renderer algorithm buffer and the output
      // stream buffer is audible, so kPaused lands early, while the 250ms pump
      // period pushes detection late. The net error is on the order of a few
      // hundred milliseconds. A precise signal needs end-of-stream detection
      // inside ReadAloudAudioRenderer, which zero-fills on underflow today.
      // Once available, this should call HaltPlayback(kPaused) instead.
      decoder_sequencer_.StopPumping();
      SetPlaybackState(read_aloud::mojom::PlaybackState::kPaused);
      return;
    case ReadAloudDecoderSequencer::PumpStatus::kFailed:
      // The entire timeline was consumed without ever yielding a single audio
      // segment, so this session can no longer produce sound.
      HaltPlayback(read_aloud::mojom::PlaybackState::kError);
      return;
  }
}

void ReadAloudPlaybackController::HaltPlayback(
    read_aloud::mojom::PlaybackState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  play_on_ready_ = false;
  play_on_ready_timer_.Stop();
  if (audio_resources_ && audio_resources_->audio_output_stream.is_bound()) {
    audio_resources_->audio_output_stream->Pause();
  }
  // Stop pumping before reporting so that no in-flight pump status can
  // overwrite `state`, and so that a later seek cannot silently resume
  // playback.
  decoder_sequencer_.StopPumping();
  SetPlaybackState(state);
}

void ReadAloudPlaybackController::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HaltPlayback(read_aloud::mojom::PlaybackState::kPaused);
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
  // When not actively playing (paused, drained or errored), make sure the
  // stream is paused too, so that audio decoded for the new position is not
  // rendered until the next Play().
  // TODO(b/565419447): Remove once end of document is driven by a real
  // end-of-stream signal that pauses the audio output stream. The stream is
  // then paused whenever the pump is stopped, making this guard redundant.
  if (!decoder_sequencer_.is_pumping() && audio_resources_ &&
      audio_resources_->audio_output_stream.is_bound()) {
    audio_resources_->audio_output_stream->Pause();
  }
  decoder_sequencer_.SetNextChunkToDecode(segment_index);
}

void ReadAloudPlaybackController::SeekToTime(base::TimeDelta position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (position.is_negative() || position.is_max()) {
    controller_receiver_.ReportBadMessage(
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
  prefetch_manager_.ClearCache();
  decoder_sequencer_.Reset();

  if (!audio_resources_) {
    return;
  }

  if (audio_resources_->audio_segment_queue) {
    audio_resources_->audio_segment_queue->Clear(
        base::PassKey<ReadAloudPlaybackController>());
  }
  if (audio_resources_->audio_renderer) {
    audio_resources_->audio_renderer->Flush();
  }
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
  playback_mode_ = read_aloud::mojom::PlaybackMode::kClassic;
  // A new session starts with no state reported yet, so the first transition
  // is always forwarded to the newly bound client.
  playback_state_.reset();
  play_on_ready_ = false;
  play_on_ready_timer_.Stop();
  session_weak_factory_.InvalidateWeakPtrs();
}

void ReadAloudPlaybackController::OnPrefetchSynthesisRequest(
    uint32_t chunk_index,
    std::u16string_view text,
    read_aloud::mojom::Speaker speaker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t sequence_id = prefetch_manager_.GetCurrentSequenceId();
  if (!client_.is_bound()) {
    prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index, nullptr,
                                          {});
    return;
  }
  client_->RequestSpeechSynthesis(
      std::u16string(text), speaker, sequence_id,
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
  // A single failed or undecodable chunk is recoverable: the sequencer skips
  // it and keeps playing the rest of the document, so no error state is
  // reported here. Only a document that never produces any audio at all is
  // surfaced as kError, via OnPumpStatusChanged().
  if (!success) {
    prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index, nullptr,
                                          {});
    decoder_sequencer_.ReplenishBuffer();
    return;
  }

  const std::vector<TextChunk>& timeline = prefetch_manager_.GetTimelineChunks();
  if (chunk_index >= timeline.size()) {
    prefetch_manager_.OnSynthesisResponse(sequence_id, chunk_index, nullptr,
                                          {});
    decoder_sequencer_.ReplenishBuffer();
    return;
  }
  const TextChunk& chunk = timeline[chunk_index];

  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(response_bytes), chunk);

  prefetch_manager_.OnSynthesisResponse(
      sequence_id, chunk_index, std::move(result.audio_buffer),
      std::move(result.timings));

  decoder_sequencer_.ReplenishBuffer();
}

}  // namespace readaloud
