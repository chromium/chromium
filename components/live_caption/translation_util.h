// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_
#define COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_
#include <string>
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
}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_TRANSLATION_UTIL_H_
