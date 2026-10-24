// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/content_extraction_utils.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/containers/fixed_flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/transliterator.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::local_search_service {

namespace {

using string_matching::TokenizedString;

std::unique_ptr<base::i18n::Transliterator> CreateDiacriticRemover() {
  // Adds a rule to remove diacritic from text. Adds a few characters that are
  // not handled by ICU (ł > l; ø > o; đ > d).
  return base::i18n::CreateTransliteratorFromRules(
      "RemoveDiacritic",
      "::NFD; ::[:Nonspacing Mark:] Remove;::NFC; ł > l; ø > o; đ > d;");
}

std::unique_ptr<base::i18n::Transliterator> CreateHyphenRemover() {
  // Hyphen characters list is taken from here: http://jkorpela.fi/dashes.html
  // U+002D(-), U+007E(~), U+058A(֊), U+05BE(־), U+1806(᠆), U+2010(‐),
  // U+2011(‑), U+2012(‒), U+2013(–), U+2014(—), U+2015(―), U+2053(⁓),
  // U+207B(⁻), U+208B(₋), U+2212(−), U+2E3A(⸺ ), U+2E3B(⸻  ), U+301C(〜),
  // U+3030(〰), U+30A0(゠), U+FE58(﹘), U+FE63(﹣), U+FF0D(－).
  return base::i18n::CreateTransliteratorFromRules(
      "RemoveHyphen", "::[-~֊־᠆‐‑‒–—―⁓⁻₋−⸺⸻〜〰゠﹘﹣－] Remove;");
}

}  // namespace

std::vector<Token> ConsolidateToken(const std::vector<Token>& tokens) {
  std::unordered_map<std::u16string, std::vector<WeightedPosition>> dictionary;
  for (const auto& token : tokens) {
    base::Extend(dictionary[token.content], token.positions);
  }

  std::vector<Token> results;
  results.reserve(dictionary.size());
  for (auto& item : dictionary) {
    results.emplace_back(item.first, std::move(item.second));
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
  static constexpr auto kNonLatinLocales =
      base::MakeFixedFlatSet<std::string_view>(
          {"am", "ar", "be", "bg", "bn", "el", "fa", "gu", "hi", "hy", "iw",
           "ja", "ka", "kk", "km", "kn", "ko", "ky", "lo", "mk", "ml", "mn",
           "mr", "my", "pa", "ru", "sr", "ta", "te", "th", "uk", "zh"});
  return kNonLatinLocales.contains(std::string_view(locale).substr(0, 2));
}

bool IsStopword(const std::u16string& word, const std::string& locale) {
  // TODO(thanhdng): Currently we support stopword list for English only. In the
  // future, when we need to support other languages, creates resource files to
  // store the stopwords.
  if (!locale.starts_with("en")) {
    return false;
  }

  // A set of stopwords in English. This set is taken from NLTK library.
  static constexpr auto kEnglishStopWords =
      base::MakeFixedFlatSet<std::u16string_view>(
          {u"i",         u"me",         u"my",        u"myself",  u"we",
           u"our",       u"ours",       u"ourselves", u"you",     u"you're",
           u"you've",    u"you'll",     u"you'd",     u"your",    u"yours",
           u"yourself",  u"yourselves", u"he",        u"him",     u"his",
           u"himself",   u"she",        u"she's",     u"her",     u"hers",
           u"herself",   u"it",         u"it's",      u"its",     u"itself",
           u"they",      u"them",       u"their",     u"theirs",  u"themselves",
           u"what",      u"which",      u"who",       u"whom",    u"this",
           u"that",      u"that'll",    u"these",     u"those",   u"am",
           u"is",        u"are",        u"was",       u"were",    u"be",
           u"been",      u"being",      u"have",      u"has",     u"had",
           u"having",    u"do",         u"does",      u"did",     u"doing",
           u"a",         u"an",         u"the",       u"and",     u"but",
           u"if",        u"or",         u"because",   u"as",      u"until",
           u"while",     u"of",         u"at",        u"by",      u"for",
           u"with",      u"about",      u"against",   u"between", u"into",
           u"through",   u"during",     u"before",    u"after",   u"above",
           u"below",     u"to",         u"from",      u"up",      u"down",
           u"in",        u"out",        u"on",        u"off",     u"over",
           u"under",     u"again",      u"further",   u"then",    u"once",
           u"here",      u"there",      u"when",      u"where",   u"why",
           u"how",       u"all",        u"any",       u"both",    u"each",
           u"few",       u"more",       u"most",      u"other",   u"some",
           u"such",      u"no",         u"nor",       u"not",     u"only",
           u"own",       u"same",       u"so",        u"than",    u"too",
           u"very",      u"s",          u"t",         u"can",     u"will",
           u"just",      u"don",        u"don't",     u"should",  u"should've",
           u"now",       u"d",          u"ll",        u"m",       u"o",
           u"re",        u"ve",         u"y",         u"ain",     u"aren",
           u"aren't",    u"couldn",     u"couldn't",  u"didn",    u"didn't",
           u"doesn",     u"doesn't",    u"hadn",      u"hadn't",  u"hasn",
           u"hasn't",    u"haven",      u"haven't",   u"isn",     u"isn't",
           u"ma",        u"mightn",     u"mightn't",  u"mustn",   u"mustn't",
           u"needn",     u"needn't",    u"shan",      u"shan't",  u"shouldn",
           u"shouldn't", u"wasn",       u"wasn't",    u"weren",   u"weren't",
           u"won",       u"won't",      u"wouldn",    u"wouldn't"});
  return kEnglishStopWords.contains(word);
}

std::u16string Normalizer(const std::u16string& word, bool remove_hyphen) {
  // Fast path for ASCII strings: pure ASCII characters have no combining
  // diacritics and cannot contain ł, ø, or đ, and the only ASCII characters in
  // the hyphen-removal set are '-' and '~'.
  if (base::IsStringASCII(word)) {
    std::u16string result;
    result.reserve(word.size());
    for (char16_t c : word) {
      if (remove_hyphen && (c == '-' || c == '~')) {
        continue;
      }
      result.push_back(base::ToLowerASCII(c));
    }
    return result;
  }

  // Case folding.
  std::u16string case_folded = base::i18n::FoldCase(word);

  // Removes diacritic.
  static base::NoDestructor<std::unique_ptr<base::i18n::Transliterator>>
      diacritic_remover(CreateDiacriticRemover());
  std::u16string diacritic_removed =
      (*diacritic_remover)->Transliterate(case_folded);

  // Removes hyphen.
  if (remove_hyphen) {
    static base::NoDestructor<std::unique_ptr<base::i18n::Transliterator>>
        hyphen_remover(CreateHyphenRemover());
    return (*hyphen_remover)->Transliterate(diacritic_removed);
  }
  return diacritic_removed;
}

}  // namespace ash::local_search_service
