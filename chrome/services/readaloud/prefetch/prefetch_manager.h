// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_
#define CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/common/readaloud/read_aloud.mojom-forward.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
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

// Manages document-bound caching of compressed speech synthesis audio.
class PrefetchManager {
 public:
  PrefetchManager();
  PrefetchManager(const PrefetchManager&) = delete;
  PrefetchManager& operator=(const PrefetchManager&) = delete;
  ~PrefetchManager();

  // Document-bound lifecycle:
  // Sets new document text segments, uses ChunkText(..., kSpeed) to establish
  // the canonical sentence-level timeline (0...N-1), and purges session_cache_.
  void SetTextContent(
      const std::vector<read_aloud::mojom::TextSegmentPtr>& segments,
      std::optional<base::i18n::LanguageTag> locale_tag = std::nullopt);

  // Clears all cached segments and resets the timeline.
  void ResetSession();

  // Cache accessors & modifiers:
  bool HasCachedSegment(int32_t chunk_index) const;
  const CachedCompressedSegment* GetCachedSegment(int32_t chunk_index) const;
  void InsertCachedSegment(
      int32_t chunk_index,
      scoped_refptr<media::DecoderBuffer> opus_buffer,
      std::vector<DecodedAudioSegment::WordTiming> timings);
  void ClearCache();

  // Timeline inspection:
  size_t GetTimelineChunkCount() const;
  const std::vector<TextChunk>& GetTimelineChunks() const;

 private:
  // Maps 0-indexed canonical sentence chunk indices to compressed cache
  // entries.
  std::map<int32_t, CachedCompressedSegment> session_cache_;

  // Canonical sentence-level timeline generated from input segments.
  std::vector<TextChunk> timeline_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_PREFETCH_PREFETCH_MANAGER_H_
