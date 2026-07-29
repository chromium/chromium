// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_CHUNKING_TEXT_CHUNKER_H_
#define CHROME_SERVICES_READALOUD_CHUNKING_TEXT_CHUNKER_H_

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "base/i18n/language_tag.h"

namespace readaloud {

// Specifies the strategy used when segmenting text into chunks.
enum class ChunkingMode {
  // Fast sentence-based chunking using ICU sentence break iteration.
  kSpeed,
  // High-quality chunking optimized for natural prosody.
  // TODO(b/527525619): Unimplemented.
  kQuality,
};

// Represents a contiguous segment of text within the original string.
struct TextChunk {
  // A zero-copy string view referencing a slice of the original text.
  std::u16string_view text;
  // The 0-based code unit offset of this chunk relative to the original text.
  // NOTE: This offset is measured in 16-bit code units (char16_t), NOT Unicode
  // code points (UChar32). Take care not to split surrogate pairs across offsets.
  size_t start_code_unit_offset = 0;

  friend bool operator==(const TextChunk&, const TextChunk&) = default;
};

// Splits `text` into sentence chunks according to `mode`.
// Leading and trailing whitespace is trimmed from each resulting chunk.
// `locale_tag` specifies the optional BCP-47 LanguageTag for sentence
// breaking. If omitted, defaults to the system ICU locale.
// Returns an empty vector if `text` is empty or contains only whitespace.
std::vector<TextChunk> ChunkText(
    std::u16string_view text,
    ChunkingMode mode = ChunkingMode::kSpeed,
    std::optional<base::i18n::LanguageTag> locale_tag = std::nullopt);

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_CHUNKING_TEXT_CHUNKER_H_
