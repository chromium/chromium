// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/chunking/text_chunker.h"

#include <iterator>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/language_tag.h"
#include "base/strings/string_util.h"

namespace readaloud {

namespace {

// Target chunk size in 16-bit code units (char16_t) for prosody grouping.
constexpr size_t kTargetChunkSize = 450;

std::unique_ptr<base::i18n::BreakIterator> CreateSentenceBreakIterator(
    std::u16string_view text,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  if (text.empty()) {
    return nullptr;
  }

  std::unique_ptr<base::i18n::BreakIterator> iter;
  if (locale_tag.has_value()) {
    iter = std::make_unique<base::i18n::BreakIterator>(
        text, base::i18n::BreakIterator::BREAK_SENTENCE, *locale_tag);
  } else {
    iter = std::make_unique<base::i18n::BreakIterator>(
        text, base::i18n::BreakIterator::BREAK_SENTENCE);
  }

  if (!iter->Init()) {
    return nullptr;
  }
  return iter;
}

// Trims leading/trailing whitespace from the sentence boundary range [start, end)
// and appends a TextChunk with its absolute character offset to `chunks` if non-empty.
void ProcessNextSentence(std::u16string_view text,
                         size_t start,
                         size_t end,
                         std::vector<TextChunk>& chunks) {
  DCHECK_LE(start, end);
  DCHECK_LE(end, text.size());
  std::u16string_view sentence = text.substr(start, end - start);
  std::u16string_view trimmed_sentence =
      base::TrimWhitespace(sentence, base::TRIM_ALL);
  if (trimmed_sentence.empty()) {
    return;
  }

  CHECK_GE(trimmed_sentence.data(), text.data());
  size_t absolute_start =
      static_cast<size_t>(std::distance(text.data(), trimmed_sentence.data()));
  CHECK_LE(absolute_start + trimmed_sentence.size(), text.size());

  chunks.emplace_back(trimmed_sentence, absolute_start);
}

void GetSurroundingWhitespace(std::u16string_view sentence,
                              std::u16string_view trimmed,
                              std::u16string_view* out_leading_ws,
                              std::u16string_view* out_trailing_ws) {
  DCHECK(out_leading_ws);
  DCHECK(out_trailing_ws);
  CHECK_GE(trimmed.data(), sentence.data());
  size_t leading_len =
      static_cast<size_t>(std::distance(sentence.data(), trimmed.data()));
  *out_leading_ws = sentence.substr(0, leading_len);
  *out_trailing_ws = sentence.substr(leading_len + trimmed.length());
}

bool ShouldFlushBeforeSentence(size_t accum_start,
                               size_t sentence_end,
                               std::u16string_view leading_ws,
                               size_t max_chunk_size) {
  if (accum_start == std::u16string_view::npos) {
    return false;
  }
  return leading_ws.find_first_of(u"\n\r") != std::u16string_view::npos ||
         (sentence_end - accum_start > max_chunk_size);
}

std::vector<TextChunk> ChunkTextSpeedMode(
    std::u16string_view text,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  std::vector<TextChunk> chunks;
  std::unique_ptr<base::i18n::BreakIterator> bi =
      CreateSentenceBreakIterator(text, locale_tag);
  if (!bi) {
    return chunks;
  }

  while (bi->Advance()) {
    ProcessNextSentence(text, bi->prev(), bi->pos(), chunks);
  }

  return chunks;
}

std::vector<TextChunk> ChunkTextQualityMode(
    std::u16string_view text,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  std::vector<TextChunk> chunks;
  std::unique_ptr<base::i18n::BreakIterator> bi =
      CreateSentenceBreakIterator(text, locale_tag);
  if (!bi) {
    return chunks;
  }

  size_t accum_start = std::u16string_view::npos;
  size_t accum_end = std::u16string_view::npos;

  auto flush_accumulator = [&accum_start, &accum_end, &text, &chunks]() {
    if (accum_start != std::u16string_view::npos) {
      ProcessNextSentence(text, accum_start, accum_end, chunks);
      accum_start = std::u16string_view::npos;
      accum_end = std::u16string_view::npos;
    }
  };

  while (bi->Advance()) {
    size_t sentence_start = bi->prev();
    size_t sentence_end = bi->pos();
    std::u16string_view sentence =
        text.substr(sentence_start, sentence_end - sentence_start);

    std::u16string_view trimmed =
        base::TrimWhitespace(sentence, base::TRIM_ALL);
    if (trimmed.empty()) {
      // If a whitespace-only segment contains a newline/paragraph break,
      // flush the accumulated chunk before skipping.
      if (sentence.find_first_of(u"\n\r") != std::u16string_view::npos) {
        flush_accumulator();
      }
      continue;
    }

    std::u16string_view leading_ws;
    std::u16string_view trailing_ws;
    GetSurroundingWhitespace(sentence, trimmed, &leading_ws, &trailing_ws);

    // Flush before appending if there is a paragraph boundary in leading
    // whitespace or if adding this sentence exceeds the target chunk size limit.
    if (ShouldFlushBeforeSentence(accum_start, sentence_end, leading_ws,
                                  kTargetChunkSize)) {
      flush_accumulator();
    }

    if (accum_start == std::u16string_view::npos) {
      accum_start = sentence_start;
    }
    accum_end = sentence_end;

    // Flush after appending if there is a paragraph boundary in trailing whitespace.
    if (trailing_ws.find_first_of(u"\n\r") != std::u16string_view::npos) {
      flush_accumulator();
    }
  }

  flush_accumulator();
  return chunks;
}

}  // namespace

std::vector<TextChunk> ChunkText(
    std::u16string_view text,
    ChunkingMode mode,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  switch (mode) {
    case ChunkingMode::kSpeed:
      return ChunkTextSpeedMode(text, locale_tag);
    case ChunkingMode::kQuality:
      return ChunkTextQualityMode(text, locale_tag);
  }
}

}  // namespace readaloud
