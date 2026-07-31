// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/mock_read_aloud_playback_controller.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/strings/string_util.h"

namespace readaloud {

MockReadAloudPlaybackController::MockReadAloudPlaybackController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
        receiver)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &MockReadAloudPlaybackController::Pause, weak_factory_.GetWeakPtr()));

  ON_CALL(*this, SetTextContent)
      .WillByDefault(::testing::Invoke(
          this, &MockReadAloudPlaybackController::DefaultSetTextContent));
  ON_CALL(*this, Play).WillByDefault(::testing::Invoke(
      this, &MockReadAloudPlaybackController::DefaultPlay));
  ON_CALL(*this, Pause).WillByDefault(::testing::Invoke(
      this, &MockReadAloudPlaybackController::DefaultPause));
  ON_CALL(*this, SeekToWord)
      .WillByDefault(::testing::Invoke(
          this, &MockReadAloudPlaybackController::DefaultSeekToWord));
  ON_CALL(*this, SeekToTime)
      .WillByDefault(::testing::Invoke(
          this, &MockReadAloudPlaybackController::DefaultSeekToTime));
  ON_CALL(*this, SetPlaybackRate)
      .WillByDefault(::testing::Invoke(
          this, &MockReadAloudPlaybackController::DefaultSetPlaybackRate));
}

MockReadAloudPlaybackController::~MockReadAloudPlaybackController() = default;

void MockReadAloudPlaybackController::InitializeClient(
    mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
        client) {
  client_.reset();
  client_.Bind(std::move(client));
  client_.set_disconnect_handler(base::BindOnce(
      &MockReadAloudPlaybackController::Pause, weak_factory_.GetWeakPtr()));

  if (client_.is_connected()) {
    client_->OnPlaybackStateChanged(state_);
    client_->OnPlaybackDurationChanged(CalculateTotalDuration());
  }
}

base::TimeDelta MockReadAloudPlaybackController::CalculateTotalDuration()
    const {
  if (word_boundaries_.empty()) {
    return base::TimeDelta();
  }
  return word_boundaries_.back().audio_timestamp + kDefaultWordDuration;
}

void MockReadAloudPlaybackController::DefaultSetTextContent(
    std::vector<read_aloud::mojom::TextSegmentPtr> segments) {
  Pause();
  segments_ = std::move(segments);
  word_boundaries_.clear();
  current_boundary_index_ = 0;

  base::TimeDelta current_time = base::TimeDelta();
  for (const read_aloud::mojom::TextSegmentPtr& segment : segments_) {
    if (!segment) {
      continue;
    }
    const std::u16string& text = segment->text;
    uint32_t seg_idx = segment->segment_index;

    bool in_word = false;
    for (size_t i = 0; i < text.length(); ++i) {
      char16_t c = text[i];
      if (base::IsUnicodeWhitespace(c)) {
        in_word = false;
      } else {
        if (!in_word) {
          word_boundaries_.push_back(
              {seg_idx, static_cast<uint32_t>(i), current_time});
          current_time += kDefaultWordDuration;
          in_word = true;
        }
      }
    }
  }

  if (client_.is_connected()) {
    client_->OnPlaybackDurationChanged(CalculateTotalDuration());
  }
}

void MockReadAloudPlaybackController::DefaultPlay() {
  if (state_ == read_aloud::mojom::PlaybackState::kPlaying) {
    return;
  }

  if (current_boundary_index_ >= word_boundaries_.size()) {
    current_boundary_index_ = 0;
  }
  UpdatePlaybackState(read_aloud::mojom::PlaybackState::kPlaying);
  TriggerWordBoundary();
  current_boundary_index_++;

  StartTimer();
}

void MockReadAloudPlaybackController::DefaultPause() {
  if (state_ == read_aloud::mojom::PlaybackState::kPaused) {
    return;
  }
  UpdatePlaybackState(read_aloud::mojom::PlaybackState::kPaused);
  timer_.Stop();
}

void MockReadAloudPlaybackController::DefaultSeekToWord(
    uint32_t segment_index,
    uint32_t character_offset) {
  if (segments_.empty()) {
    return;
  }

  size_t closest_idx = 0;
  uint32_t min_diff = std::numeric_limits<uint32_t>::max();
  bool found = false;
  for (size_t i = 0; i < word_boundaries_.size(); ++i) {
    if (word_boundaries_[i].segment_index == segment_index) {
      found = true;
      uint32_t offset = word_boundaries_[i].character_offset;
      uint32_t diff = offset > character_offset ? offset - character_offset
                                                : character_offset - offset;
      if (diff < min_diff) {
        min_diff = diff;
        closest_idx = i;
      }
    }
  }
  if (found) {
    current_boundary_index_ = closest_idx;
    TriggerWordBoundary();
    current_boundary_index_++;
    if (state_ == read_aloud::mojom::PlaybackState::kPlaying) {
      StartTimer();
    }
  }
}

// Gracefully handles arbitrary or out-of-bounds time positions by snapping
// to the nearest word boundary.
void MockReadAloudPlaybackController::DefaultSeekToTime(
    base::TimeDelta position) {
  if (word_boundaries_.empty()) {
    return;
  }

  size_t closest_idx = 0;
  base::TimeDelta min_diff = base::TimeDelta::Max();
  for (size_t i = 0; i < word_boundaries_.size(); ++i) {
    base::TimeDelta diff =
        (word_boundaries_[i].audio_timestamp - position).magnitude();
    if (diff < min_diff) {
      min_diff = diff;
      closest_idx = i;
    }
  }
  current_boundary_index_ = closest_idx;
  TriggerWordBoundary();
  current_boundary_index_++;
  if (state_ == read_aloud::mojom::PlaybackState::kPlaying) {
    StartTimer();
  }
}

// Gracefully handles invalid/NaN rates by ignoring them, and clamps
// out-of-bounds rates to the [0.25, 4.0] range.
void MockReadAloudPlaybackController::DefaultSetPlaybackRate(float rate) {
  if (!(rate > 0.0f)) {
    return;
  }
  playback_rate_ = std::clamp(rate, 0.25f, 4.0f);
  if (state_ == read_aloud::mojom::PlaybackState::kPlaying) {
    StartTimer();
  }
}

void MockReadAloudPlaybackController::StartTimer() {
  timer_.Stop();
  if (word_boundaries_.empty()) {
    return;
  }

  base::TimeDelta interval = kDefaultWordDuration / playback_rate_;
  timer_.Start(FROM_HERE, interval, this,
               &MockReadAloudPlaybackController::OnTimerFired);
}

void MockReadAloudPlaybackController::OnTimerFired() {
  if (current_boundary_index_ >= word_boundaries_.size()) {
    Pause();
    return;
  }
  TriggerWordBoundary();
  current_boundary_index_++;
}

void MockReadAloudPlaybackController::TriggerWordBoundary() {
  if (!client_.is_connected() ||
      current_boundary_index_ >= word_boundaries_.size()) {
    return;
  }
  const WordBoundary& boundary = word_boundaries_[current_boundary_index_];
  client_->OnWordBoundaryReached(boundary.segment_index,
                                 boundary.character_offset,
                                 boundary.audio_timestamp);
}

void MockReadAloudPlaybackController::UpdatePlaybackState(
    read_aloud::mojom::PlaybackState state) {
  state_ = state;
  if (client_.is_connected()) {
    client_->OnPlaybackStateChanged(state_);
  }
}

}  // namespace readaloud
