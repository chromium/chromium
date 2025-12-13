// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/live_caption/translation_util.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace captions {
std::vector<std::string> SplitSentences(const std::string& text,
                                        const std::string& locale) {
  std::vector<std::string> sentences;
  UErrorCode status = U_ZERO_ERROR;

  // Use icu::BreakIterator instead of base::i18n::BreakIterator to avoid flakey
  // mid-string sentence breaks.
  icu::BreakIterator* iter =
      icu::BreakIterator::createSentenceInstance(locale.c_str(), status);

  DCHECK(U_SUCCESS(status))
      << "ICU could not open a break iterator: " << u_errorName(status) << " ("
      << status << ")";

  // Set the text to be analyzed.
  icu::UnicodeString unicode_text = icu::UnicodeString::fromUTF8(text);
  iter->setText(unicode_text);

  // Iterate over the sentences.
  int32_t start = iter->first();
  int32_t end = iter->next();
  while (end != icu::BreakIterator::DONE) {
    icu::UnicodeString sentence;
    unicode_text.extractBetween(start, end, sentence);
    std::string sentence_string;
    sentence.toUTF8String(sentence_string);
    sentences.emplace_back(sentence_string);
    start = end;
    end = iter->next();
  }

  delete iter;

  return sentences;
}

bool ContainsTrailingSpace(std::string_view str) {
  return !str.empty() && base::IsAsciiWhitespace(str.back());
}

std::string RemoveTrailingSpace(std::string_view str) {
  if (ContainsTrailingSpace(str)) {
    return std::string(str.substr(0, str.length() - 1));
  }

  return std::string(str);
}

std::string RemovePunctuationToLower(std::string str) {
  re2::RE2::GlobalReplace(&str, "[[:punct:]]", "");

  return base::ToLowerASCII(str);
}

std::string GetTranslationCacheKey(std::string_view source_language,
                                   std::string_view target_language,
                                   std::string_view transcription) {
  return base::StrCat({source_language, target_language, "|",
                       RemovePunctuationToLower(std::string(transcription))});
}

bool IsIdeographicLocale(const std::string& locale) {
  // Retrieve the script codes used by the given language from ICU. When the
  // given language consists of two or more scripts, we just use the first
  // script. The size of returned script codes is always < 8. Therefore, we use
  // an array of size 8 so we can include all script codes without insufficient
  // buffer errors.
  UErrorCode error = U_ZERO_ERROR;
  UScriptCode script_code[8];
  int scripts = uscript_getCode(locale.c_str(), script_code,
                                std::size(script_code), &error);

  return U_SUCCESS(error) && scripts >= 1 &&
         (script_code[0] == USCRIPT_HAN || script_code[0] == USCRIPT_HIRAGANA ||
          script_code[0] == USCRIPT_YI || script_code[0] == USCRIPT_KATAKANA);
}

TranslationCache::TranslationCache() = default;
TranslationCache::~TranslationCache() = default;

std::pair<std::string, std::string>
TranslationCache::FindCachedTranslationOrRemaining(
    const std::string& transcript,
    const std::string& source_language,
    const std::string& target_language) const {
  std::vector<std::string> sentences =
      SplitSentences(transcript, source_language);

  std::string cached_translation;
  std::string string_to_translate;
  bool cached_translation_found = true;
  for (std::string_view sentence : sentences) {
    if (cached_translation_found) {
      std::string_view trailing_space;
      if (ContainsTrailingSpace(sentence)) {
        trailing_space =
            sentence.substr(sentence.length() - 1, sentence.length());
      }
      auto translation_cache_key = GetTranslationCacheKey(
          source_language, target_language,
          trailing_space.empty() ? sentence : RemoveTrailingSpace(sentence));
      if (auto iter = translation_cache_.find(translation_cache_key);
          iter != translation_cache_.end()) {
        cached_translation += iter->second;
        if (!trailing_space.empty()) {
          base::StrAppend(&cached_translation, {trailing_space});
        }

        continue;
      }
      cached_translation_found = false;
    }

    string_to_translate = base::StrCat({string_to_translate, sentence});
  }
  if (cached_translation_found) {
    return {"", std::move(cached_translation)};
  } else {
    return {std::move(string_to_translate), std::move(cached_translation)};
  }
}

void TranslationCache::InsertIntoCache(
    const std::string& original_transcription,
    const std::string& result,
    const std::string& source_language,
    const std::string& target_language) {
  auto original_sentences =
      SplitSentences(original_transcription, source_language);
  auto translated_sentences = SplitSentences(result, target_language);
  if (original_sentences.size() > 1 &&
      original_sentences.size() == translated_sentences.size()) {
    for (size_t i = 0; i < original_sentences.size() - 1; i++) {
      // Sentences are always cached without the trailing space.
      std::string sentence = RemoveTrailingSpace(original_sentences[i]);
      translation_cache_.insert(
          {GetTranslationCacheKey(source_language, target_language,
                                  std::move(sentence)),
           RemoveTrailingSpace(translated_sentences[i])});
    }
  }
}

void TranslationCache::Clear() {
  translation_cache_.clear();
}

}  // namespace captions
