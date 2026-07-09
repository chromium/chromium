// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <utility>

namespace readaloud {

ReadAloudPlaybackController::ReadAloudPlaybackController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
        receiver)
    : receiver_(this, std::move(receiver)) {}

ReadAloudPlaybackController::~ReadAloudPlaybackController() = default;

void ReadAloudPlaybackController::CreateController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
        controller,
    mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
        client) {
  controller_receiver_.reset();
  controller_receiver_.Bind(std::move(controller));
  client_.reset();
  client_.Bind(std::move(client));
}

void ReadAloudPlaybackController::InitializeAudio(
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {}

void ReadAloudPlaybackController::SetTextContent(
    std::vector<read_aloud::mojom::TextSegmentPtr> segments) {
  // TODO(b/529882158): Validate segment indices (e.g. non-negative) to prevent
  // out-of-bounds reads/writes before storing.
}

void ReadAloudPlaybackController::Play() {}

void ReadAloudPlaybackController::Pause() {}

void ReadAloudPlaybackController::SeekToWord(uint32_t segment_index,
                                             uint32_t character_offset) {
  // TODO(b/529882158): Validate that segment_index is within bounds of stored
  // segments, and character_offset is within the segment length. Call
  // mojo::ReportBadMessage on invalid input.
}

void ReadAloudPlaybackController::SeekToTime(base::TimeDelta position) {}

void ReadAloudPlaybackController::SetVoice(const std::string& voice_id) {}

void ReadAloudPlaybackController::SetPlaybackRate(float rate) {}

void ReadAloudPlaybackController::FlushBuffers() {}

}  // namespace readaloud
