// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/timeline/playback_timeline.h"

namespace readaloud {

namespace {
// Maximum number of sentence chunks to prefetch ahead in a single evaluation
// to avoid queueing excessive network requests on long documents.
constexpr size_t kMaxPrefetchLookahead = 5;
}  // namespace

CachedCompressedSegment::CachedCompressedSegment() = default;

CachedCompressedSegment::CachedCompressedSegment(
    scoped_refptr<media::DecoderBuffer> opus_buffer,
    std::vector<WordTiming> timings,
    SynthesisResultStatus status)
    : status(status),
      opus_buffer(std::move(opus_buffer)),
      timings(std::move(timings)) {}

CachedCompressedSegment::CachedCompressedSegment(
    const CachedCompressedSegment&) = default;
CachedCompressedSegment& CachedCompressedSegment::operator=(
    const CachedCompressedSegment&) = default;
CachedCompressedSegment::CachedCompressedSegment(
    CachedCompressedSegment&&) noexcept = default;
CachedCompressedSegment& CachedCompressedSegment::operator=(
    CachedCompressedSegment&&) noexcept = default;

CachedCompressedSegment::~CachedCompressedSegment() = default;

PrefetchManager::PrefetchManager(const PlaybackTimeline* timeline)
    : timeline_(timeline) {
  DCHECK(timeline_);
}

PrefetchManager::~PrefetchManager() = default;

void PrefetchManager::ResetSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_cache_.clear();
  mode_scheduler_.Reset();
  CancelInflightRequests();
  weak_factory_.InvalidateWeakPtrs();
}

void PrefetchManager::CancelInflightRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++session_sequence_id_;
  inflight_requests_.clear();
  pending_requests_.clear();
}

void PrefetchManager::SetRequestSynthesisCallback(
    RequestSynthesisCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_synthesis_callback_ = std::move(callback);
  MaybeIssueSynthesisRequest();
}

void PrefetchManager::SchedulePrefetch(uint32_t chunk_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunk_index >= GetTimelineChunkCount()) {
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
    std::vector<WordTiming> timings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sequence_id != session_sequence_id_) {
    return;
  }
  if (!inflight_requests_.contains(chunk_index)) {
    return;
  }
  inflight_requests_.erase(chunk_index);

  if (!opus_buffer || opus_buffer->empty()) {
    LOG(WARNING) << "ReadAloud: Synthesis error or empty audio buffer received "
                    "for chunk "
                 << chunk_index;
    InsertCachedSegment(chunk_index, nullptr, {},
                        SynthesisResultStatus::kSynthesisError);
  } else {
    InsertCachedSegment(chunk_index, std::move(opus_buffer), std::move(timings),
                        SynthesisResultStatus::kSuccess);
  }
  MaybeIssueSynthesisRequest();
}

PrefetchMode PrefetchManager::UpdatePrefetchMode(
    base::TimeDelta current_buffered_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.UpdateMode(current_buffered_duration);
}

PrefetchMode PrefetchManager::GetPrefetchMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.GetPrefetchMode();
}

base::TimeDelta PrefetchManager::GetTargetPrefetchDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mode_scheduler_.GetTargetPrefetchDuration();
}

bool PrefetchManager::HasCachedSegment(uint32_t chunk_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<uint32_t, CachedCompressedSegment>::const_iterator it =
      session_cache_.find(chunk_index);
  return it != session_cache_.end() &&
         it->second.status == SynthesisResultStatus::kSuccess &&
         it->second.opus_buffer && !it->second.opus_buffer->empty();
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
    std::vector<WordTiming> timings,
    SynthesisResultStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetTimelineChunkCount() > 0 && chunk_index >= GetTimelineChunkCount()) {
    return;
  }
  session_cache_.insert_or_assign(
      chunk_index, CachedCompressedSegment(std::move(opus_buffer),
                                           std::move(timings), status));
}

void PrefetchManager::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_cache_.clear();
  mode_scheduler_.Reset();
}

size_t PrefetchManager::GetTimelineChunkCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timeline_->GetChunkCount();
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
  const std::vector<TextChunk>& chunks = timeline_->chunks();
  while (!pending_requests_.empty() &&
         inflight_requests_.size() < kMaxConcurrentRequests) {
    uint32_t next_index = pending_requests_.front();
    pending_requests_.pop_front();

    if (next_index >= chunks.size() || session_cache_.contains(next_index) ||
        inflight_requests_.contains(next_index)) {
      // Skip chunk indices that are out of bounds, already cached, or
      // currently in flight.
      continue;
    }
    inflight_requests_.insert(next_index);
    // TODO(b/527525636): Use `GetPrefetchMode()` (`PrefetchMode::kQuality` vs
    // `PrefetchMode::kSpeed`) to group adjacent canonical chunks with the same
    // speaker into a single multi-chunk synthesis request for improved prosody
    // once the audio buffer reaches the quality watermark.
    request_synthesis_callback_.Run(next_index, chunks[next_index].text,
                                    chunks[next_index].speaker);
  }
}

std::vector<uint32_t> PrefetchManager::GetRequiredPrefetchChunks(
    size_t current_chunk_index,
    base::TimeDelta current_buffered_duration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<uint32_t> required_chunks;
  const size_t total_chunks = GetTimelineChunkCount();

  if (current_chunk_index >= total_chunks ||
      current_buffered_duration.is_negative() ||
      current_buffered_duration >= kAudioBufferPrefetchWatermark) {
    return required_chunks;
  }

  size_t start_idx = current_chunk_index;
  size_t end_idx = std::min(total_chunks, start_idx + kMaxPrefetchLookahead);

  for (size_t i = start_idx; i < end_idx; ++i) {
    uint32_t chunk_idx = static_cast<uint32_t>(i);
    if (!session_cache_.contains(chunk_idx)) {
      required_chunks.push_back(chunk_idx);
    }
  }

  return required_chunks;
}

}  // namespace readaloud
