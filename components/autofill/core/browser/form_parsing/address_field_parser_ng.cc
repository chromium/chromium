// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field_parser_ng.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_util.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

// Specify `--vmodule=address_field_parser_ng=1` to get insights into the
// classification process. It will produce output describing the recursive
// exploration of field type assignments.

// Adds a FormControlType to MatchParams.
MatchParams MatchParamsWithFieldType(MatchParams p,
                                     FormControlType field_type) {
  p.field_types.insert(field_type);
  return p;
}

// Removes a MatchAttribute from MatchParams.
MatchParams MatchParamsWithoutAttribute(MatchParams p,
                                        MatchAttribute attribute) {
  p.attributes.erase(attribute);
  return p;
}

std::string SequenceToScoreString(const ClassifiedFieldSequence& sequence) {
  return base::NumberToString(sequence.contained_types.size()) + "/" +
         base::NumberToString(sequence.score);
}

}  // namespace

ClassifiedFieldSequence::ClassifiedFieldSequence() = default;
ClassifiedFieldSequence::~ClassifiedFieldSequence() = default;

bool ClassifiedFieldSequence::BetterThan(
    const ClassifiedFieldSequence& other) const {
  if (contained_types.size() != other.contained_types.size()) {
    return contained_types.size() > other.contained_types.size();
  }
  return score > other.score;
}

// TODO(crbug.com/328954153): This initialization of prepared work could be
// cached in a registry to prevent the repetitive creation effort.
AddressFieldParserNG::AddressFieldParserNG(AddressCountryCode client_country)
    : constraints_(
          std::make_unique<AddressHierarchyConstraints>(client_country)) {}

AddressFieldParserNG::~AddressFieldParserNG() = default;

// static
std::unique_ptr<FormFieldParser> AddressFieldParserNG::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  if (scanner.IsEnd()) {
    return nullptr;
  }

  const AutofillScanner::Position saved_cursor = scanner.GetPosition();
  std::unique_ptr<AddressFieldParserNG> address_field(new AddressFieldParserNG(
      AddressCountryCode(context.client_country.value())));
  address_field->context_ = &context;
  address_field->scanner_ = &scanner;
  address_field->initial_field_ = &scanner.Cursor();

  DVLOG(1) << "Parse recursively starting at " << scanner.GetOffset() << " "
           << scanner.Cursor().label();

  address_field->ParseRecursively();

  // These members are used during the parse run and should be cleared because
  // we cannot make any life-cycle assumptions on them beyond the call of Parse.
  address_field->context_ = nullptr;
  address_field->scanner_ = nullptr;
  address_field->initial_field_ = nullptr;

  // As per the contract of parse functions: If a viable classification was
  // found, set the cursor to the last classified field + 1, otherwise return
  // the scanner in the initial state.
  if (!address_field->best_classification_.assignments.empty()) {
    scanner.Restore(
        *address_field->best_classification_.last_classified_field_index);
    scanner.Advance();
    return address_field;
  }
  scanner.Restore(saved_cursor);
  return nullptr;
}

void AddressFieldParserNG::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  for (auto [field_type, field_ptr] : best_classification_.assignments) {
    if (!field_ptr) {
      continue;
    }
    // TODO(crbug.com/320965828): Support MatchInfo. The NG parser doesn't track
    // how matches are found. `kHighQualityLabel` is merely a placeholder.
    AddClassification(
        FieldAndMatchInfo(field_ptr,
                          {.matched_attribute =
                               MatchInfo::MatchAttribute::kHighQualityLabel}),
        field_type, HeuristicParser::kAddress, field_candidates);
  }
}

std::optional<double> AddressFieldParserNG::FindScoreOfBestMatchingRule(
    FieldType field_type) {
  // Naming convention: In the following code,
  //   auto r =
  // is a short cut for
  //   std::optional<double> result =
  // is used consistently to keep the code readable.

  // Give the label priority over the name to avoid misclassifications when the
  // name has a misleading value (e.g. in MX the input field for
  // "Municipio/Delegación" is sometimes named "city" even though that should be
  // mapped to a "Ciudad").
  bool prefer_label = context_->client_country == GeoIpCountryCode("MX");

  auto MatchOnlyLabel = [](const MatchParams& p) {
    return MatchParamsWithoutAttribute(p, MatchAttribute::kName);
  };
  auto MatchOnlyName = [](const MatchParams& p) {
    return MatchParamsWithoutAttribute(p, MatchAttribute::kLabel);
  };
  // Returns `score` if the regex pattern identified by `pattern_name` matches
  // against the label or name of a field. In some countries we prefer matches
  // to labels over matches to field names; in other countries we prefer matches
  // to field names. If a match happens on the preferred attribute, the score is
  // boosted by 0.05.
  auto Match = [&](std::string_view pattern_name, double score,
                   MatchParams (*match_pattern_projection)(const MatchParams&) =
                       nullptr) -> std::optional<double> {
    // Helper function to consecutively match the regex against the label and
    // the name attribute in the desired order and adding a boost in case the
    // preferred attribute match.
    auto MatchAttribute = [&](bool match_label) -> std::optional<double> {
      if (FieldMatchesMatchPatternRef(
              *context_, scanner_->Cursor(), pattern_name,
              {match_label ? MatchOnlyLabel : MatchOnlyName,
               match_pattern_projection})) {
        return score + (match_label == prefer_label ? 0.05 : 0.0);
      }
      return std::nullopt;
    };
    if (prefer_label) {
      auto r = MatchAttribute(/*match_label=*/true);
      return r ? r : MatchAttribute(/*match_label=*/false);
    } else {
      auto r = MatchAttribute(/*match_label=*/false);
      return r ? r : MatchAttribute(/*match_label=*/true);
    }
  };

  // TOOD(crbug.com/328954153) Consider whether it makes sense to pull the
  // country specific rules out of this big switch statement.
  switch (field_type) {
    case UNKNOWN_TYPE:
      // The following are field types that may occur interspersed in an
      // address form but matches are ignored. Email fields are reported by a
      // different FormFieldParser. The other fields are just ignored.
      for (const char* type : {"ADDRESS_LOOKUP", "ADDRESS_NAME_IGNORED",
                               "EMAIL_ADDRESS", "ATTENTION_IGNORED"}) {
        if (Match(type, 10.0)) {
          return 10;
        }
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_ADDRESS:
      // The score is a bit higher than the score of an address line 1.
      // This ensures that
      // score(ADDRESS_HOME_STREET_ADDRESS) > score(ADDRESS_HOME_LINE1)
      // but
      // score(ADDRESS_HOME_STREET_ADDRESS) < score(ADDRESS_HOME_LINE1) +
      //                                      score(ADDRESS_HOME_LINE2)
      return Match("ADDRESS_LINE_1", 1.6, [](const MatchParams& p) {
        return MatchParamsWithFieldType(p, FormControlType::kTextArea);
      });
    case ADDRESS_HOME_LINE1:
      return Match("ADDRESS_LINE_1", 1.0);
    case ADDRESS_HOME_LINE2:
      // Address lines 2 can follow address lines 1 - and, in the legacy model,
      // a street name and house number. For countries explicitly modeled for
      // the i18n address hierarchy, `incompatible_` will suppress a combination
      // of street name/house number and address line 2.
      if (partial_classification_.contained_types.contains(
              ADDRESS_HOME_LINE1) ||
          partial_classification_.contained_types.contains_all(
              {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER})) {
        // If the country model does not contain support for an apartment
        // number, we treat a match for the apartment number regex as an
        // address line 2.
        if (!constraints_->supported_field_types().contains(
                ADDRESS_HOME_APT_NUM)) {
          if (auto r = Match("ADDRESS_HOME_APT_NUM", 1.0)) {
            return r;
          }
        }
        return Match("ADDRESS_LINE_2", 1.0);
      }
      return std::nullopt;
    case ADDRESS_HOME_LINE3:
      // An address line 3 can only directly follow an address line 2.
      if (partial_classification_.contained_types.contains_all(
              {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2}) &&
          partial_classification_.assignments[ADDRESS_HOME_LINE2] ==
              scanner_->Predecessor()) {
        if (auto r = Match("ADDRESS_LINE_2", 1.0)) {
          return r;
        }
        return Match("ADDRESS_LINE_EXTRA", 1.0);
      }
      return std::nullopt;
    case ADDRESS_HOME_APT_NUM:
      return Match("ADDRESS_HOME_APT_NUM", 1.0);
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_TYPE:
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      // ADDRESS_HOME_APT, ADDRESS_HOME_APT_TYPE and
      // ADDRESS_HOME_HOUSE_NUMBER_AND_APT are currently internal nodes of the
      // address hierarchy that only exist to parse and format an address. They
      // don't exist as recognized field types.
      return std::nullopt;
    case ADDRESS_HOME_CITY:
      return Match("CITY", 1.0);
    case ADDRESS_HOME_STATE:
      return Match("STATE", 1.0);
    case ADDRESS_HOME_ZIP:
      return Match("ZIP_CODE", 1.0);
      // TODO(crbug.com/328954153): ZIP4
    case ADDRESS_HOME_COUNTRY:
      // A bit >1.0 to prefer country over state in "country/region"
      if (auto r = Match("COUNTRY", 1.1)) {
        return r;
      }
      // The occasional page (e.g. google account registration page) calls
      // this a "location". However, this only makes sense for select tags, so
      // a different PatternRef is used.
      return Match("COUNTRY_LOCATION", 1.1);
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      // In India a special regex is used for the locality (dependent locality).
      if (context_->client_country == GeoIpCountryCode("IN")) {
        return Match("IN_DEPENDENT_LOCALITY", 1.0);
      }
      return Match("ADDRESS_HOME_DEPENDENT_LOCALITY", 1.0);
    case ADDRESS_HOME_STREET_NAME:
      // A bit >1.0 to prefer a street name over address line 1.
      return Match("ADDRESS_HOME_STREET_NAME", 1.1);
    case ADDRESS_HOME_HOUSE_NUMBER:
      return Match("ADDRESS_HOME_HOUSE_NUMBER", 1.1);
    case ADDRESS_HOME_STREET_LOCATION:
      // In India a special regex is used for the street location.
      if (context_->client_country == GeoIpCountryCode("IN")) {
        return Match("IN_STREET_LOCATION", 1.0);
      }
      // In most countries, street location is a combination of multiple
      // fields. Therefore, the score is higher than the score of each compound.
      return Match("ADDRESS_HOME_STREET_LOCATION", 1.5);
    case ADDRESS_HOME_LANDMARK:
      return Match("LANDMARK", 1.0);
    case ADDRESS_HOME_BETWEEN_STREETS:
      return Match("BETWEEN_STREETS", 1.5);
    case ADDRESS_HOME_BETWEEN_STREETS_1:
      // These are scored a big higher than ADDRESS_HOME_STREET_NAME to give
      // priority to ADDRESS_HOME_BETWEEN_STREETS_1/2. This is fine because the
      // regex is more specific than the regex for ADDRESS_HOME_STREET_NAME.
      return Match("BETWEEN_STREETS_LINE_1", 1.2);
    case ADDRESS_HOME_BETWEEN_STREETS_2:
      if (partial_classification_.contained_types.contains(
              ADDRESS_HOME_BETWEEN_STREETS_1)) {
        return Match("BETWEEN_STREETS_LINE_2", 1.2);
      }
      return std::nullopt;
    case ADDRESS_HOME_ADMIN_LEVEL2:
      // The score is a bit higher than city for MX because the term
      // "Municipio/Delegación" should take precedence.
      return Match("ADMIN_LEVEL_2", 1.1);
    case ADDRESS_HOME_OVERFLOW:
      return Match("OVERFLOW", 1.0);
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      // Higher score because the field needs to contain hints for both
      // between streets and landmark.
      return Match("BETWEEN_STREETS_OR_LANDMARK", 1.7);
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      // Higher score because the field needs to contain hints for both
      // overflow and landmark.
      return Match("OVERFLOW_AND_LANDMARK", 1.7);
    case COMPANY_NAME:
      // A bit less than 1.0 to prioritize an address line 2 interpretation.
      // score(street address) + score(company name) <
      //   score(address line 1) + score(address line 2)
      if (!Match("ADDRESS_LINE_1", 1.0) && !Match("ADDRESS_LINE_2", 1.0) &&
          !Match("ADDRESS_HOME_APT_NUM", 1.0)) {
        return Match("COMPANY_NAME", 0.8);
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_STREET_LOCATION", 1.0) &&
          Match("IN_DEPENDENT_LOCALITY", 1.0)) {
        return 1.5;
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_STREET_LOCATION", 1.0) && Match("LANDMARK", 1.0)) {
        return 1.5;
      }
      // Some Location and landmark fields are labeled "Address". We give it
      // a 0.8 score to prefer a classification as a STREET_ADDRESS or
      // ADDRESS_LINE_1, but allow a higher score when combined with a
      // landmark.
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("ADDRESS_LINE_1", 1.0)) {
        return 0.8;
      }
      return std::nullopt;
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_DEPENDENT_LOCALITY", 1.0) && Match("LANDMARK", 1.0)) {
        return 1.5;
      }
      return std::nullopt;
    case ADDRESS_HOME_ZIP_AND_CITY:
      if (context_->client_country == GeoIpCountryCode("FR") &&
          Match("ZIP_CODE", 1.0) && Match("CITY", 1.0)) {
        return 1.5;
      }
      return std::nullopt;

    // Address related fields that we don't parse (yet).
    case DELIVERY_INSTRUCTIONS:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_SORTING_CODE:
      return std::nullopt;

    // Fields that are not processed by the AddressFieldParserNG.
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case ALTERNATIVE_FULL_NAME:
    case ALTERNATIVE_GIVEN_NAME:
    case ALTERNATIVE_FAMILY_NAME:
    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case IBAN_VALUE:
    case MERCHANT_PROMO_CODE:
    case USERNAME:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case SINGLE_USERNAME:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case NOT_PASSWORD:
    case NOT_USERNAME:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case ONE_TIME_CODE:
    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case MERCHANT_EMAIL_SIGNUP:
    case PRICE:
    case NUMERIC_QUANTITY:
    case SEARCH_TERM:
    case PASSPORT_NUMBER:
    case PASSPORT_ISSUING_COUNTRY:
    case PASSPORT_EXPIRATION_DATE:
    case PASSPORT_ISSUE_DATE:
    case LOYALTY_MEMBERSHIP_PROGRAM:
    case LOYALTY_MEMBERSHIP_PROVIDER:
    case LOYALTY_MEMBERSHIP_ID:
    case VEHICLE_LICENSE_PLATE:
    case VEHICLE_VIN:
    case VEHICLE_MAKE:
    case VEHICLE_MODEL:
    case VEHICLE_YEAR:
    case VEHICLE_PLATE_STATE:
    case DRIVERS_LICENSE_REGION:
    case DRIVERS_LICENSE_NUMBER:
    case DRIVERS_LICENSE_EXPIRATION_DATE:
    case DRIVERS_LICENSE_ISSUE_DATE:
    case EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
    case NATIONAL_ID_CARD_NUMBER:
    case NATIONAL_ID_CARD_EXPIRATION_DATE:
    case NATIONAL_ID_CARD_ISSUE_DATE:
    case NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case REDRESS_NUMBER:
    case KNOWN_TRAVELER_NUMBER:
    case KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case ADDRESS_HOME_ZIP_PREFIX:
    case ADDRESS_HOME_ZIP_SUFFIX:
    case FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case FLIGHT_RESERVATION_TICKET_NUMBER:
    case FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
    case FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
    case FLIGHT_RESERVATION_DEPARTURE_DATE:
    case ORDER_ID:
    case ORDER_DATE:
    case ORDER_MERCHANT_NAME:
    case SHIPMENT_TRACKING_NUMBER:
    case MAX_VALID_FIELD_TYPE:
      return std::nullopt;
  }
}

void AddressFieldParserNG::ParseRecursively() {
  auto log_prefix = [&]() { return std::string(scanner_->GetOffset(), ' '); };
  if (scanner_->IsEnd()) {
    DVLOG(1) << log_prefix() << "END of input";
    DVLOG(1) << log_prefix()
             << "score=" << SequenceToScoreString(partial_classification_)
             << ", best_score_so_far="
             << SequenceToScoreString(best_classification_)
             << ", plausible=" << IsClassificationPlausible();
    // Store classification if it's better.
    if (partial_classification_.BetterThan(best_classification_) &&
        IsClassificationPlausible()) {
      DVLOG(1) << log_prefix() << "NEW BEST SOLUTION";
      best_classification_ = partial_classification_;
    }
    return;
  }

  // UNKNOWN_TYPE should always be the first element. If we have a match,
  // we skip all other field types.
  CHECK_EQ(*constraints_->supported_field_types().begin(), UNKNOWN_TYPE);

  // Whether any field type could be assigned for the current scanner position.
  bool found_extra_assignment = false;
  for (FieldType field_type : constraints_->supported_field_types()) {
    // Skip trying field_type if it's incompatible with already assigned types.
    if (partial_classification_.contained_types.contains_any(
            constraints_->incompatible_field_types(field_type))) {
      DVLOG(1) << log_prefix() << "---- " << FieldTypeToStringView(field_type)
               << " conflict.";
      continue;
    }

    std::optional<double> extra_score = FindScoreOfBestMatchingRule(field_type);
    if (!extra_score) {
      DVLOG(1) << log_prefix() << "---- " << FieldTypeToStringView(field_type)
               << " non-match.";
      continue;
    }

    found_extra_assignment = true;

    // Perform new assignment.
    const double old_score = partial_classification_.score;
    const std::optional<AutofillScanner::Position>
        old_last_classified_field_index =
            partial_classification_.last_classified_field_index;
    if (field_type != UNKNOWN_TYPE) {
      partial_classification_.contained_types.insert(field_type);
      partial_classification_.assignments[field_type] = &scanner_->Cursor();
      partial_classification_.last_classified_field_index =
          scanner_->GetPosition();
    }
    partial_classification_.score += *extra_score;

    DVLOG(1) << log_prefix() << "++++ " << FieldTypeToStringView(field_type)
             << " match. new score is " << partial_classification_.score;

    const AutofillScanner::Position old_position = scanner_->GetPosition();
    scanner_->Advance();
    ParseRecursively();
    scanner_->Restore(old_position);

    // Revert new assignment.
    if (field_type != UNKNOWN_TYPE) {
      partial_classification_.contained_types.erase(field_type);
      partial_classification_.assignments.erase(field_type);
      partial_classification_.last_classified_field_index =
          old_last_classified_field_index;
    }
    partial_classification_.score = old_score;

    // If we had a match on UNKNOWN_TYPE (i.e. an email field, address lookup
    // field, etc.), we don't want to try other field types. E.g. "address" is
    // a substring of "email address" and should not be considered.
    if (field_type == UNKNOWN_TYPE && extra_score) {
      break;
    }
  }
  if (!found_extra_assignment) {
    DVLOG(1) << log_prefix() << "END did not find another classification.";
    DVLOG(1) << log_prefix()
             << "score=" << SequenceToScoreString(partial_classification_)
             << ", best_score_so_far="
             << SequenceToScoreString(best_classification_)
             << ", plausible=" << IsClassificationPlausible();
    if (partial_classification_.BetterThan(best_classification_) &&
        IsClassificationPlausible()) {
      DVLOG(1) << log_prefix() << "NEW BEST SOLUTION";
      best_classification_ = partial_classification_;
    }
  }
}

bool AddressFieldParserNG::IsClassificationPlausible() const {
  // The house number is easy to guess wrong (e.g. to mix up with a CC number).
  // Therefore, we require extra evidence.
  const FieldTypeSet& contained_types = partial_classification_.contained_types;
  if (contained_types.contains(ADDRESS_HOME_HOUSE_NUMBER) &&
      !contained_types.contains_any(
          {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_OVERFLOW,
           ADDRESS_HOME_LANDMARK, ADDRESS_HOME_OVERFLOW_AND_LANDMARK})) {
    return false;
  }
  if (contained_types.contains(ADDRESS_HOME_APT_NUM) &&
      !contained_types.contains_all(
          {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER})) {
    return false;
  }
  return true;
}

}  // namespace autofill
