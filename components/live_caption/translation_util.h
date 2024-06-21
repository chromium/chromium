// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_
#define COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace captions {

// Split the transcription into sentences. Spaces are included in the preceding
// sentence.
std::vector<std::string> SplitSentences(const std::string& text,
                                        const std::string& locale);
bool ContainsTrailingSpace(const std::string& str);

std::string RemoveTrailingSpace(const std::string& str);

std::string RemovePunctuationToLower(std::string str);
std::string GetTranslationCacheKey(const std::string& source_language,
                                   const std::string& target_language,
                                   const std::string& transcription);
bool IsIdeographicLocale(const std::string& locale);

// Used to cache translations to avoid retranslating the same string. The key
// is the source and target language codes followed by a `|` separator
// character and the original text. The value is the translated text. This
// cache is cleared upon receiving a final recognition event. The size of this
// cache depends on the frequency of partial and final recognition events, but
// is typically under ~10 entries.
class TranslationCache {
 public:
  TranslationCache();
  virtual ~TranslationCache();
  // returns a pair: first is remaining str to translate, the next is the cached
  // value so far.
  std::pair<std::string, std::string> FindCachedTranslationOrRemaining(
      const std::string& transcript,
      const std::string& source_language,
      const std::string& target_language) const;
  void InsertIntoCache(const std::string& original_transcription,
                       const std::string& translation,
                       const std::string& source_language,
                       const std::string& target_language);

  void Clear();

 private:
  std::unordered_map<std::string, std::string> translation_cache_;
};
}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_
