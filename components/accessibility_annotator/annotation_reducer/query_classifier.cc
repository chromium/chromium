// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/annotation_reducer/query_classifier.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/annotation_reducer/query_intent_type.h"

namespace annotation_reducer {

namespace {

// Check if the 'words' contains 'seq' as a sub-sequence.
bool ContainsSequence(base::span<const std::u16string> words,
                      base::span<const std::u16string> seq) {
  if (seq.empty()) {
    return true;
  }
  auto result = std::ranges::search(words, seq);
  return !result.empty();
}

}  // namespace

QueryClassifier::QueryClassifier() = default;
QueryClassifier::~QueryClassifier() = default;

QueryIntentType QueryClassifier::Classify(const std::u16string& query) {
  // Hardcoded list of stop words.
  // TODO(crbug.com/485682510): Load stopwords from a single JSON.
  static constexpr auto kStopWords =
      base::MakeFixedFlatSet<std::u16string_view>({
          u"i",       u"me",       u"my",         u"myself",     u"we",
          u"our",     u"ours",     u"ourselves",  u"you",        u"your",
          u"yours",   u"yourself", u"yourselves", u"he",         u"him",
          u"his",     u"himself",  u"she",        u"her",        u"hers",
          u"herself", u"it",       u"its",        u"itself",     u"they",
          u"them",    u"their",    u"theirs",     u"themselves", u"what",
          u"which",   u"who",      u"whom",       u"this",       u"that",
          u"these",   u"those",    u"am",         u"is",         u"are",
          u"was",     u"were",     u"be",         u"been",       u"being",
          u"have",    u"has",      u"had",        u"having",     u"do",
          u"does",    u"did",      u"doing",      u"a",          u"an",
          u"the",     u"and",      u"but",        u"if",         u"or",
          u"because", u"as",       u"until",      u"while",      u"of",
          u"at",      u"by",       u"for",        u"with",       u"about",
          u"against", u"between",  u"into",       u"through",    u"during",
          u"before",  u"after",    u"above",      u"below",      u"to",
          u"from",    u"up",       u"down",       u"in",         u"out",
          u"on",      u"off",      u"over",       u"under",      u"again",
          u"further", u"then",     u"once",       u"here",       u"there",
          u"when",    u"where",    u"why",        u"how",        u"all",
          u"any",     u"both",     u"each",       u"few",        u"more",
          u"most",    u"other",    u"some",       u"such",       u"no",
          u"nor",     u"not",      u"only",       u"own",        u"same",
          u"so",      u"than",     u"too",        u"very",       u"s",
          u"t",       u"can",      u"will",       u"just",       u"don",
          u"should",  u"now",      u"show",       u"what's",     u"please",
          u"tell",    u"get",      u"detail",     u"details",    u"whats",
      });

  std::u16string lower_query = base::ToLowerASCII(query);
  // Split by space and common punctuation that might not be part of words.
  std::vector<std::u16string> all_words = base::SplitString(
      lower_query, u" ;?,.'", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Remove stop words from all_words in place.
  std::erase_if(all_words, [](const std::u16string& word) {
    return kStopWords.contains(word);
  });

  if (all_words.empty()) {
    return QueryIntentType::kUnknown;
  }

  auto contains = [&](auto... keyword_phrases) {
    auto contains_single_phrase = [&](const std::u16string& phrase) {
      std::vector<std::u16string> seq_words = base::SplitString(
          phrase, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      return !seq_words.empty() && ContainsSequence(all_words, seq_words);
    };
    return (contains_single_phrase(keyword_phrases) || ...);
  };

  if (contains(u"zip", u"zip-code", u"postal-code", u"postal")) {
    return QueryIntentType::kAddressZip;
  }
  if (contains(u"city", u"town")) {
    return QueryIntentType::kAddressCity;
  }
  if (contains(u"state", u"province")) {
    return QueryIntentType::kAddressState;
  }
  if (contains(u"country")) {
    return QueryIntentType::kAddressCountry;
  }
  if (contains(u"street")) {
    return QueryIntentType::kAddressStreetAddress;
  }
  if (contains(u"phone", u"mobile", u"telephone")) {
    return QueryIntentType::kPhone;
  }
  if (contains(u"email", u"e-mail")) {
    return QueryIntentType::kEmail;
  }
  if (contains(u"name")) {
    return QueryIntentType::kNameFull;
  }
  if (contains(u"address", u"home", u"work", u"live")) {
    return QueryIntentType::kAddressFull;
  }
  if (contains(u"iban", u"bank account")) {
    return QueryIntentType::kIban;
  }
  if (contains(u"license plate", u"plate number", u"plate")) {
    return QueryIntentType::kVehiclePlateNumber;
  }
  if (contains(u"vin")) {
    return QueryIntentType::kVehicleVin;
  }
  if (contains(u"vehicle", u"car")) {
    return QueryIntentType::kVehicle;
  }
  if (contains(u"passport")) {
    return QueryIntentType::kPassportFull;
  }
  if (contains(u"flight reservation", u"flight", u"reservation")) {
    return QueryIntentType::kFlightReservationFull;
  }
  if (contains(u"national id", u"id card", u"id")) {
    return QueryIntentType::kNationalIdCardFull;
  }
  if (contains(u"redress number", u"redress")) {
    return QueryIntentType::kRedressNumberFull;
  }
  if (contains(u"known traveler number", u"traveler number", u"ktn")) {
    return QueryIntentType::kKnownTravelerNumberFull;
  }
  if (contains(u"drivers license", u"driver's license", u"driving license",
               u"license")) {
    return QueryIntentType::kDriversLicenseFull;
  }

  return QueryIntentType::kUnknown;
}

}  // namespace annotation_reducer
