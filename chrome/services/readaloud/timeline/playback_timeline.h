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

 private:
  std::u16string document_text_;
  std::vector<TextChunk> chunks_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_TIMELINE_PLAYBACK_TIMELINE_H_
