// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_
#define CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom-forward.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/prefetch/prefetch_mode_scheduler.h"
#include "media/base/decoder_buffer.h"

namespace readaloud {

// Holds compressed Ogg/Opus speech synthesis bytes and frame-accurate word
// timing metadata for a single sentence chunk in the document session.
struct CachedCompressedSegment {
  CachedCompressedSegment();
  CachedCompressedSegment(scoped_refptr<media::DecoderBuffer> opus_buffer,
                          std::vector<DecodedAudioSegment::WordTiming> timings);
  CachedCompressedSegment(const CachedCompressedSegment&);
  CachedCompressedSegment& operator=(const CachedCompressedSegment&);
  CachedCompressedSegment(CachedCompressedSegment&&) noexcept;
  CachedCompressedSegment& operator=(CachedCompressedSegment&&) noexcept;
  ~CachedCompressedSegment();

  scoped_refptr<media::DecoderBuffer> opus_buffer;
  std::vector<DecodedAudioSegment::WordTiming> timings;
};

// Manages document-bound caching of compressed speech synthesis audio,
// coordinates prefetch sentence chunking with the active hysteresis mode, and
// throttles in-flight synthesis requests to prevent network saturation.
class PrefetchManager {
 public:
  static constexpr size_t kMaxConcurrentRequests = 3;

  // Invoked when an in-flight prefetch request is dispatched.
  using RequestSynthesisCallback =
      base::RepeatingCallback<void(uint32_t chunk_index,
                                   std::u16string_view text)>;

  using OnTextChunkedCallback =
      base::RepeatingCallback<void(const std::vector<std::u16string>& chunks)>;

  PrefetchManager();
  PrefetchManager(const PrefetchManager&) = delete;
  PrefetchManager& operator=(const PrefetchManager&) = delete;
  ~PrefetchManager();

  // Sets the callback invoked when a synthesis request is dispatched.
  void SetRequestSynthesisCallback(RequestSynthesisCallback callback);

  // Sets the callback invoked when text content is chunked.
  void SetOnTextChunkedCallback(OnTextChunkedCallback callback);

  // Document-bound lifecycle:
  // Sets new document text segments, uses ChunkText(..., GetChunkingMode()) to
  // establish the sentence-level timeline (0...N-1), purges session_cache_,
  // increments session_sequence_id_, and clears in-flight/pending requests.
  void SetTextContent(
      const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
      std::optional<base::i18n::LanguageTag> locale_tag = std::nullopt);

  // Clears all cached segments, resets the timeline, resets mode scheduler,
  // increments session_sequence_id_, and clears in-flight/pending requests.
  void ResetSession();

  // Evaluates the current buffered audio duration against hysteresis thresholds
  // and updates the active prefetch mode (`kSpeed` or `kQuality`).
  // Delegated directly to `PrefetchModeScheduler`.
  // Note: Mode upgrades to `kQuality` group adjacent sentences for upcoming
  // uncached lookahead requests without re-chunking or invalidating existing
  // `session_cache_` entries.
  ChunkingMode UpdatePrefetchMode(base::TimeDelta current_buffered_duration);

  // Returns the current prefetch chunking mode (`kSpeed` or `kQuality`).
  ChunkingMode GetChunkingMode() const;

  // Returns the target prefetch audio duration threshold for the active mode
  // (15s for `kSpeed` mode, 50s for `kQuality` mode).
  base::TimeDelta GetTargetPrefetchDuration() const;

  // Schedules prefetch synthesis for the given sentence chunk index.
  // Throttles in-flight requests to kMaxConcurrentRequests.
  void SchedulePrefetch(uint32_t chunk_index);

  // Receives an asynchronous synthesis response. Discards stale or out-of-order
  // responses if sequence_id does not match the current session sequence ID.
  void OnSynthesisResponse(
      uint64_t sequence_id,
      uint32_t chunk_index,
      scoped_refptr<media::DecoderBuffer> opus_buffer,
      std::vector<DecodedAudioSegment::WordTiming> timings);

  // Cache accessors & modifiers:
  bool HasCachedSegment(uint32_t chunk_index) const;
  const CachedCompressedSegment* GetCachedSegment(uint32_t chunk_index) const;
  void InsertCachedSegment(
      uint32_t chunk_index,
      scoped_refptr<media::DecoderBuffer> opus_buffer,
      std::vector<DecodedAudioSegment::WordTiming> timings);
  void ClearCache();

  // Timeline & scheduler inspection:
  size_t GetTimelineChunkCount() const;
  const std::vector<TextChunk>& GetTimelineChunks() const;
  uint64_t GetCurrentSequenceId() const;
  size_t GetInflightRequestCount() const;

  base::WeakPtr<PrefetchManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Evaluates the sliding prefetch window starting from |current_chunk_index|.
  // Returns a vector of uncached chunk indices ahead of |current_chunk_index|
  // that should be synthesized to satisfy the target prefetch watermark.
  //
  // - If |current_buffered_duration| >= kAudioBufferPrefetchWatermark (15s),
  //   returns an empty vector.
  // - Skips indices that are already present in session_cache_.
  // - Bounded by kMaxPrefetchLookahead and timeline_.size().
  std::vector<uint32_t> GetRequiredPrefetchChunks(
      size_t current_chunk_index,
      base::TimeDelta current_buffered_duration) const;

 private:
  // Manages hysteresis transitions between kSpeed and kQuality modes.
  PrefetchModeScheduler mode_scheduler_;

  // Issues pending prefetch synthesis requests by executing
  // `request_synthesis_callback_` for queued chunks while in-flight count is
  // below max concurrency limit.
  void MaybeIssueSynthesisRequest();

  // Maps 0-indexed canonical sentence chunk indices to compressed cache
  // entries.
  std::map<uint32_t, CachedCompressedSegment> session_cache_;

  // Canonical sentence-level timeline generated from input segments.
  std::vector<TextChunk> timeline_;

  // Callback invoked when a prefetch request is dispatched.
  RequestSynthesisCallback request_synthesis_callback_;

  // Callback invoked when text content is chunked.
  OnTextChunkedCallback on_text_chunked_callback_;

  // Currently in-flight sentence chunk indices.
  std::set<uint32_t> inflight_requests_;

  // FIFO queue of sentence chunk indices waiting for concurrency slots.
  base::circular_deque<uint32_t> pending_requests_;

  // Current document session generation sequence ID.
  uint64_t session_sequence_id_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchManager> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_
