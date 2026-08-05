// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <iterator>
#include <utility>

#include "chrome/common/readaloud/read_aloud.mojom.h"

namespace readaloud {

CachedCompressedSegment::CachedCompressedSegment() = default;

CachedCompressedSegment::CachedCompressedSegment(
    scoped_refptr<media::DecoderBuffer> opus_buffer,
    std::vector<DecodedAudioSegment::WordTiming> timings)
    : opus_buffer(std::move(opus_buffer)), timings(std::move(timings)) {}

CachedCompressedSegment::CachedCompressedSegment(
    const CachedCompressedSegment&) = default;
CachedCompressedSegment& CachedCompressedSegment::operator=(
    const CachedCompressedSegment&) = default;
CachedCompressedSegment::CachedCompressedSegment(
    CachedCompressedSegment&&) noexcept = default;
CachedCompressedSegment& CachedCompressedSegment::operator=(
    CachedCompressedSegment&&) noexcept = default;

CachedCompressedSegment::~CachedCompressedSegment() = default;

PrefetchManager::PrefetchManager() = default;

PrefetchManager::~PrefetchManager() = default;

void PrefetchManager::SetTextContent(
    const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timeline_.clear();
  session_cache_.clear();

  for (const read_aloud::mojom::TextSegmentPtr& segment : segments) {
    if (!segment || segment->text.empty()) {
      continue;
    }
    // TODO(b/543025514): In ChunkingMode::kSpeed, handle isolated
    // all-punctuation sentences (e.g., ellipses "...") in TextChunker so they
    // are not sent as standalone network synthesis requests. In
    // ChunkingMode::kQuality, retain them within paragraph groupings for
    // natural prosody and pauses.
    std::vector<TextChunk> sentence_chunks =
        ChunkText(segment->text, ChunkingMode::kSpeed, locale_tag);
    timeline_.insert(timeline_.end(),
                     std::make_move_iterator(sentence_chunks.begin()),
                     std::make_move_iterator(sentence_chunks.end()));
  }
}

void PrefetchManager::ResetSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timeline_.clear();
  session_cache_.clear();
}

bool PrefetchManager::HasCachedSegment(int32_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunk_index < 0) {
    return false;
  }
  return session_cache_.contains(chunk_index);
}

const CachedCompressedSegment* PrefetchManager::GetCachedSegment(
    int32_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunk_index < 0) {
    return nullptr;
  }
  std::map<int32_t, CachedCompressedSegment>::const_iterator it =
      session_cache_.find(chunk_index);
  if (it == session_cache_.end()) {
    return nullptr;
  }
  return &it->second;
}

void PrefetchManager::InsertCachedSegment(
    int32_t chunk_index,
    scoped_refptr<media::DecoderBuffer> opus_buffer,
    std::vector<DecodedAudioSegment::WordTiming> timings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunk_index < 0 || !opus_buffer || opus_buffer->empty()) {
    return;
  }
  if (!timeline_.empty() &&
      chunk_index >= static_cast<int32_t>(timeline_.size())) {
    return;
  }
  session_cache_.insert_or_assign(
      chunk_index,
      CachedCompressedSegment(std::move(opus_buffer), std::move(timings)));
}

void PrefetchManager::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_cache_.clear();
}

size_t PrefetchManager::GetTimelineChunkCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timeline_.size();
}

const std::vector<TextChunk>& PrefetchManager::GetTimelineChunks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timeline_;
}

}  // namespace readaloud
