// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_TIMELINE_PLAYBACK_TIMELINE_H_
#define CHROME_SERVICES_READALOUD_TIMELINE_PLAYBACK_TIMELINE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/sequence_checker.h"
#include "chrome/common/readaloud/read_aloud.mojom-forward.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/timeline/timeline_position.h"

namespace readaloud {

// Manages canonical document timeline chunking and provides access to sentence chunks.
class PlaybackTimeline {
 public:
  // Baseline estimated speaking duration per 16-bit character code unit at 1.0x
  // playback speed before actual TTS synthesis timings arrive.
  static constexpr base::TimeDelta kEstimatedDurationPerChar =
      base::Milliseconds(65);

  PlaybackTimeline();
  PlaybackTimeline(const PlaybackTimeline&) = delete;
  PlaybackTimeline& operator=(const PlaybackTimeline&) = delete;
  ~PlaybackTimeline();

  // Initializes the timeline by concatenating input segments into a unified
  // document buffer and splitting into canonical atomic sentence chunks.
  void SetTextContent(
      const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
      std::optional<base::i18n::LanguageTag> locale_tag = std::nullopt);

  // Resets all timeline data.
  void Clear();

  size_t GetChunkCount() const;
  const std::vector<TextChunk>& chunks() const { return chunks_; }

  // Resolves a 0-based chunk index (from highlighter.js) and character offset
  // within that chunk into a complete TimelinePosition in O(1) time.
  // Returns std::nullopt if `segment_index` is out of bounds or
  // `character_offset` exceeds the chunk length.
  // TODO(b/565884306): Unify 'sentence'/'segment' naming to 'chunk' in a
  // follow-up cleanup CL.
  std::optional<TimelinePosition> ResolveSegmentOffset(
      uint32_t segment_index,
      uint32_t character_offset) const;

  // Updates the actual 1.0x duration for a specific chunk index in O(1) time
  // by recording its deviation from the initial character-based duration
  // estimate.
  // TODO(b/565884306): Unify 'sentence'/'segment' naming to 'chunk' in a
  // follow-up cleanup CL.
  void UpdateSentenceDuration(uint32_t sentence_index,
                              base::TimeDelta actual_duration);

 private:
  base::TimeDelta GetChunkDuration(size_t chunk_index) const;

  bool is_initialized_ = false;
  std::u16string document_text_;
  std::vector<TextChunk> chunks_;
  std::vector<base::TimeDelta> static_est_start_times_1_0x_;
  std::vector<base::TimeDelta> deviations_1_0x_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_TIMELINE_PLAYBACK_TIMELINE_H_
