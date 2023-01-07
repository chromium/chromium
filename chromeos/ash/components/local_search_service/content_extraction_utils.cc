// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/content_extraction_utils.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace ash::local_search_service {

namespace {

using string_matching::TokenizedString;

std::unique_ptr<icu::Transliterator> CreateDiacriticRemover() {
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;

  // Adds a rule to remove diacritic from text. Adds a few characters that are
  // not handled by ICU (ł > l; ø > o; đ > d).
  return base::WrapUnique(icu::Transliterator::createFromRules(
      UNICODE_STRING_SIMPLE("RemoveDiacritic"),
      icu::UnicodeString::fromUTF8("::NFD; ::[:Nonspacing Mark:] Remove; "
                                   "::NFC; ł > l; ø > o; đ > d;"),
      UTRANS_FORWARD, parse_error, status));
}

std::unique_ptr<icu::Transliterator> CreateHyphenRemover() {
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;

  // Hyphen characters list is taken from here: http://jkorpela.fi/dashes.html
  // U+002D(-), U+007E(~), U+058A(֊), U+05BE(־), U+1806(᠆), U+2010(‐),
  // U+2011(‑), U+2012(‒), U+2013(–), U+2014(—), U+2015(―), U+2053(⁓),
  // U+207B(⁻), U+208B(₋), U+2212(−), U+2E3A(⸺ ), U+2E3B(⸻  ), U+301C(〜),
  // U+3030(〰), U+30A0(゠), U+FE58(﹘), U+FE63(﹣), U+FF0D(－).
  return base::WrapUnique(icu::Transliterator::createFromRules(
      UNICODE_STRING_SIMPLE("RemoveHyphen"),
      icu::UnicodeString::fromUTF8("::[-~֊־᠆‐‑‒–—―⁓⁻₋−⸺⸻〜〰゠﹘﹣－] Remove;"),
      UTRANS_FORWARD, parse_error, status));
}

}  // namespace

std::vector<Token> ConsolidateToken(const std::vector<Token>& tokens) {
  std::unordered_map<std::u16string, std::vector<WeightedPosition>> dictionary;
  for (const auto& token : tokens) {
    dictionary[token.content].insert(dictionary[token.content].end(),
                                     token.positions.begin(),
                                     token.positions.end());
  }

  std::vector<Token> results;
  for (const auto& item : dictionary) {
    results.push_back(Token(item.first, item.second));
  }
  return results;
}

std::vector<Token> ExtractContent(const std::string& content_id,
                                  const std::u16string& text,
                                  double weight,
                                  const std::string& locale) {
  // Use two different string tokenizing algorithms for Latin and non Latin
  // locale.
  TokenizedString::Mode mode;
  if (IsNonLatinLocale(locale)) {
    mode = TokenizedString::Mode::kCamelCase;
  } else {
    mode = TokenizedString::Mode::kWords;
  }

  const TokenizedString tokenized_string(text, mode);
  DCHECK(tokenized_string.tokens().size() ==
         tokenized_string.mappings().size());

  const size_t num_tokens = tokenized_string.tokens().size();
  std::vector<Token> tokens;

  for (size_t i = 0; i < num_tokens; i++) {
    const std::u16string word = Normalizer(tokenized_string.tokens()[i]);
    if (IsStopword(word, locale))
      continue;
    tokens.push_back(Token(
        word,
        {WeightedPosition(
            weight, Position(content_id, tokenized_string.mappings()[i].start(),
                             tokenized_string.mappings()[i].end() -
                                 tokenized_string.mappings()[i].start()))}));
  }

  return tokens;
}

bool IsNonLatinLocale(const std::string& locale) {
  static const base::NoDestructor<base::flat_set<std::string>>
      non_latin_locales({"am", "ar", "be", "bg", "bn", "el", "fa", "gu",
                         "hi", "hy", "iw", "ja", "ka", "kk", "km", "kn",
                         "ko", "ky", "lo", "mk", "ml", "mn", "mr", "my",
                         "pa", "ru", "sr", "ta", "te", "th", "uk", "zh"});
  return base::Contains(*non_latin_locales, locale.substr(0, 2));
}

bool IsStopword(const std::u16string& word, const std::string& locale) {
  // TODO(thanhdng): Currently we support stopword list for English only. In the
  // future, when we need to support other languages, creates resource files to
  // store the stopwords.
  if (locale.substr(0, 2) != "en")
    return false;

  // A set of stopwords in English. This set is taken from NLTK library.
  static const base::NoDestructor<base::flat_set<std::string>>
      english_stopwords(
          {"i",         "me",         "my",        "myself",  "we",
           "our",       "ours",       "ourselves", "you",     "you're",
           "you've",    "you'll",     "you'd",     "your",    "yours",
           "yourself",  "yourselves", "he",        "him",     "his",
           "himself",   "she",        "she's",     "her",     "hers",
           "herself",   "it",         "it's",      "its",     "itself",
           "they",      "them",       "their",     "theirs",  "themselves",
           "what",      "which",      "who",       "whom",    "this",
           "that",      "that'll",    "these",     "those",   "am",
           "is",        "are",        "was",       "were",    "be",
           "been",      "being",      "have",      "has",     "had",
           "having",    "do",         "does",      "did",     "doing",
           "a",         "an",         "the",       "and",     "but",
           "if",        "or",         "because",   "as",      "until",
           "while",     "of",         "at",        "by",      "for",
           "with",      "about",      "against",   "between", "into",
           "through",   "during",     "before",    "after",   "above",
           "below",     "to",         "from",      "up",      "down",
           "in",        "out",        "on",        "off",     "over",
           "under",     "again",      "further",   "then",    "once",
           "here",      "there",      "when",      "where",   "why",
           "how",       "all",        "any",       "both",    "each",
           "few",       "more",       "most",      "other",   "some",
           "such",      "no",         "nor",       "not",     "only",
           "own",       "same",       "so",        "than",    "too",
           "very",      "s",          "t",         "can",     "will",
           "just",      "don",        "don't",     "should",  "should've",
           "now",       "d",          "ll",        "m",       "o",
           "re",        "ve",         "y",         "ain",     "aren",
           "aren't",    "couldn",     "couldn't",  "didn",    "didn't",
           "doesn",     "doesn't",    "hadn",      "hadn't",  "hasn",
           "hasn't",    "haven",      "haven't",   "isn",     "isn't",
           "ma",        "mightn",     "mightn't",  "mustn",   "mustn't",
           "needn",     "needn't",    "shan",      "shan't",  "shouldn",
           "shouldn't", "wasn",       "wasn't",    "weren",   "weren't",
           "won",       "won't",      "wouldn",    "wouldn't"});
  return base::Contains(*english_stopwords, base::UTF16ToUTF8(word));
}

std::u16string Normalizer(const std::u16string& word, bool remove_hyphen) {
  // Case folding.
  icu::UnicodeString source = icu::UnicodeString::fromUTF8(
      base::UTF16ToUTF8(base::i18n::FoldCase(word)));

  // Removes diacritic.
  static base::NoDestructor<std::unique_ptr<icu::Transliterator>>
      diacritic_remover(CreateDiacriticRemover());
  (*diacritic_remover)->transliterate(source);

  // Removes hyphen.
  if (remove_hyphen) {
    static base::NoDestructor<std::unique_ptr<icu::Transliterator>>
        hyphen_remover(CreateHyphenRemover());
    (*hyphen_remover)->transliterate(source);
  }

  return base::i18n::UnicodeStringToString16(source);
}

}  // namespace ash::local_search_service
