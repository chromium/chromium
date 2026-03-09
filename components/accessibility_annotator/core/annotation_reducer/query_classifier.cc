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
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

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

  // Vehicle
  if (contains(u"vin")) {
    return QueryIntentType::kVehicleVin;
  }
  if (contains(u"vehicle make", u"car make")) {
    return QueryIntentType::kVehicleMake;
  }
  if (contains(u"vehicle model", u"car model")) {
    return QueryIntentType::kVehicleModel;
  }
  if (contains(u"vehicle year", u"car year")) {
    return QueryIntentType::kVehicleYear;
  }
  if (contains(u"vehicle owner", u"car owner")) {
    return QueryIntentType::kVehicleOwner;
  }
  if (contains(u"plate state", u"license plate state")) {
    return QueryIntentType::kVehiclePlateState;
  }
  if (contains(u"vehicle", u"car")) {
    return QueryIntentType::kVehicle;
  }
  if (contains(u"license plate", u"plate number", u"plate")) {
    return QueryIntentType::kVehiclePlateNumber;
  }

  // Passport
  if (contains(u"passport number")) {
    return QueryIntentType::kPassportNumber;
  }
  if (contains(u"passport expiration", u"passport expiry")) {
    return QueryIntentType::kPassportExpirationDate;
  }
  if (contains(u"passport issue")) {
    return QueryIntentType::kPassportIssueDate;
  }
  if (contains(u"passport country")) {
    return QueryIntentType::kPassportCountry;
  }
  if (contains(u"passport name")) {
    return QueryIntentType::kPassportName;
  }
  if (contains(u"passport")) {
    return QueryIntentType::kPassportFull;
  }

  // Flight Reservation
  if (contains(u"flight number")) {
    return QueryIntentType::kFlightReservationFlightNumber;
  }
  if (contains(u"ticket number")) {
    return QueryIntentType::kFlightReservationTicketNumber;
  }
  if (contains(u"confirmation code", u"flight confirmation")) {
    return QueryIntentType::kFlightReservationConfirmationCode;
  }
  if (contains(u"passenger name", u"flight passenger")) {
    return QueryIntentType::kFlightReservationPassengerName;
  }
  if (contains(u"departure airport", u"from airport")) {
    return QueryIntentType::kFlightReservationDepartureAirport;
  }
  if (contains(u"arrival airport", u"to airport")) {
    return QueryIntentType::kFlightReservationArrivalAirport;
  }
  if (contains(u"departure date", u"flight date")) {
    return QueryIntentType::kFlightReservationDepartureDate;
  }
  if (contains(u"flight reservation", u"flight", u"reservation")) {
    return QueryIntentType::kFlightReservationFull;
  }

  // Order
  if (contains(u"order id", u"order number")) {
    return QueryIntentType::kOrderId;
  }
  if (contains(u"order account")) {
    return QueryIntentType::kOrderAccount;
  }
  if (contains(u"order date")) {
    return QueryIntentType::kOrderDate;
  }
  if (contains(u"merchant name", u"store name", u"order merchant")) {
    return QueryIntentType::kOrderMerchantName;
  }
  if (contains(u"merchant domain")) {
    return QueryIntentType::kOrderMerchantDomain;
  }
  if (contains(u"product names", u"order products")) {
    return QueryIntentType::kOrderProductNames;
  }
  if (contains(u"grand total", u"order total", u"total amount")) {
    return QueryIntentType::kOrderGrandTotal;
  }
  if (contains(u"order")) {
    return QueryIntentType::kOrderFull;
  }

  // National ID Card
  if (contains(u"national id number")) {
    return QueryIntentType::kNationalIdCardNumber;
  }
  if (contains(u"national id expiration", u"national id expiry")) {
    return QueryIntentType::kNationalIdCardExpirationDate;
  }
  if (contains(u"national id issue")) {
    return QueryIntentType::kNationalIdCardIssueDate;
  }
  if (contains(u"national id country")) {
    return QueryIntentType::kNationalIdCardCountry;
  }
  if (contains(u"national id name")) {
    return QueryIntentType::kNationalIdCardName;
  }
  if (contains(u"id")) {
    return QueryIntentType::kNationalIdCardFull;
  }

  // Redress Number
  if (contains(u"redress number name", u"redress name")) {
    return QueryIntentType::kRedressNumberName;
  }
  if (contains(u"redress number")) {
    return QueryIntentType::kRedressNumberNumber;
  }
  if (contains(u"redress")) {
    return QueryIntentType::kRedressNumberFull;
  }

  // Known Traveler Number
  if (contains(u"known traveler number name", u"ktn name")) {
    return QueryIntentType::kKnownTravelerNumberName;
  }
  if (contains(u"known traveler number number", u"ktn number")) {
    return QueryIntentType::kKnownTravelerNumberNumber;
  }
  if (contains(u"known traveler number expiration", u"ktn expiration",
               u"ktn expiry")) {
    return QueryIntentType::kKnownTravelerNumberExpirationDate;
  }
  if (contains(u"known traveler number", u"traveler number", u"ktn")) {
    return QueryIntentType::kKnownTravelerNumberFull;
  }

  // Drivers License
  if (contains(u"drivers license number", u"driver's license number",
               u"driver license number")) {
    return QueryIntentType::kDriversLicenseNumber;
  }
  if (contains(u"drivers license state", u"driver's license state")) {
    return QueryIntentType::kDriversLicenseState;
  }
  if (contains(u"drivers license expiration", u"driver's license expiration",
               u"drivers license expiry", u"driver's license expiry")) {
    return QueryIntentType::kDriversLicenseExpirationDate;
  }
  if (contains(u"drivers license issue", u"driver's license issue")) {
    return QueryIntentType::kDriversLicenseIssueDate;
  }
  if (contains(u"drivers license name", u"driver's license name")) {
    return QueryIntentType::kDriversLicenseName;
  }
  if (contains(u"drivers license", u"driver's license", u"driving license",
               u"license")) {
    return QueryIntentType::kDriversLicenseFull;
  }

  // Personal profiles
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
  if (contains(u"organization", u"company")) {
    return QueryIntentType::kCompanyName;
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

  return QueryIntentType::kUnknown;
}

}  // namespace accessibility_annotator
