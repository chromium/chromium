// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/common/readaloud/read_aloud_constants.h"

namespace readaloud {

namespace {
// Maximum number of sentence chunks to prefetch ahead in a single evaluation
// to avoid queueing excessive network requests on long documents.
constexpr size_t kMaxPrefetchLookahead = 5;
}  // namespace

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
  ResetSession();

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
        ChunkText(segment->text, GetChunkingMode(), locale_tag);
    timeline_.insert(timeline_.end(),
                     std::make_move_iterator(sentence_chunks.begin()),
                     std::make_move_iterator(sentence_chunks.end()));
  }

  if (on_text_chunked_callback_) {
    std::vector<std::u16string> string_chunks;
    size_t num_chunks = std::min(timeline_.size(), readaloud::kMaxTextChunks);
    string_chunks.reserve(num_chunks);
    for (size_t i = 0; i < num_chunks; ++i) {
      const TextChunk& chunk = timeline_[i];
      string_chunks.emplace_back(chunk.text);
    }
    on_text_chunked_callback_.Run(string_chunks);
  }
}

void PrefetchManager::ResetSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timeline_.clear();
  session_cache_.clear();
  mode_scheduler_.Reset();
  ++session_sequence_id_;
  inflight_requests_.clear();
  pending_requests_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void PrefetchManager::SetRequestSynthesisCallback(
    RequestSynthesisCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_synthesis_callback_ = std::move(callback);
  MaybeIssueSynthesisRequest();
}

void PrefetchManager::SetOnTextChunkedCallback(OnTextChunkedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_text_chunked_callback_ = std::move(callback);
}

void PrefetchManager::SchedulePrefetch(uint32_t chunk_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunk_index >= timeline_.size()) {
    return;
  }
  if (session_cache_.contains(chunk_index) ||
      inflight_requests_.contains(chunk_index) ||
      std::ranges::find(pending_requests_, chunk_index) !=
          pending_requests_.end()) {
    return;
  }
  pending_requests_.push_back(chunk_index);
  MaybeIssueSynthesisRequest();
}

void PrefetchManager::OnSynthesisResponse(
    uint64_t sequence_id,
    uint32_t chunk_index,
    scoped_refptr<media::DecoderBuffer> opus_buffer,
    std::vector<DecodedAudioSegment::WordTiming> timings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sequence_id != session_sequence_id_) {
    return;
  }
  if (!inflight_requests_.contains(chunk_index)) {
    return;
  }
  inflight_requests_.erase(chunk_index);
  InsertCachedSegment(chunk_index, std::move(opus_buffer), std::move(timings));
  MaybeIssueSynthesisRequest();
}

ChunkingMode PrefetchManager::UpdatePrefetchMode(
    base::TimeDelta current_buffered_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.UpdateMode(current_buffered_duration);
}

ChunkingMode PrefetchManager::GetChunkingMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.GetChunkingMode();
}

base::TimeDelta PrefetchManager::GetTargetPrefetchDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.GetTargetPrefetchDuration();
}

bool PrefetchManager::HasCachedSegment(uint32_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<uint32_t, CachedCompressedSegment>::const_iterator it =
      session_cache_.find(chunk_index);
  return it != session_cache_.end() && it->second.opus_buffer &&
         !it->second.opus_buffer->empty();
}

const CachedCompressedSegment* PrefetchManager::GetCachedSegment(
    uint32_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<uint32_t, CachedCompressedSegment>::const_iterator it =
      session_cache_.find(chunk_index);
  if (it == session_cache_.end()) {
    return nullptr;
  }
  return &it->second;
}

void PrefetchManager::InsertCachedSegment(
    uint32_t chunk_index,
    scoped_refptr<media::DecoderBuffer> opus_buffer,
    std::vector<DecodedAudioSegment::WordTiming> timings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!timeline_.empty() && chunk_index >= GetTimelineChunkCount()) {
    return;
  }
  session_cache_.insert_or_assign(
      chunk_index,
      CachedCompressedSegment(std::move(opus_buffer), std::move(timings)));
}

void PrefetchManager::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_cache_.clear();
  mode_scheduler_.Reset();
}

size_t PrefetchManager::GetTimelineChunkCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timeline_.size();
}

const std::vector<TextChunk>& PrefetchManager::GetTimelineChunks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timeline_;
}

uint64_t PrefetchManager::GetCurrentSequenceId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_sequence_id_;
}

size_t PrefetchManager::GetInflightRequestCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return inflight_requests_.size();
}

void PrefetchManager::MaybeIssueSynthesisRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_synthesis_callback_) {
    return;
  }
  while (!pending_requests_.empty() &&
         inflight_requests_.size() < kMaxConcurrentRequests) {
    uint32_t next_index = pending_requests_.front();
    pending_requests_.pop_front();

    if (session_cache_.contains(next_index) ||
        inflight_requests_.contains(next_index)) {
      // Skip chunk indices that are already cached or currently in flight.
      continue;
    }
    inflight_requests_.insert(next_index);
    // Dispatch the synthesis request to the controller via the registered
    // callback.
    request_synthesis_callback_.Run(next_index, timeline_[next_index].text);
  }
}

std::vector<uint32_t> PrefetchManager::GetRequiredPrefetchChunks(
    size_t current_chunk_index,
    base::TimeDelta current_buffered_duration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<uint32_t> required_chunks;

  if (current_chunk_index >= timeline_.size() ||
      current_buffered_duration.is_negative() ||
      current_buffered_duration >= kAudioBufferPrefetchWatermark) {
    return required_chunks;
  }

  size_t start_idx = current_chunk_index;
  size_t end_idx =
      std::min(timeline_.size(), start_idx + kMaxPrefetchLookahead);

  for (size_t i = start_idx; i < end_idx; ++i) {
    uint32_t chunk_idx = static_cast<uint32_t>(i);
    if (!session_cache_.contains(chunk_idx)) {
      required_chunks.push_back(chunk_idx);
    }
  }

  return required_chunks;
}

}  // namespace readaloud
