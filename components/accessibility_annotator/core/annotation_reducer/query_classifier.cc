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
#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/annotation_reducer_query_classifier.pb.h"
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

namespace accessibility_annotator {

namespace {

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
// The key for the intent string in the JSON returned by Gemini.
constexpr char kIntentKeyFromGemini[] = "intent";
// The key for the filter words list in the JSON returned by Gemini.
constexpr char kFilterWordsKeyFromGemini[] = "filter_words";
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

// Calls the classifiers sequentially until one of them returns a result
// different than `EntryType::kUnknown`. The `index` parameter indicates
// the current classifier to try.
void CompositeClassify(std::vector<QueryClassifier> classifiers,
                       size_t index,
                       std::u16string query,
                       bool full_search,
                       base::OnceCallback<void(ClassifiedQuery)> callback) {
  // If all classifiers were queried, return ClassifiedQuery with unknown
  // intent.
  if (index >= classifiers.size()) {
    std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
    return;
  }

  classifiers[index].Run(
      query, full_search,
      base::BindOnce(
          [](std::vector<QueryClassifier> classifiers, size_t index,
             std::u16string query, bool full_search,
             base::OnceCallback<void(ClassifiedQuery)> callback,
             ClassifiedQuery result) {
            // The first classifier that finds a result returns it.
            if (result.intent != EntryType::kUnknown) {
              std::move(callback).Run(std::move(result));
              return;
            }
            // If `EntryType::kUnknown` was returned from the
            // previous classifier, delegate the request to the next
            // classifier.
            CompositeClassify(std::move(classifiers), index + 1,
                              std::move(query), full_search,
                              std::move(callback));
          },
          std::move(classifiers), index, query, full_search,
          std::move(callback)));
}

// Classifies the query by performing substring matching against a predefined
// set of keyword phrases. Normalizes the query by lowercasing, removing
// punctuation, and stripping stop words before attempting matches. If a
// keyword phrase is found as a standalone phrase, it sets the corresponding
// intent and extracts the remaining words in the query as required words.
void KeywordQueryClassify(std::u16string_view query,
                          base::OnceCallback<void(ClassifiedQuery)> callback) {
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
  std::u16string normalized_query = base::i18n::ToLower(query);
  std::vector<std::u16string_view> all_words =
      base::SplitStringPiece(normalized_query, u" ;?,.'", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  std::erase_if(all_words, [](std::u16string_view word) {
    return kStopWords.contains(word);
  });
  normalized_query = base::JoinString(all_words, u" ");

  if (normalized_query.empty()) {
    std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
    return;
  }

  EntryType matched_intent = EntryType::kUnknown;
  std::u16string_view matched_keyword_phrase;

  // Attempts to find any of the `keyword_phrases` in `normalized_query`.
  // If a match is found, `matched_intent` and `matched_keyword_phrase`
  // are updated. Only the first successful match across all calls to
  // `try_match` is kept.
  auto try_match = [&](EntryType intent, auto... keyword_phrases) {
    if (matched_intent != EntryType::kUnknown) {
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
  try_match(EntryType::kVehicleVin, u"vin");
  try_match(EntryType::kVehicleMake, u"vehicle make", u"car make");
  try_match(EntryType::kVehicleModel, u"vehicle model", u"car model");
  try_match(EntryType::kVehicleYear, u"vehicle year", u"car year");
  try_match(EntryType::kVehicleOwner, u"vehicle owner", u"car owner");
  try_match(EntryType::kVehiclePlateState, u"license plate state",
            u"plate state");
  try_match(EntryType::kVehicle, u"vehicle", u"car");
  try_match(EntryType::kVehiclePlateNumber, u"license plate", u"plate number",
            u"plate");

  // Passport
  try_match(EntryType::kPassportNumber, u"passport number");
  try_match(EntryType::kPassportExpirationDate, u"passport expiration",
            u"passport expiry");
  try_match(EntryType::kPassportIssueDate, u"passport issue");
  try_match(EntryType::kPassportCountry, u"passport country");
  try_match(EntryType::kPassportName, u"passport name");
  try_match(EntryType::kPassportFull, u"passport");

  // Flight Reservation
  try_match(EntryType::kFlightReservationFlightNumber, u"flight number");
  try_match(EntryType::kFlightReservationTicketNumber, u"ticket number");
  try_match(EntryType::kFlightReservationConfirmationCode, u"confirmation code",
            u"flight confirmation");
  try_match(EntryType::kFlightReservationPassengerName, u"passenger name",
            u"flight passenger");
  try_match(EntryType::kFlightReservationDepartureAirport, u"departure airport",
            u"from airport");
  try_match(EntryType::kFlightReservationArrivalAirport, u"arrival airport",
            u"to airport");
  try_match(EntryType::kFlightReservationDepartureDate, u"departure date",
            u"flight date");
  try_match(EntryType::kFlightReservationArrivalDate, u"arrival date");
  try_match(EntryType::kFlightReservationFull, u"flight reservation", u"flight",
            u"reservation");

  // Shipment
  try_match(EntryType::kShipmentTrackingNumber, u"tracking number");
  try_match(EntryType::kShipmentAssociatedOrderId, u"associated order id",
            u"shipment order");
  try_match(EntryType::kShipmentDeliveryAddress, u"delivery address",
            u"shipping address");
  try_match(EntryType::kShipmentCarrierName, u"carrier name",
            u"shipping company", u"shipper name");
  try_match(EntryType::kShipmentCarrierDomain, u"carrier domain",
            u"carrier website");
  try_match(EntryType::kShipmentEstimatedDeliveryDate,
            u"estimated delivery date", u"delivery date");
  try_match(EntryType::kShipmentFull, u"shipment", u"package", u"delivery");

  // Order
  try_match(EntryType::kOrderId, u"order id", u"order number");
  try_match(EntryType::kOrderAccount, u"order account");
  try_match(EntryType::kOrderDate, u"order date");
  try_match(EntryType::kOrderMerchantName, u"merchant name", u"store name",
            u"order merchant");
  try_match(EntryType::kOrderMerchantDomain, u"merchant domain");
  try_match(EntryType::kOrderProductNames, u"product names", u"order products");
  try_match(EntryType::kOrderGrandTotal, u"grand total", u"order total",
            u"total amount");
  try_match(EntryType::kOrderFull, u"order");

  // National ID Card
  try_match(EntryType::kNationalIdCardNumber, u"national id number");
  try_match(EntryType::kNationalIdCardExpirationDate, u"national id expiration",
            u"national id expiry");
  try_match(EntryType::kNationalIdCardIssueDate, u"national id issue");
  try_match(EntryType::kNationalIdCardCountry, u"national id country");
  try_match(EntryType::kNationalIdCardName, u"national id name");
  try_match(EntryType::kNationalIdCardFull, u"national id");

  // Redress Number
  try_match(EntryType::kRedressNumberName, u"redress number name",
            u"redress name");
  try_match(EntryType::kRedressNumberNumber, u"redress number");
  try_match(EntryType::kRedressNumberFull, u"redress");

  // Known Traveler Number
  try_match(EntryType::kKnownTravelerNumberName, u"known traveler number name",
            u"ktn name");
  try_match(EntryType::kKnownTravelerNumberNumber,
            u"known traveler number number", u"ktn number");
  try_match(EntryType::kKnownTravelerNumberExpirationDate,
            u"known traveler number expiration", u"ktn expiration",
            u"ktn expiry");
  try_match(EntryType::kKnownTravelerNumberFull, u"known traveler number",
            u"traveler number", u"ktn");

  // Credit Card
  try_match(EntryType::kCreditCardExpirationDate,
            u"credit card expiration date", u"credit card expiry date",
            u"credit card expiration");
  try_match(EntryType::kCreditCardSecurityCode, u"credit card security code",
            u"card security code", u"security code", u"cvv", u"cvc");
  try_match(EntryType::kCreditCardNameOnCard, u"cardholder name", u"card name",
            u"name card");
  try_match(EntryType::kCreditCardNumber, u"credit card", u"debit card",
            u"payment method", u"credit card number", u"debit card number",
            u"card number");

  // Driver's License
  try_match(EntryType::kDriversLicenseNumber, u"drivers license number",
            u"driver's license number", u"driver license number");
  try_match(EntryType::kDriversLicenseState, u"drivers license state",
            u"driver's license state");
  try_match(EntryType::kDriversLicenseExpirationDate,
            u"drivers license expiration", u"driver's license expiration",
            u"drivers license expiry", u"driver's license expiry");
  try_match(EntryType::kDriversLicenseIssueDate, u"drivers license issue",
            u"driver's license issue");
  try_match(EntryType::kDriversLicenseName, u"drivers license name",
            u"driver's license name");
  try_match(EntryType::kDriversLicenseFull, u"drivers license",
            u"driver's license", u"driving license", u"license");

  // Personal profiles
  try_match(EntryType::kAddressZip, u"zip code", u"zip-code", u"zip",
            u"postal code", u"postal-code", u"postal");
  try_match(EntryType::kAddressCity, u"city", u"town");
  try_match(EntryType::kAddressState, u"state", u"province");
  try_match(EntryType::kAddressCountry, u"country");
  try_match(EntryType::kAddressStreetAddress, u"street");
  try_match(EntryType::kPhone, u"phone", u"mobile", u"telephone");
  try_match(EntryType::kEmail, u"e-mail", u"email");
  try_match(EntryType::kCompanyName, u"organization", u"company");
  try_match(EntryType::kNameFull, u"name");
  try_match(EntryType::kAddressFull, u"home address", u"work address",
            u"address", u"home", u"work", u"live");
  try_match(EntryType::kIban, u"iban", u"bank account");

  if (matched_intent == EntryType::kUnknown) {
    std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
    return;
  }

  // Remove the matched keyword phrase from the query and extract the remaining
  // words.
  base::ReplaceFirstSubstringAfterOffset(&normalized_query, 0,
                                         matched_keyword_phrase, u"");
  std::vector<std::u16string> filter_words = base::SplitString(
      normalized_query, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::move(callback).Run(
      ClassifiedQuery(matched_intent, std::move(filter_words)));
}

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
// Handles the completion of the Gemini model execution. Sanitizes the
// response string (stripping Markdown if present), parses the JSON
// classification result, and invokes the `callback` with the identified
// intent and filter words.
void OnGeminiClassificationComplete(
    base::OnceCallback<void(ClassifiedQuery)> callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  auto callback_with_unknown_type = [&]() {
    std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
  };

  if (!result.response.has_value()) {
    callback_with_unknown_type();
    return;
  }

  const optimization_guide::proto::Any& response_any = result.response.value();
  std::optional<
      optimization_guide::proto::AnnotationReducerQueryClassifierResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::AnnotationReducerQueryClassifierResponse>(
          response_any);

  if (!response || response->classification().empty()) {
    callback_with_unknown_type();
    return;
  }

  std::string_view response_string =
      StripMarkdownCodeBlocks(response->classification());

  // Parse the sanitized model response string into JSON.
  // The expected format is {"intent": "...", "filter_words": ["...", "..."]}
  std::optional<base::Value> value =
      base::JSONReader::Read(response_string, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    callback_with_unknown_type();
    return;
  }

  const base::DictValue* dict = value->GetIfDict();
  if (!dict) {
    callback_with_unknown_type();
    return;
  }

  const std::string* intent_str = dict->FindString(kIntentKeyFromGemini);
  if (!intent_str) {
    callback_with_unknown_type();
    return;
  }

  EntryType intent = StringToEntryType(*intent_str);
  std::vector<std::u16string> filter_words;

  if (const base::ListValue* filter_words_list =
          dict->FindList(kFilterWordsKeyFromGemini)) {
    for (const auto& filter_word : *filter_words_list) {
      if (filter_word.is_string()) {
        filter_words.push_back(
            base::i18n::ToLower(base::UTF8ToUTF16(filter_word.GetString())));
      }
    }
  }

  std::move(callback).Run(ClassifiedQuery(intent, std::move(filter_words)));
}

// Initiates an asynchronous classification of the `query` using the Gemini
// model via the provided `remote_model_executor`.
void GeminiClassify(
    optimization_guide::RemoteModelExecutor* remote_model_executor,
    std::u16string_view query,
    base::OnceCallback<void(ClassifiedQuery)> callback) {
  if (!remote_model_executor) {
    std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
    return;
  }

  optimization_guide::proto::AnnotationReducerQueryClassifierRequest request;
  request.set_query(base::UTF16ToUTF8(query));

  remote_model_executor->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::
          kAnnotationReducerQueryClassifier,
      request, optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&OnGeminiClassificationComplete, std::move(callback)));
}
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

}  // namespace

ClassifiedQuery::ClassifiedQuery(EntryType intent,
                                 std::vector<std::u16string> filter_words)
    : intent(intent), filter_words(std::move(filter_words)) {}

ClassifiedQuery::ClassifiedQuery(const ClassifiedQuery&) = default;
ClassifiedQuery& ClassifiedQuery::operator=(const ClassifiedQuery&) = default;
ClassifiedQuery::ClassifiedQuery(ClassifiedQuery&&) = default;
ClassifiedQuery& ClassifiedQuery::operator=(ClassifiedQuery&&) = default;
ClassifiedQuery::~ClassifiedQuery() = default;

QueryClassifier CreateQueryClassifier(
    optimization_guide::RemoteModelExecutor* remote_model_executor) {
  std::vector<QueryClassifier> classifiers = {
      internal::CreateKeywordQueryClassifier(),
#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
      internal::CreateGeminiClassifier(remote_model_executor)
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
  };
  return base::BindRepeating(&CompositeClassify, std::move(classifiers), 0);
}

namespace internal {

// Returns true if the `needle` is found in the `haystack` as a standalone
// phrase. A phrase is considered standalone if it is bounded by
// non-alphanumeric characters (like spaces or punctuation) or the start/end
// of the `haystack`.
bool ContainsStandalonePhrase(std::u16string_view haystack,
                              std::u16string_view needle) {
  if (needle.empty()) {
    return true;
  }

  size_t pos = haystack.find(needle);
  while (pos != std::u16string::npos) {
    // A word starts if it's at the beginning of the string or preceded by a
    // non-alphanumeric character.
    bool start_of_word =
        pos == 0 || !base::IsAsciiAlphaNumeric(haystack[pos - 1]);
    // A word ends if it's at the end of the string or followed by a
    // non-alphanumeric character.
    bool end_of_word =
        pos + needle.length() == haystack.length() ||
        !base::IsAsciiAlphaNumeric(haystack[pos + needle.length()]);
    if (start_of_word && end_of_word) {
      return true;
    }
    pos = haystack.find(needle, pos + 1);
  }
  return false;
}

QueryClassifier CreateKeywordQueryClassifier() {
  return base::BindRepeating(
      [](std::u16string query, bool full_search,
         base::OnceCallback<void(ClassifiedQuery)> callback) {
        KeywordQueryClassify(query, std::move(callback));
      });
}

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
QueryClassifier CreateGeminiClassifier(
    optimization_guide::RemoteModelExecutor* remote_model_executor) {
  return base::BindRepeating(
      [](optimization_guide::RemoteModelExecutor* remote_model_executor,
         std::u16string query, bool full_search,
         base::OnceCallback<void(ClassifiedQuery)> callback) {
        if (!full_search) {
          std::move(callback).Run(ClassifiedQuery(EntryType::kUnknown));
          return;
        }
        GeminiClassify(remote_model_executor, query, std::move(callback));
      },
      remote_model_executor);
}
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

}  // namespace internal

}  // namespace accessibility_annotator
