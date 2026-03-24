// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

namespace {

ClassifiedQuery CompositeClassify(base::span<const QueryClassifier> classifiers,
                                  std::u16string_view query) {
  for (const QueryClassifier& classifier : classifiers) {
    ClassifiedQuery classified_query = classifier.Run(query);
    if (classified_query.intent != QueryIntentType::kUnknown) {
      return classified_query;
    }
  }
  return ClassifiedQuery(QueryIntentType::kUnknown);
}

// Classifies the query by performing substring matching against a predefined
// set of keyword phrases. Normalizes the query by lowercasing, removing
// punctuation, and stripping stop words before attempting matches. If a
// keyword phrase is found as a standalone phrase, it sets the corresponding
// intent and extracts the remaining words in the query as required words.
ClassifiedQuery KeywordQueryClassify(std::u16string_view query) {
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

  // Normalize the query by:
  // - lowercasing,
  // - removing common punctuation,
  // - removing stop words,
  // - trimming sequences of whitespaces.
  std::u16string normalized_query = base::ToLowerASCII(query);
  std::vector<std::u16string_view> all_words =
      base::SplitStringPiece(normalized_query, u" ;?,.'", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  std::erase_if(all_words, [](std::u16string_view word) {
    return kStopWords.contains(word);
  });
  normalized_query = base::JoinString(all_words, u" ");

  if (normalized_query.empty()) {
    return ClassifiedQuery(QueryIntentType::kUnknown);
  }

  QueryIntentType matched_intent = QueryIntentType::kUnknown;
  std::u16string_view matched_keyword_phrase;

  // Attempts to find any of the `keyword_phrases` in `normalized_query`.
  // If a match is found, `matched_intent` and `matched_keyword_phrase`
  // are updated. Only the first successful match across all calls to
  // `try_match` is kept.
  auto try_match = [&](QueryIntentType intent, auto... keyword_phrases) {
    if (matched_intent != QueryIntentType::kUnknown) {
      return;
    }

    // Checks if a keyword phrase (which may contain multiple words) is present
    // in `normalized_query` as a contiguous sequence of whole words. This
    // prevents false positives where a keyword is a substring of another word
    // (e.g., "vin" matching "province").
    auto check_phrase = [&](std::u16string_view phrase) {
      DCHECK(base::IsStringASCII(phrase));
      DCHECK_EQ(phrase, base::ToLowerASCII(phrase));
      if (internal::ContainsStandalonePhrase(normalized_query, phrase)) {
        matched_intent = intent;
        matched_keyword_phrase = phrase;
        return true;
      }
      return false;
    };
    (check_phrase(keyword_phrases) || ...);
  };

  // IMPORTANT: Always place super-strings before sub-strings in the argument
  // list to ensure the longest phrase is matched first (e.g., "license plate
  // state" before "plate state").
  // Note that all search phrases need to be lowercase and ASCII only at
  // the moment.
  // Vehicle
  try_match(QueryIntentType::kVehicleVin, u"vin");
  try_match(QueryIntentType::kVehicleMake, u"vehicle make", u"car make");
  try_match(QueryIntentType::kVehicleModel, u"vehicle model", u"car model");
  try_match(QueryIntentType::kVehicleYear, u"vehicle year", u"car year");
  try_match(QueryIntentType::kVehicleOwner, u"vehicle owner", u"car owner");
  try_match(QueryIntentType::kVehiclePlateState, u"license plate state",
            u"plate state");
  try_match(QueryIntentType::kVehicle, u"vehicle", u"car");
  try_match(QueryIntentType::kVehiclePlateNumber, u"license plate",
            u"plate number", u"plate");

  // Passport
  try_match(QueryIntentType::kPassportNumber, u"passport number");
  try_match(QueryIntentType::kPassportExpirationDate, u"passport expiration",
            u"passport expiry");
  try_match(QueryIntentType::kPassportIssueDate, u"passport issue");
  try_match(QueryIntentType::kPassportCountry, u"passport country");
  try_match(QueryIntentType::kPassportName, u"passport name");
  try_match(QueryIntentType::kPassportFull, u"passport");

  // Flight Reservation
  try_match(QueryIntentType::kFlightReservationFlightNumber, u"flight number");
  try_match(QueryIntentType::kFlightReservationTicketNumber, u"ticket number");
  try_match(QueryIntentType::kFlightReservationConfirmationCode,
            u"confirmation code", u"flight confirmation");
  try_match(QueryIntentType::kFlightReservationPassengerName, u"passenger name",
            u"flight passenger");
  try_match(QueryIntentType::kFlightReservationDepartureAirport,
            u"departure airport", u"from airport");
  try_match(QueryIntentType::kFlightReservationArrivalAirport,
            u"arrival airport", u"to airport");
  try_match(QueryIntentType::kFlightReservationDepartureDate, u"departure date",
            u"flight date");
  try_match(QueryIntentType::kFlightReservationFull, u"flight reservation",
            u"flight", u"reservation");

  // Order
  try_match(QueryIntentType::kOrderId, u"order id", u"order number");
  try_match(QueryIntentType::kOrderAccount, u"order account");
  try_match(QueryIntentType::kOrderDate, u"order date");
  try_match(QueryIntentType::kOrderMerchantName, u"merchant name",
            u"store name", u"order merchant");
  try_match(QueryIntentType::kOrderMerchantDomain, u"merchant domain");
  try_match(QueryIntentType::kOrderProductNames, u"product names",
            u"order products");
  try_match(QueryIntentType::kOrderGrandTotal, u"grand total", u"order total",
            u"total amount");
  try_match(QueryIntentType::kOrderFull, u"order");

  // National ID Card
  try_match(QueryIntentType::kNationalIdCardNumber, u"national id number");
  try_match(QueryIntentType::kNationalIdCardExpirationDate,
            u"national id expiration", u"national id expiry");
  try_match(QueryIntentType::kNationalIdCardIssueDate, u"national id issue");
  try_match(QueryIntentType::kNationalIdCardCountry, u"national id country");
  try_match(QueryIntentType::kNationalIdCardName, u"national id name");
  try_match(QueryIntentType::kNationalIdCardFull, u"national id");

  // Redress Number
  try_match(QueryIntentType::kRedressNumberName, u"redress number name",
            u"redress name");
  try_match(QueryIntentType::kRedressNumberNumber, u"redress number");
  try_match(QueryIntentType::kRedressNumberFull, u"redress");

  // Known Traveler Number
  try_match(QueryIntentType::kKnownTravelerNumberName,
            u"known traveler number name", u"ktn name");
  try_match(QueryIntentType::kKnownTravelerNumberNumber,
            u"known traveler number number", u"ktn number");
  try_match(QueryIntentType::kKnownTravelerNumberExpirationDate,
            u"known traveler number expiration", u"ktn expiration",
            u"ktn expiry");
  try_match(QueryIntentType::kKnownTravelerNumberFull, u"known traveler number",
            u"traveler number", u"ktn");

  // Driver's License
  try_match(QueryIntentType::kDriversLicenseNumber, u"drivers license number",
            u"driver's license number", u"driver license number");
  try_match(QueryIntentType::kDriversLicenseState, u"drivers license state",
            u"driver's license state");
  try_match(QueryIntentType::kDriversLicenseExpirationDate,
            u"drivers license expiration", u"driver's license expiration",
            u"drivers license expiry", u"driver's license expiry");
  try_match(QueryIntentType::kDriversLicenseIssueDate, u"drivers license issue",
            u"driver's license issue");
  try_match(QueryIntentType::kDriversLicenseName, u"drivers license name",
            u"driver's license name");
  try_match(QueryIntentType::kDriversLicenseFull, u"drivers license",
            u"driver's license", u"driving license", u"license");

  // Personal profiles
  try_match(QueryIntentType::kAddressZip, u"zip code", u"zip-code", u"zip",
            u"postal code", u"postal-code", u"postal");
  try_match(QueryIntentType::kAddressCity, u"city", u"town");
  try_match(QueryIntentType::kAddressState, u"state", u"province");
  try_match(QueryIntentType::kAddressCountry, u"country");
  try_match(QueryIntentType::kAddressStreetAddress, u"street");
  try_match(QueryIntentType::kPhone, u"phone", u"mobile", u"telephone");
  try_match(QueryIntentType::kEmail, u"e-mail", u"email");
  try_match(QueryIntentType::kCompanyName, u"organization", u"company");
  try_match(QueryIntentType::kNameFull, u"name");
  try_match(QueryIntentType::kAddressFull, u"home address", u"work address",
            u"address", u"home", u"work", u"live");
  try_match(QueryIntentType::kIban, u"iban", u"bank account");

  if (matched_intent == QueryIntentType::kUnknown) {
    return ClassifiedQuery(QueryIntentType::kUnknown);
  }

  // Remove the matched keyword phrase from the query and extract the remaining
  // words.
  base::ReplaceFirstSubstringAfterOffset(&normalized_query, 0,
                                         matched_keyword_phrase, u"");
  std::vector<std::u16string> filter_words = base::SplitString(
      normalized_query, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return ClassifiedQuery(matched_intent, std::move(filter_words));
}

ClassifiedQuery GeminiClassify(std::u16string_view query) {
  // TODO(crbug.com/490211801): Implement Gemini model call within MES.
  return ClassifiedQuery(QueryIntentType::kUnknown);
}

}  // namespace

ClassifiedQuery::ClassifiedQuery(QueryIntentType intent,
                                 std::vector<std::u16string> filter_words)
    : intent(intent), filter_words(std::move(filter_words)) {}

ClassifiedQuery::ClassifiedQuery(const ClassifiedQuery&) = default;
ClassifiedQuery& ClassifiedQuery::operator=(const ClassifiedQuery&) = default;
ClassifiedQuery::ClassifiedQuery(ClassifiedQuery&&) = default;
ClassifiedQuery& ClassifiedQuery::operator=(ClassifiedQuery&&) = default;
ClassifiedQuery::~ClassifiedQuery() = default;

QueryClassifier CreateQueryClassifier() {
  std::vector<QueryClassifier> classifiers = {
      internal::CreateKeywordQueryClassifier(),
      internal::CreateGeminiClassifier()};
  return base::BindRepeating(&CompositeClassify, std::move(classifiers));
}

namespace internal {

bool ContainsStandalonePhrase(std::u16string_view haystack,
                              std::u16string_view needle) {
  if (needle.empty()) {
    return true;
  }

  size_t pos = haystack.find(needle);
  while (pos != std::u16string::npos) {
    bool start_of_word = pos == 0 || haystack[pos - 1] == u' ';
    bool end_of_word = pos + needle.length() == haystack.length() ||
                       haystack[pos + needle.length()] == u' ';
    if (start_of_word && end_of_word) {
      return true;
    }
    pos = haystack.find(needle, pos + 1);
  }
  return false;
}

QueryClassifier CreateKeywordQueryClassifier() {
  return base::BindRepeating(&KeywordQueryClassify);
}

QueryClassifier CreateGeminiClassifier() {
  return base::BindRepeating(&GeminiClassify);
}

}  // namespace internal

}  // namespace accessibility_annotator
