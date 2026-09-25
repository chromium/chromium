// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/timeline/playback_timeline.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"

namespace readaloud {

PlaybackTimeline::PlaybackTimeline() = default;

PlaybackTimeline::~PlaybackTimeline() = default;

void PlaybackTimeline::SetTextContent(
    const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
}

void PlaybackTimeline::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chunks_.clear();
  document_text_.clear();
}

size_t PlaybackTimeline::GetChunkCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chunks_.size();
}

}  // namespace readaloud
