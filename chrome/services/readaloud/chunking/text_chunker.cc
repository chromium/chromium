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
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace readaloud {

namespace {

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

}  // namespace

std::vector<TextChunk> ChunkText(
    std::u16string_view text,
    ChunkingMode mode,
    std::optional<base::i18n::LanguageTag> locale_tag) {
  switch (mode) {
    case ChunkingMode::kSpeed:
      return ChunkTextSpeedMode(text, locale_tag);
    case ChunkingMode::kQuality:
      // Quality mode is implemented in top CL 8040845.
      NOTREACHED();
  }
}

}  // namespace readaloud
