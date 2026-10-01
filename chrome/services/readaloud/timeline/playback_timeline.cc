// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/timeline/playback_timeline.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"

namespace readaloud {

namespace {

// Snaps `target_offset` within `text` to the start of the word covering it
// using ICU word break iteration so seeking never splits a word in half.
size_t SnapToWordStart(std::u16string_view text, size_t target_offset) {
  if (target_offset == 0 || text.empty()) {
    return 0;
  }
  if (target_offset >= text.size()) {
    return text.size();
  }
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return target_offset;
  }
  size_t last_word_start = 0;
  while (iter.Advance()) {
    if (!iter.IsWord()) {
      continue;
    }
    size_t word_start = iter.prev();
    size_t word_end = iter.pos();
    if (target_offset >= word_start && target_offset < word_end) {
      return word_start;
    }
    if (word_start <= target_offset) {
      last_word_start = word_start;
    } else {
      break;
    }
  }
  return last_word_start;
}

}  // namespace

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

  // Pre-reserve the full character count so `document_text_.append()` in the
  // loop below never reallocates and invalidates `std::u16string_view`s stored
  // in earlier `chunks_` entries.
  size_t total_text_size = 0;
  for (const read_aloud::mojom::TextSegmentPtr& segment : segments) {
    if (segment && !segment->text.empty()) {
      total_text_size += segment->text.size();
    }
  }
  document_text_.reserve(total_text_size);

  size_t document_offset = 0;
  for (const read_aloud::mojom::TextSegmentPtr& segment : segments) {
    if (!segment || segment->text.empty()) {
      continue;
    }
    const size_t segment_start = document_text_.size();
    DCHECK_LE(document_text_.size() + segment->text.size(),
              document_text_.capacity());
    document_text_.append(segment->text);
    std::u16string_view segment_view =
        std::u16string_view(document_text_)
            .substr(segment_start, segment->text.size());

    // Always use ChunkingMode::kSpeed so PlaybackTimeline preserves the
    // canonical atomic sentence boundaries (0...N-1) rather than prosody
    // multi-sentence groups.
    // TODO(b/543025514): In ChunkingMode::kSpeed, handle isolated
    // all-punctuation sentences (e.g., ellipses "...") in TextChunker so they
    // are not sent as standalone network synthesis requests. In
    // ChunkingMode::kQuality, retain them within paragraph groupings for
    // natural prosody and pauses.
    std::vector<TextChunk> sentence_chunks =
        ChunkText(segment_view, ChunkingMode::kSpeed, locale_tag);
    // ChunkText() counts from 0 within the segment. Shift the chunks so the
    // document's canonical offsets continue across segments, with one
    // separator after the previous segment's last chunk, as between any two
    // chunks. highlighter.js depends on this; see
    // `TextChunk::start_code_unit_offset`.
    for (TextChunk& chunk : sentence_chunks) {
      chunk.start_code_unit_offset += document_offset;
      chunk.speaker = segment->speaker;
    }
    if (!sentence_chunks.empty()) {
      const TextChunk& last = sentence_chunks.back();
      document_offset = last.start_code_unit_offset + last.text.size() + 1;
    }
    chunks_.insert(chunks_.end(),
                   std::make_move_iterator(sentence_chunks.begin()),
                   std::make_move_iterator(sentence_chunks.end()));
  }

  base::TimeDelta cumulative_est_time;
  static_est_start_times_1_0x_.reserve(chunks_.size());
  for (const TextChunk& chunk : chunks_) {
    static_est_start_times_1_0x_.push_back(cumulative_est_time);
    cumulative_est_time += chunk.text.size() * kEstimatedDurationPerChar;
  }
  deviations_1_0x_.assign(chunks_.size(), base::TimeDelta());
  sentence_word_timings_.assign(chunks_.size(), {});
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
  sentence_word_timings_.clear();
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

void PlaybackTimeline::UpdateSentenceDuration(
    uint32_t sentence_index,
    base::TimeDelta actual_duration,
    std::vector<WordTiming> word_timings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(sentence_index, chunks_.size());
  DCHECK_EQ(chunks_.size(), deviations_1_0x_.size());
  DCHECK_EQ(chunks_.size(), sentence_word_timings_.size());
  base::TimeDelta est_duration =
      chunks_[sentence_index].text.size() * kEstimatedDurationPerChar;
  deviations_1_0x_[sentence_index] = actual_duration - est_duration;
  sentence_word_timings_[sentence_index] = std::move(word_timings);
}

uint32_t PlaybackTimeline::ResolveCharOffsetInChunk(
    size_t chunk_index,
    base::TimeDelta offset_in_chunk,
    base::TimeDelta chunk_duration) const {
  DCHECK_LT(chunk_index, chunks_.size());
  const TextChunk& chunk = chunks_[chunk_index];
  if (offset_in_chunk <= base::TimeDelta() ||
      chunk_duration <= base::TimeDelta()) {
    return 0;
  }

  // 1. If synthesized WordTimings are available for this sentence, resolve
  // using the exact word timing boundaries and snap to the start of the word.
  const std::vector<WordTiming>& timings = sentence_word_timings_[chunk_index];
  if (!timings.empty()) {
    uint32_t last_word_offset = 0;
    for (const WordTiming& word : timings) {
      uint32_t rel_start =
          word.start_character_offset >= chunk.start_code_unit_offset
              ? static_cast<uint32_t>(word.start_character_offset -
                                      chunk.start_code_unit_offset)
              : word.start_character_offset;
      rel_start = std::min(rel_start, static_cast<uint32_t>(chunk.text.size()));
      if (offset_in_chunk >= word.start_time &&
          offset_in_chunk < word.end_time) {
        return rel_start;
      }
      if (word.start_time <= offset_in_chunk) {
        last_word_offset = rel_start;
      } else {
        break;
      }
    }
    return last_word_offset;
  }

  // 2. Fallback for unsynthesized chunks: estimate character index linearly and
  // snap backward to the start of the covering word via BreakIterator.
  double ratio = offset_in_chunk.InSecondsF() / chunk_duration.InSecondsF();
  size_t raw_char_offset = static_cast<size_t>(ratio * chunk.text.size());
  raw_char_offset = std::min(raw_char_offset, chunk.text.size());
  return static_cast<uint32_t>(SnapToWordStart(chunk.text, raw_char_offset));
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

std::optional<TimelinePosition> PlaybackTimeline::ResolveTimeOffset(
    base::TimeDelta target_time_1_0x) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(chunks_.size(), static_est_start_times_1_0x_.size());
  DCHECK_EQ(chunks_.size(), deviations_1_0x_.size());
  if (chunks_.empty() || target_time_1_0x.is_negative() ||
      target_time_1_0x.is_max()) {
    return std::nullopt;
  }

  base::TimeDelta cumulative_deviation;
  for (size_t i = 0; i < chunks_.size(); ++i) {
    base::TimeDelta chunk_start =
        static_est_start_times_1_0x_[i] + cumulative_deviation;
    base::TimeDelta chunk_duration = GetChunkDuration(i);
    base::TimeDelta chunk_end = chunk_start + chunk_duration;
    cumulative_deviation += deviations_1_0x_[i];

    if (target_time_1_0x < chunk_end) {
      base::TimeDelta offset_in_chunk = target_time_1_0x - chunk_start;
      uint32_t char_offset_in_chunk =
          ResolveCharOffsetInChunk(i, offset_in_chunk, chunk_duration);
      return TimelinePosition(static_cast<uint32_t>(i), chunks_[i],
                              char_offset_in_chunk, target_time_1_0x,
                              chunk_end);
    }
  }

  // Target time is at or beyond total timeline duration; clamp to end of final
  // sentence chunk.
  size_t last_idx = chunks_.size() - 1;
  base::TimeDelta last_chunk_start =
      static_est_start_times_1_0x_[last_idx] + cumulative_deviation -
      deviations_1_0x_[last_idx];
  base::TimeDelta total_duration =
      last_chunk_start + GetChunkDuration(last_idx);
  return TimelinePosition(
      static_cast<uint32_t>(last_idx), chunks_[last_idx],
      static_cast<uint32_t>(chunks_[last_idx].text.size()),
      std::min(target_time_1_0x, total_duration), total_duration);
}

}  // namespace readaloud
