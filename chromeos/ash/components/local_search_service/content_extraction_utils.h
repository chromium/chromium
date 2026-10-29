// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_

#include <string>

#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace base::i18n {
class LanguageTag;
}  // namespace base::i18n

namespace ash::local_search_service {

// Given a list of tokens, returns a list of tokens where each token has a
// unique content.
std::vector<Token> ConsolidateToken(const std::vector<Token>& tokens);

// Given a text document, returns a list of Tokens.
// `locale` is the language of `text` (e.g. base::i18n::GetDefaultIcuLocale()),
// it determines how `text` is tokenized and which stopwords are removed.
std::vector<Token> ExtractContent(const std::string& content_id,
                                  const std::u16string& text,
                                  double weight,
                                  const base::i18n::LanguageTag& locale);

// Checks if the language of `locale` is written in a non Latin script.
bool IsNonLatinLocale(const base::i18n::LanguageTag& locale);

// Checks if a word is a stopword in the language of `locale`.
bool IsStopword(const std::u16string& word,
                const base::i18n::LanguageTag& locale);

// Returns a normalized version of a string16: removes diacritics, convert to
// lower-case and possibly remove hyphen from the text (set to true by default).
std::u16string Normalizer(const std::u16string& word,
                          bool remove_hyphen = true);

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
