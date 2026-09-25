// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/timeline/playback_timeline.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"

namespace readaloud {

PlaybackTimeline::PlaybackTimeline() = default;

PlaybackTimeline::~PlaybackTimeline() = default;

void PlaybackTimeline::SetTextContent(
    const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_initialized_)
      << "Mid-session re-chunking is prohibited; call Clear() before "
         "SetTextContent().";
  Clear();

  // TODO(b/565884049): Record a UMA metric if segments.size() > 1 occurs in
  // production.
  if (segments.size() > 1) {
    LOG(ERROR) << "Expected at most 1 text segment from Distiller, received "
               << segments.size()
               << ". Concatenating into a single document.";
  }

  for (const read_aloud::mojom::TextSegmentPtr& segment : segments) {
    if (segment && !segment->text.empty()) {
      document_text_.append(segment->text);
    }
  }

  if (!document_text_.empty()) {
    // Always use ChunkingMode::kSpeed so PlaybackTimeline preserves the
    // canonical atomic sentence boundaries (0...N-1) rather than prosody
    // multi-sentence groups.
    chunks_ = ChunkText(document_text_, ChunkingMode::kSpeed, locale_tag,
                        /*base_offset=*/0);
  }

  base::TimeDelta cumulative_est_time;
  static_est_start_times_1_0x_.reserve(chunks_.size());
  for (const TextChunk& chunk : chunks_) {
    static_est_start_times_1_0x_.push_back(cumulative_est_time);
    cumulative_est_time += chunk.text.size() * kEstimatedDurationPerChar;
  }
  deviations_1_0x_.assign(chunks_.size(), base::TimeDelta());
  if (!chunks_.empty()) {
    is_initialized_ = true;
  }
}

void PlaybackTimeline::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chunks_.clear();
  document_text_.clear();
  static_est_start_times_1_0x_.clear();
  deviations_1_0x_.clear();
  is_initialized_ = false;
}

size_t PlaybackTimeline::GetChunkCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chunks_.size();
}

base::TimeDelta PlaybackTimeline::GetChunkDuration(size_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(chunk_index, chunks_.size());
  return chunks_[chunk_index].text.size() * kEstimatedDurationPerChar +
         deviations_1_0x_[chunk_index];
}

void PlaybackTimeline::UpdateSentenceDuration(uint32_t sentence_index,
                                              base::TimeDelta actual_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(sentence_index, chunks_.size());
  DCHECK_EQ(chunks_.size(), deviations_1_0x_.size());
  base::TimeDelta est_duration =
      chunks_[sentence_index].text.size() * kEstimatedDurationPerChar;
  deviations_1_0x_[sentence_index] = actual_duration - est_duration;
}

std::optional<TimelinePosition> PlaybackTimeline::ResolveSegmentOffset(
    uint32_t segment_index,
    uint32_t character_offset) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(chunks_.size(), static_est_start_times_1_0x_.size());
  DCHECK_EQ(chunks_.size(), deviations_1_0x_.size());
  if (segment_index >= chunks_.size()) {
    return std::nullopt;
  }

  const TextChunk& chunk = chunks_[segment_index];
  if (character_offset > chunk.text.size()) {
    return std::nullopt;
  }

  base::TimeDelta accumulated_time =
      static_est_start_times_1_0x_[segment_index];
  for (size_t i = 0; i < segment_index; ++i) {
    accumulated_time += deviations_1_0x_[i];
  }
  base::TimeDelta chunk_end_time =
      accumulated_time + GetChunkDuration(segment_index);

  return TimelinePosition(segment_index, chunk, character_offset,
                          accumulated_time, chunk_end_time);
}

}  // namespace readaloud
