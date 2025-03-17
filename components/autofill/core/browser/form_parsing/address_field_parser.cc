// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field_parser.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

namespace {

// Removes a MatchAttribute from MatchParams.
MatchParams WithoutAttribute(MatchParams p, MatchAttribute attribute) {
  p.attributes.erase(attribute);
  return p;
}

// Adds a FormControlType to MatchParams.
MatchParams WithFieldType(MatchParams p, FormControlType field_type) {
  p.field_types.insert(field_type);
  return p;
}

}  // namespace

// static
std::unique_ptr<FormFieldParser> AddressFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return nullptr;
  }

  std::unique_ptr<AddressFieldParser> address_field(new AddressFieldParser());
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    // Ignore "Address Lookup" field. http://crbug.com/427622
    if (ParseField(context, scanner, "ADDRESS_LOOKUP") ||
        ParseField(context, scanner, "ADDRESS_NAME_IGNORED")) {
      continue;
      // Ignore email addresses.
    } else if (ParseField(context, scanner, "EMAIL_ADDRESS", nullptr,
                          [](const MatchParams& p) {
                            return WithFieldType(p, FormControlType::kTextArea);
                          })) {
      continue;
    } else if (address_field->ParseAddress(context, scanner) ||
               address_field->ParseAddressField(context, scanner) ||
               address_field->ParseCompany(context, scanner)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(context, scanner, "ATTENTION_IGNORED") ||
               ParseField(context, scanner, "REGION_IGNORED")) {
      // We ignore the following:
      // * Attention.
      // * Province/Region/Other.
      continue;
    } else if (scanner->Cursor() != initial_field &&
               ParseEmptyLabel(context, scanner, nullptr)) {
      // Ignore non-labeled fields within an address; the page
      // MapQuest Driving Directions North America.html contains such a field.
      // We only ignore such fields after we've parsed at least one other field;
      // otherwise we'd effectively parse address fields before other field
      // types after any non-labeled fields, and we want email address fields to
      // have precedence since some pages contain fields labeled
      // "Email address".
      if (!has_trailing_non_labeled_fields) {
        has_trailing_non_labeled_fields = true;
        begin_trailing_non_labeled_fields = cursor;
      }

      continue;
    } else {
      // No field found.
      break;
    }
  }

  // If we have identified any address fields in this field then it should be
  // added to the list of fields.
  if (address_field->company_ || address_field->address1_ ||
      address_field->address2_ || address_field->address3_ ||
      address_field->street_address_ || address_field->city_ ||
      address_field->state_ || address_field->zip_ || address_field->zip4_ ||
      address_field->street_name_ || address_field->house_number_ ||
      address_field->country_ || address_field->apartment_number_ ||
      address_field->dependent_locality_ || address_field->landmark_ ||
      address_field->between_streets_ ||
      address_field->between_streets_line_1_ ||
      address_field->between_streets_line_2_ || address_field->admin_level2_ ||
      address_field->between_streets_or_landmark_ ||
      address_field->overflow_and_landmark_ || address_field->overflow_ ||
      address_field->street_location_ || address_field->house_number_and_apt_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

// static
bool AddressFieldParser::IsStandaloneZipSupported(
    const GeoIpCountryCode& client_country) {
  return client_country == GeoIpCountryCode("BR") ||
         client_country == GeoIpCountryCode("MX");
}

// static
std::unique_ptr<FormFieldParser> AddressFieldParser::ParseStandaloneZip(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return nullptr;
  }

  std::unique_ptr<AddressFieldParser> address_field(new AddressFieldParser());
  size_t saved_cursor = scanner->SaveCursor();

  address_field->ParseZipCode(context, scanner);
  if (address_field->zip_) {
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

AddressFieldParser::AddressFieldParser() = default;
AddressFieldParser::~AddressFieldParser() = default;

void AddressFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  // The page can request the address lines as a single textarea input or as
  // multiple text fields (or not at all), but it shouldn't be possible to
  // request both.
  DCHECK(!(address1_ && street_address_));
  DCHECK(!(address2_ && street_address_));
  DCHECK(!(address3_ && street_address_));

  AddClassification(company_, COMPANY_NAME, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address1_, ADDRESS_HOME_LINE1, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address2_, ADDRESS_HOME_LINE2, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address3_, ADDRESS_HOME_LINE3, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(street_location_, ADDRESS_HOME_STREET_LOCATION,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(street_address_, ADDRESS_HOME_STREET_ADDRESS,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(dependent_locality_, ADDRESS_HOME_DEPENDENT_LOCALITY,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(city_, ADDRESS_HOME_CITY, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(state_, ADDRESS_HOME_STATE, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(zip_, ADDRESS_HOME_ZIP, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(country_, ADDRESS_HOME_COUNTRY, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(house_number_, ADDRESS_HOME_HOUSE_NUMBER,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(street_name_, ADDRESS_HOME_STREET_NAME,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(apartment_number_, ADDRESS_HOME_APT_NUM,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(landmark_, ADDRESS_HOME_LANDMARK, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(between_streets_, ADDRESS_HOME_BETWEEN_STREETS,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(between_streets_line_1_, ADDRESS_HOME_BETWEEN_STREETS_1,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(between_streets_line_2_, ADDRESS_HOME_BETWEEN_STREETS_2,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(admin_level2_, ADDRESS_HOME_ADMIN_LEVEL2,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(between_streets_or_landmark_,
                    ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(overflow_and_landmark_, ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(overflow_, ADDRESS_HOME_OVERFLOW, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(house_number_and_apt_, ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
                    kBaseAddressParserScore, field_candidates);
}

bool AddressFieldParser::ParseCompany(ParsingContext& context,
                                      AutofillScanner* scanner) {
  if (company_)
    return false;

  return ParseField(context, scanner, "COMPANY_NAME", &company_);
}

bool AddressFieldParser::ParseAddressFieldSequence(ParsingContext& context,
                                                   AutofillScanner* scanner) {
  // Search for an uninterrupted sequence of fields that indicate that a form
  // asks for structured information.
  //
  // We look for fields that are part of a street address and are therefore
  // conflicting with address address lines 1, 2, 3.
  //
  // Common examples are:
  // - house number + street name
  // - house number + zip code (in some countries the zip code and house number
  //     fully identify a building)
  // - house number + overflow (in some countries the zip code is requested
  //     first and a followup form only asks for overflow + house number because
  //     together with the zip code a building is fully identified)
  // - house number + overflow and landmark
  //
  // Another case is the existence of fields that cannot be reliably mapped to
  // a specific address line:
  // - street location (could be address line1) + between streets
  // - street location (could be address line1) + apartment [*]
  // - street location (could be address line1) + landmark (not implemented,
  //   yet)
  // [*] note that street location + apartment is a common pattern in the US but
  // the sequence of fields is so frequent and not paired with other
  // fine-grained fields that we want to treat it as address lines 1 and 2. We
  // enforce this by not enabling street location in the US.
  //
  // Only if a house number and one of the extra fields are found in an
  // arbitrary order the parsing is considered successful.
  const size_t saved_cursor_position = scanner->CursorPosition();

  std::optional<FieldAndMatchInfo> old_street_location = street_location_;
  std::optional<FieldAndMatchInfo> old_street_name = street_name_;
  std::optional<FieldAndMatchInfo> old_overflow = overflow_;
  std::optional<FieldAndMatchInfo> old_between_streets_or_landmark =
      between_streets_or_landmark_;
  std::optional<FieldAndMatchInfo> old_overflow_and_landmark =
      overflow_and_landmark_;
  std::optional<FieldAndMatchInfo> old_between_streets = between_streets_;
  std::optional<FieldAndMatchInfo> old_between_streets_line_1 =
      between_streets_line_1_;
  std::optional<FieldAndMatchInfo> old_between_streets_line_2 =
      between_streets_line_2_;
  std::optional<FieldAndMatchInfo> old_house_number = house_number_;
  std::optional<FieldAndMatchInfo> old_zip = zip_;
  std::optional<FieldAndMatchInfo> old_zip4 = zip4_;
  std::optional<FieldAndMatchInfo> old_apartment_number = apartment_number_;
  std::optional<FieldAndMatchInfo> old_house_number_and_apt_ =
      house_number_and_apt_;
  std::optional<FieldAndMatchInfo> old_dependent_locality = dependent_locality_;
  std::optional<FieldAndMatchInfo> old_landmark = landmark_;

  AddressCountryCode country_code(context.client_country.value());

  while (!scanner->IsEnd()) {
    // We look for street location before street name, because the name/label of
    // a street location typically contains strings that match the regular
    // expressions for a street name as well.
    if (ParseStreetLocation(context, scanner)) {
      continue;
    }

    if (ParseHouseNumAptNumStreetNameSequence(context, scanner)) {
      continue;
    }

    if (!street_location_ && ParseStreetName(context, scanner)) {
      continue;
    }

    if (ParseZipCode(context, scanner)) {
      continue;
    }

    if (!between_streets_ && !between_streets_line_1_ &&
        !between_streets_line_2_ &&
        ParseBetweenStreetsOrLandmark(context, scanner)) {
      continue;
    }

    if (!overflow_ && ParseOverflowAndLandmark(context, scanner)) {
      continue;
    }

    // Because `overflow_and_landmark_` and `overflow_` overflow in semantics
    // we don't want them both to be in the same form section. This would
    // probably point to some problem in the classification.
    if (!overflow_and_landmark_ && ParseOverflow(context, scanner)) {
      continue;
    }

    if (ParseFieldSpecificsForHouseNumberAndApt(context, scanner)) {
      continue;
    }

    if (!street_location_ && ParseHouseNumber(context, scanner)) {
      continue;
    }

    if (ParseApartmentNumber(context, scanner)) {
      continue;
    }

    if (ParseBetweenStreetsFields(context, scanner)) {
      continue;
    }

    if (ParseDependentLocality(context, scanner)) {
      continue;
    }

    if (ParseLandmark(context, scanner)) {
      continue;
    }

    break;
  }

  // This is a safety mechanism: If no field was classified, we do not want to
  // return true, because the caller assumes that the cursor position moved.
  // Otherwise, we could end up in an infinite loop.
  if (scanner->CursorPosition() == saved_cursor_position) {
    return false;
  }

  if (PossiblyAStructuredAddressForm(context.client_country)) {
    return true;
  }

  // Reset all fields if the non-optional requirements could not be met.
  street_location_ = old_street_location;
  street_name_ = old_street_name;
  house_number_ = old_house_number;
  overflow_ = old_overflow;
  between_streets_or_landmark_ = old_between_streets_or_landmark;
  overflow_and_landmark_ = old_overflow_and_landmark;
  between_streets_ = old_between_streets;
  between_streets_line_1_ = old_between_streets_line_1;
  between_streets_line_2_ = old_between_streets_line_2;
  zip_ = old_zip;
  zip4_ = old_zip4;
  apartment_number_ = old_apartment_number;
  house_number_and_apt_ = old_house_number_and_apt_;
  dependent_locality_ = old_dependent_locality;
  landmark_ = old_landmark;

  scanner->RewindTo(saved_cursor_position);
  return false;
}

bool AddressFieldParser::ParseAddress(ParsingContext& context,
                                      AutofillScanner* scanner) {
  // The following if-statements ensure in particular that we don't try to parse
  // a form as an address-line 1, 2, 3 form because we have collected enough
  // evidence that the current form is a structured form. If structured form
  // fields are missing, they will be discovered later via
  // AddressFieldParser::ParseAddressField.
  if (PossiblyAStructuredAddressForm(context.client_country)) {
    return false;
  }

  // Do not inline these calls: After passing an address field sequence, there
  // might be an additional address line 2 to parse afterwards.
  bool has_field_sequence = ParseAddressFieldSequence(context, scanner);
  if (base::FeatureList::IsEnabled(
          features::kAutofillStructuredFieldsDisableAddressLines) &&
      has_field_sequence) {
    return true;
  }
  bool has_address_lines = ParseAddressLines(context, scanner);
  return has_field_sequence || has_address_lines;
}

bool AddressFieldParser::ParseAddressLines(ParsingContext& context,
                                           AutofillScanner* scanner) {
  // We only match the string "address" in page text, not in element names,
  // because sometimes every element in a group of address fields will have
  // a name containing the string "address"; for example, on the page
  // Kohl's - Register Billing Address.html the text element labeled "city"
  // has the name "BILL_TO_ADDRESS<>city".  We do match address labels
  // such as "address1", which appear as element names on various pages (eg
  // AmericanGirl-Registration.html, BloomingdalesBilling.html,
  // EBay Registration Enter Information.html).
  if (address1_ || street_address_)
    return false;

  // Address line 1 is skipped if a |street_name_|, |house_number_| combination
  // is present.
  if (!(street_name_ && house_number_) &&
      !ParseField(context, scanner, "ADDRESS_LINE_1", &address1_) &&
      !ParseField(context, scanner, "ADDRESS_LINE_1", &street_address_,
                  [](const MatchParams& p) {
                    return WithFieldType(p, FormControlType::kTextArea);
                  })) {
    return false;
  }

  if (street_address_)
    return true;

  if (!ParseField(context, scanner, "ADDRESS_LINE_2", &address2_)) {
    return true;
  }

  // Optionally parse address line 3. This uses the same regexp as address 2
  // above.
  if (!ParseField(context, scanner, "ADDRESS_LINE_EXTRA", &address3_) &&
      !ParseField(context, scanner, "ADDRESS_LINE_2", &address3_)) {
    return true;
  }

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  while (ParseField(context, scanner, "ADDRESS_LINE_EXTRA")) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressFieldParser::ParseHouseNumAptNumStreetNameSequence(
    ParsingContext& context,
    AutofillScanner* scanner) {
  // TODO(crbug.com/383972664) Extend to other countries where prioritizing
  // house number is beneficial.
  // Currently, we only support this sequence in NL.
  if (context.client_country != GeoIpCountryCode("NL") ||
      !base::FeatureList::IsEnabled(features::kAutofillUseNLAddressModel)) {
    return false;
  }

  // Assumes that no field expected in the sequence is present.
  if (street_name_ || house_number_ || apartment_number_) {
    return false;
  }

  const size_t saved_cursor_position = scanner->CursorPosition();

  std::optional<FieldAndMatchInfo> old_street_name = street_name_;
  std::optional<FieldAndMatchInfo> old_house_number = house_number_;
  std::optional<FieldAndMatchInfo> old_apartment_number = apartment_number_;

  ParseHouseNumber(context, scanner);
  ParseApartmentNumber(context, scanner);
  if (house_number_) {
    ParseStreetName(context, scanner);
  }

  // Sequence counts as detected if house number is followed by either a street
  // name, an apartment number, or both.
  // Common address sequence patterns parsed with this function:
  // 1. House number, apartment number, street name.
  // 2. House number, street name.
  // 3. House number, apartment number.
  if (house_number_ && (street_name_ || apartment_number_)) {
    return true;
  }

  // Reset all fields if the non-optional requirements could not be met.
  street_name_ = old_street_name;
  house_number_ = old_house_number;
  apartment_number_ = old_apartment_number;

  scanner->RewindTo(saved_cursor_position);
  return false;
}

bool AddressFieldParser::ParseZipCode(ParsingContext& context,
                                      AutofillScanner* scanner) {
  if (zip_)
    return false;

  if (!ParseField(context, scanner, "ZIP_CODE", &zip_)) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseField(context, scanner, "ZIP_4", &zip4_);
  return true;
}

bool AddressFieldParser::ParseCity(ParsingContext& context,
                                   AutofillScanner* scanner) {
  if (city_)
    return false;

  return ParseField(context, scanner, "CITY", &city_);
}

bool AddressFieldParser::ParseState(ParsingContext& context,
                                    AutofillScanner* scanner) {
  if (state_)
    return false;

  return ParseField(context, scanner, "STATE", &state_);
}

bool AddressFieldParser::ParseStreetLocation(ParsingContext& context,
                                             AutofillScanner* scanner) {
  if (street_location_) {
    return false;
  }
  // TODO(crbug.com/40279279) Find a better way to gate street location
  // support. This is easy to confuse with with an address line 1 field.
  // This is currently allowlisted for MX which prefers pairs of
  // street location and address overflow fields.
  if (context.client_country == GeoIpCountryCode("MX")) {
    return ParseField(context, scanner, "ADDRESS_HOME_STREET_LOCATION",
                      &street_location_);
  }
  // India uses a different set of regexes to match the street location field.
  if (context.client_country == GeoIpCountryCode("IN") &&
      base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return ParseField(context, scanner, "IN_STREET_LOCATION",
                      &street_location_);
  }
  return false;
}

bool AddressFieldParser::ParseDependentLocality(ParsingContext& context,
                                                AutofillScanner* scanner) {
  if (dependent_locality_) {
    return false;
  }
  // Different from `ParseNameAndLabelForDependentLocality()`, by supporting
  // only clients from India.
  if (context.client_country == GeoIpCountryCode("IN") &&
      base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return ParseField(context, scanner, "IN_DEPENDENT_LOCALITY",
                      &dependent_locality_);
  }
  return false;
}

bool AddressFieldParser::ParseLandmark(ParsingContext& context,
                                       AutofillScanner* scanner) {
  if (landmark_) {
    return false;
  }
  // Different from `ParseNameAndLabelForLandmark()`, by supporting only
  // clients from India.
  // TODO(crbug.com/393294031): Use india specific regexes for landmark.
  if (context.client_country == GeoIpCountryCode("IN") &&
      base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return ParseField(context, scanner, "LANDMARK", &landmark_);
  }
  return false;
}

bool AddressFieldParser::ParseStreetName(ParsingContext& context,
                                         AutofillScanner* scanner) {
  if (street_name_) {
    return false;
  }

  return ParseField(context, scanner, "ADDRESS_HOME_STREET_NAME",
                    &street_name_);
}

bool AddressFieldParser::ParseHouseNumber(ParsingContext& context,
                                          AutofillScanner* scanner) {
  if (house_number_) {
    return false;
  }

  return ParseField(context, scanner, "ADDRESS_HOME_HOUSE_NUMBER",
                    &house_number_);
}

bool AddressFieldParser::ParseApartmentNumber(ParsingContext& context,
                                              AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (apartment_number_ || !i18n_model_definition::IsTypeEnabledForCountry(
                               ADDRESS_HOME_APT_NUM, country_code)) {
    return false;
  }

  return ParseField(context, scanner, "ADDRESS_HOME_APT_NUM",
                    &apartment_number_);
}

bool AddressFieldParser::ParseBetweenStreetsOrLandmark(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (between_streets_or_landmark_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK, country_code)) {
    return false;
  }

  return ParseField(context, scanner, "BETWEEN_STREETS_OR_LANDMARK",
                    &between_streets_or_landmark_);
}

bool AddressFieldParser::ParseOverflowAndLandmark(ParsingContext& context,
                                                  AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (overflow_and_landmark_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_OVERFLOW_AND_LANDMARK, country_code)) {
    return false;
  }

  return ParseField(context, scanner, "OVERFLOW_AND_LANDMARK",
                    &overflow_and_landmark_);
}

bool AddressFieldParser::ParseOverflow(ParsingContext& context,
                                       AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (overflow_ || !i18n_model_definition::IsTypeEnabledForCountry(
                       ADDRESS_HOME_OVERFLOW, country_code)) {
    return false;
  }

  return ParseField(context, scanner, "OVERFLOW", &overflow_);
}

bool AddressFieldParser::ParseBetweenStreetsFields(ParsingContext& context,
                                                   AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (!i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_BETWEEN_STREETS, country_code)) {
    return false;
  }

  if (!between_streets_ && !between_streets_line_1_ &&
      ParseField(context, scanner, "BETWEEN_STREETS", &between_streets_)) {
    return true;
  }

  if (!between_streets_line_1_ &&
      ParseField(context, scanner, "BETWEEN_STREETS_LINE_1",
                 &between_streets_line_1_)) {
    return true;
  }

  if (!between_streets_line_2_ &&
      (between_streets_ || between_streets_line_1_) &&
      ParseField(context, scanner, "BETWEEN_STREETS_LINE_2",
                 &between_streets_line_2_)) {
    return true;
  }
  return false;
}

// static
AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelSeparately(
    ParsingContext& context,
    AutofillScanner* scanner,
    const char* regex_name,
    std::optional<FieldAndMatchInfo>* match) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  std::optional<FieldAndMatchInfo> cur_match;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name = ParseField(
      context, scanner, regex_name, &cur_match, [](const MatchParams& p) {
        return WithoutAttribute(p, MatchAttribute::kLabel);
      });
  scanner->RewindTo(saved_cursor);
  bool parsed_label = ParseField(
      context, scanner, regex_name, &cur_match, [](const MatchParams& p) {
        return WithoutAttribute(p, MatchAttribute::kName);
      });
  // Only consider high quality label matches to avoid false positives.
  parsed_label =
      parsed_label && cur_match->match_info.matched_attribute ==
                          MatchInfo::MatchAttribute::kHighQualityLabel;
  if (parsed_name && parsed_label) {
    if (match) {
      *match = std::move(cur_match);
    }
    return RESULT_MATCH_NAME_LABEL;
  }

  scanner->RewindTo(saved_cursor);
  if (parsed_name)
    return RESULT_MATCH_NAME;
  if (parsed_label)
    return RESULT_MATCH_LABEL;
  return RESULT_MATCH_NONE;
}

bool AddressFieldParser::ParseAddressField(ParsingContext& context,
                                           AutofillScanner* scanner) {
  // The |scanner| is not pointing at a field.
  if (scanner->IsEnd())
    return false;

  // Check for matches to both the name and the label.
  ParseNameLabelResult dependent_locality_result =
      ParseNameAndLabelForDependentLocality(context, scanner);
  if (dependent_locality_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult city_result = ParseNameAndLabelForCity(context, scanner);
  if (city_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult state_result =
      ParseNameAndLabelForState(context, scanner);
  if (state_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult country_result =
      ParseNameAndLabelForCountry(context, scanner);
  if (country_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult between_streets_or_landmark_result =
      ParseNameAndLabelForBetweenStreetsOrLandmark(context, scanner);
  if (between_streets_or_landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult overflow_and_landmark_result =
      ParseNameAndLabelForOverflowAndLandmark(context, scanner);
  if (overflow_and_landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  // This line bears some potential problems. A field with
  // <label>Complemento e referência: <input name="complemento"><label>
  // will match the "overflow" in the label and name. The function would
  // exit here. Instead of later recognizing that "Complemento e referência"
  // points to a different type.
  ParseNameLabelResult overflow_result =
      ParseNameAndLabelForOverflow(context, scanner);
  if (overflow_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult landmark_result =
      ParseNameAndLabelForLandmark(context, scanner);
  if (landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult street_location_result =
      ParseNameAndLabelForStreetLocation(context, scanner);
  if (street_location_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult between_streets_result =
      ParseNameAndLabelForBetweenStreets(context, scanner);
  if (between_streets_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult between_street_lines12_result =
      ParseNameAndLabelForBetweenStreetsLines12(context, scanner);
  if (between_street_lines12_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult admin_level2_result =
      ParseNameAndLabelForAdminLevel2(context, scanner);
  if (admin_level2_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult zip_result =
      ParseNameAndLabelForZipCode(context, scanner);
  if (zip_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }

  int num_of_matches = 0;
  for (const auto result :
       {dependent_locality_result, city_result, state_result, country_result,
        zip_result, landmark_result, between_streets_result,
        between_street_lines12_result, admin_level2_result,
        between_streets_or_landmark_result, overflow_and_landmark_result,
        overflow_result, street_location_result}) {
    if (result != RESULT_MATCH_NONE)
      ++num_of_matches;
  }

  // Check if there is only one potential match.
  if (num_of_matches == 1) {
    if (dependent_locality_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, dependent_locality_result,
                                      &dependent_locality_);
    if (city_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, city_result, &city_);
    if (state_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, state_result, &state_);
    if (country_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, country_result, &country_);
    if (between_streets_or_landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner,
                                      between_streets_or_landmark_result,
                                      &between_streets_or_landmark_);
    }
    if (overflow_and_landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, overflow_and_landmark_result,
                                      &overflow_and_landmark_);
    }
    if (overflow_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, overflow_result, &overflow_);
    }
    if (landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, landmark_result, &landmark_);
    }
    if (street_location_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, street_location_result,
                                      &street_location_);
    }
    if (between_streets_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, between_streets_result,
                                      &between_streets_);
    }
    if (between_street_lines12_result != RESULT_MATCH_NONE &&
        !between_streets_line_1_) {
      return SetFieldAndAdvanceCursor(scanner, between_street_lines12_result,
                                      &between_streets_line_1_);
    }
    if (between_street_lines12_result != RESULT_MATCH_NONE &&
        !between_streets_line_2_) {
      return SetFieldAndAdvanceCursor(scanner, between_street_lines12_result,
                                      &between_streets_line_2_);
    }
    if (admin_level2_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, admin_level2_result,
                                      &admin_level2_);
    }
    if (zip_result != RESULT_MATCH_NONE)
      return ParseZipCode(context, scanner);
  }

  // If there is a clash between the country and the state, set the type of
  // the field to the country.
  if (num_of_matches == 2 && state_result != RESULT_MATCH_NONE &&
      country_result != RESULT_MATCH_NONE)
    return SetFieldAndAdvanceCursor(scanner, country_result, &country_);

  // By default give the name priority over the label.
  ParseNameLabelResult results_to_match[] = {RESULT_MATCH_NAME,
                                             RESULT_MATCH_LABEL};
  // Give the label priority over the name to avoid misclassifications when the
  // name has a misleading value (e.g. in TR the province field is named "city",
  // in MX the input field for "Municipio/Delegación" is sometimes named "city"
  // even though that should be mapped to a "Ciudad").
  if (context.page_language == LanguageCode("tr") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLabelPrecedenceForTurkishAddresses)) {
    std::swap(results_to_match[0], results_to_match[1]);
  } else if (context.client_country == GeoIpCountryCode("MX")) {
    // We may want to consider whether we unify this logic with the previous
    // block. Currently, we don't swap the language if page_language ==
    // LanguageCode("es") because Spanish is spoken in many countries and we
    // don't know whether such a change is uniformly positive. At the same time,
    // limiting the feature to the Turkish geolocation may restrict the behavior
    // more than necessary. The list of countries is currently hard-coded for
    // simplicity and performance.
    std::swap(results_to_match[0], results_to_match[1]);
  }

  for (const auto result : results_to_match) {
    if (dependent_locality_result == result)
      return SetFieldAndAdvanceCursor(scanner, dependent_locality_result,
                                      &dependent_locality_);
    if (city_result == result)
      return SetFieldAndAdvanceCursor(scanner, city_result, &city_);
    if (state_result == result)
      return SetFieldAndAdvanceCursor(scanner, state_result, &state_);
    if (country_result == result)
      return SetFieldAndAdvanceCursor(scanner, country_result, &country_);
    if (between_streets_or_landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner,
                                      between_streets_or_landmark_result,
                                      &between_streets_or_landmark_);
    }
    if (overflow_and_landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner, overflow_and_landmark_result,
                                      &overflow_and_landmark_);
    }
    if (overflow_result == result) {
      return SetFieldAndAdvanceCursor(scanner, overflow_result, &overflow_);
    }
    if (landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner, landmark_result, &landmark_);
    }
    if (street_location_result == result) {
      return SetFieldAndAdvanceCursor(scanner, street_location_result,
                                      &street_location_);
    }
    if (between_streets_result == result) {
      return SetFieldAndAdvanceCursor(scanner, between_streets_result,
                                      &between_streets_);
    }
    if (between_street_lines12_result == result && !between_streets_line_1_) {
      return SetFieldAndAdvanceCursor(scanner, between_street_lines12_result,
                                      &between_streets_line_1_);
    }
    if (between_street_lines12_result == result && !between_streets_line_2_) {
      return SetFieldAndAdvanceCursor(scanner, between_street_lines12_result,
                                      &between_streets_line_2_);
    }
    if (admin_level2_result == result) {
      return SetFieldAndAdvanceCursor(scanner, admin_level2_result,
                                      &admin_level2_);
    }
    if (zip_result == result)
      return ParseZipCode(context, scanner);
  }

  return false;
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForZipCode(ParsingContext& context,
                                                AutofillScanner* scanner) {
  if (zip_)
    return RESULT_MATCH_NONE;

  ParseNameLabelResult result =
      ParseNameAndLabelSeparately(context, scanner, "ZIP_CODE", &zip_);

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(context, scanner);
  if (found_non_zip4) {
    city_.reset();
  }
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(context, scanner);
    if (found_non_zip4) {
      state_.reset();
    }
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseField(context, scanner, "ZIP_4", &zip4_);
  }
  return result;
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForDependentLocality(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (dependent_locality_) {
    return RESULT_MATCH_NONE;
  }

  if (context.client_country == GeoIpCountryCode("IN") &&
      base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return ParseNameAndLabelSeparately(
        context, scanner, "IN_DEPENDENT_LOCALITY", &dependent_locality_);
  }

  return ParseNameAndLabelSeparately(context, scanner,
                                     "ADDRESS_HOME_DEPENDENT_LOCALITY",
                                     &dependent_locality_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForCity(ParsingContext& context,
                                             AutofillScanner* scanner) {
  if (city_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(context, scanner, "CITY", &city_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForState(ParsingContext& context,
                                              AutofillScanner* scanner) {
  if (state_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(context, scanner, "STATE", &state_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForCountry(ParsingContext& context,
                                                AutofillScanner* scanner) {
  if (country_)
    return RESULT_MATCH_NONE;

  ParseNameLabelResult country_result =
      ParseNameAndLabelSeparately(context, scanner, "COUNTRY", &country_);
  if (country_result != RESULT_MATCH_NONE)
    return country_result;

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  return ParseNameAndLabelSeparately(context, scanner, "COUNTRY_LOCATION",
                                     &country_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForBetweenStreetsOrLandmark(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (between_streets_or_landmark_ || landmark_ || between_streets_ ||
      between_streets_line_1_ || between_streets_line_2_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK, country_code)) {
    return RESULT_MATCH_NONE;
  }

  auto result = ParseNameAndLabelSeparately(context, scanner,
                                            "BETWEEN_STREETS_OR_LANDMARK",
                                            &between_streets_or_landmark_);

  return result;
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForOverflowAndLandmark(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  //  TODO(crbug.com/40266693) Remove feature check when launched.
  if (overflow_and_landmark_ || overflow_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_OVERFLOW_AND_LANDMARK, country_code)) {
    return RESULT_MATCH_NONE;
  }

  auto result = ParseNameAndLabelSeparately(
      context, scanner, "OVERFLOW_AND_LANDMARK", &overflow_and_landmark_);
  return result;
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForOverflow(ParsingContext& context,
                                                 AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  // TODO(crbug.com/40266693) Remove feature check when launched.
  if (overflow_and_landmark_ || overflow_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(ADDRESS_HOME_OVERFLOW,
                                                      country_code)) {
    return RESULT_MATCH_NONE;
  }

  return ParseNameAndLabelSeparately(context, scanner, "OVERFLOW", &overflow_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForLandmark(ParsingContext& context,
                                                 AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  // TODO(crbug.com/40266693) Remove feature check when launched.
  if (landmark_ || !i18n_model_definition::IsTypeEnabledForCountry(
                       ADDRESS_HOME_LANDMARK, country_code)) {
    return RESULT_MATCH_NONE;
  }

  return ParseNameAndLabelSeparately(context, scanner, "LANDMARK", &landmark_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForStreetLocation(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (street_location_ || context.client_country != GeoIpCountryCode("IN") ||
      !base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return RESULT_MATCH_NONE;
  }
  return ParseNameAndLabelSeparately(context, scanner, "IN_STREET_LOCATION",
                                     &street_location_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForBetweenStreets(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  // TODO(crbug.com/40266693) Remove feature check when launched.
  if (between_streets_ || between_streets_line_1_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_BETWEEN_STREETS, country_code)) {
    return RESULT_MATCH_NONE;
  }

  return ParseNameAndLabelSeparately(context, scanner, "BETWEEN_STREETS",
                                     &between_streets_);
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForBetweenStreetsLines12(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  // TODO(crbug.com/40266693) Remove feature check when launched.
  if (between_streets_line_2_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_BETWEEN_STREETS, country_code)) {
    return RESULT_MATCH_NONE;
  }

  if (!between_streets_line_1_) {
    return ParseNameAndLabelSeparately(
        context, scanner, "BETWEEN_STREETS_LINE_1", &between_streets_line_1_);
  } else if (!between_streets_line_2_) {
    return ParseNameAndLabelSeparately(
        context, scanner, "BETWEEN_STREETS_LINE_2", &between_streets_line_2_);
  }

  return RESULT_MATCH_NONE;
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForAdminLevel2(ParsingContext& context,
                                                    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  // TODO(crbug.com/40266693) Remove feature check when launched.
  if (admin_level2_ || !i18n_model_definition::IsTypeEnabledForCountry(
                           ADDRESS_HOME_ADMIN_LEVEL2, country_code)) {
    return RESULT_MATCH_NONE;
  }

  return ParseNameAndLabelSeparately(context, scanner, "ADMIN_LEVEL_2",
                                     &admin_level2_);
}

bool AddressFieldParser::SetFieldAndAdvanceCursor(
    AutofillScanner* scanner,
    ParseNameLabelResult parse_result,
    std::optional<FormFieldParser::FieldAndMatchInfo>* match) {
  auto match_attribute_of = [](ParseNameLabelResult parse_result) {
    switch (parse_result) {
      case RESULT_MATCH_NONE:
        NOTREACHED();
      case RESULT_MATCH_LABEL:
      // Since the parser matches against the label first, interpret
      // RESULT_MATCH_NAME_LABEL as a label match.
      // `ParseNameAndLabelSeparately()` only allows for high quality label
      // matches.
      case RESULT_MATCH_NAME_LABEL:
        return MatchInfo::MatchAttribute::kHighQualityLabel;
      case RESULT_MATCH_NAME:
        return MatchInfo::MatchAttribute::kName;
    }
  };
  *match = {scanner->Cursor(),
            {.matched_attribute = match_attribute_of(parse_result)}};
  scanner->Advance();
  return true;
}

bool AddressFieldParser::ParseFieldSpecificsForHouseNumberAndApt(
    ParsingContext& context,
    AutofillScanner* scanner) {
  AddressCountryCode country_code(context.client_country.value());
  if (house_number_and_apt_ || house_number_ || apartment_number_ ||
      !i18n_model_definition::IsTypeEnabledForCountry(
          ADDRESS_HOME_HOUSE_NUMBER_AND_APT, country_code)) {
    return RESULT_MATCH_NONE;
  }

  return ParseField(context, scanner, "ADDRESS_HOME_HOUSE_NUMBER_AND_APT",
                    &house_number_and_apt_);
}

bool AddressFieldParser::PossiblyAStructuredAddressForm(
    GeoIpCountryCode country_code) const {
  // Record success if the house number and at least one of the other
  // fields were found because that indicates a structured address form.
  if (house_number_ &&
      (street_name_ || zip_ || overflow_ || overflow_and_landmark_ ||
       between_streets_or_landmark_ || apartment_number_ || between_streets_ ||
       between_streets_line_1_ || between_streets_line_2_)) {
    return true;
  }

  if (street_name_ && house_number_and_apt_) {
    return true;
  }

  // India has a specific set of fields that are required to be present in order
  // to be considered a structured address form. For now only the combination
  // where all `street_location_`, `dependent_locality_` and `landmark_` are
  // present is supported.
  // TODO(crbug.com/393294031): Accept combination of synthetic fields too.
  if (country_code == GeoIpCountryCode("IN") && street_location_ &&
      dependent_locality_ && landmark_) {
    return true;
  }

  return street_location_ &&
         (apartment_number_ || overflow_ || overflow_and_landmark_ ||
          between_streets_or_landmark_ || between_streets_ ||
          between_streets_line_1_ || between_streets_line_2_);
}

}  // namespace autofill
