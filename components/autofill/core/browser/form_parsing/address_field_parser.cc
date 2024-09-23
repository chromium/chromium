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
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

namespace {

base::span<const MatchPatternRef> GetMatchPatterns(std::string_view name,
                                                   ParsingContext& context) {
  return GetMatchPatterns(name, context.page_language, context.pattern_file);
}

base::span<const MatchPatternRef> GetMatchPatterns(FieldType type,
                                                   ParsingContext& context) {
  return GetMatchPatterns(type, context.page_language, context.pattern_file);
}

bool SetFieldAndAdvanceCursor(AutofillScanner* scanner,
                              raw_ptr<AutofillField>* field) {
  *field = scanner->Cursor();
  scanner->Advance();
  return true;
}

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

  base::span<const MatchPatternRef> email_patterns =
      GetMatchPatterns("EMAIL_ADDRESS", context);
  base::span<const MatchPatternRef> address_patterns =
      GetMatchPatterns("ADDRESS_LOOKUP", context);
  base::span<const MatchPatternRef> address_ignore_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", context);
  base::span<const MatchPatternRef> attention_ignore_patterns =
      GetMatchPatterns("ATTENTION_IGNORED", context);
  base::span<const MatchPatternRef> region_ignore_patterns =
      GetMatchPatterns("REGION_IGNORED", context);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    // Ignore "Address Lookup" field. http://crbug.com/427622
    if (ParseField(context, scanner, address_patterns, nullptr,
                   "ADDRESS_LOOKUP") ||
        ParseField(context, scanner, address_ignore_patterns, nullptr,
                   "ADDRESS_NAME_IGNORED")) {
      continue;
      // Ignore email addresses.
    } else if (ParseField(context, scanner, email_patterns, nullptr, "kEmailRe",
                          [](const MatchParams& p) {
                            return WithFieldType(p, FormControlType::kTextArea);
                          })) {
      continue;
    } else if (address_field->ParseAddress(context, scanner) ||
               address_field->ParseAddressField(context, scanner) ||
               address_field->ParseCompany(context, scanner)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(context, scanner, attention_ignore_patterns, nullptr,
                          "ATTENTION_IGNORED") ||
               ParseField(context, scanner, region_ignore_patterns, nullptr,
                          "REGION_IGNORED")) {
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

  base::span<const MatchPatternRef> company_patterns =
      GetMatchPatterns("COMPANY_NAME", context);
  return ParseField(context, scanner, company_patterns, &company_,
                    "COMPANY_NAME");
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

  base::span<const MatchPatternRef> street_location_patterns =
      GetMatchPatterns(ADDRESS_HOME_STREET_LOCATION, context);
  base::span<const MatchPatternRef> street_name_patterns =
      GetMatchPatterns(ADDRESS_HOME_STREET_NAME, context);
  base::span<const MatchPatternRef> house_number_patterns =
      GetMatchPatterns(ADDRESS_HOME_HOUSE_NUMBER, context);
  base::span<const MatchPatternRef> apartment_number_patterns =
      GetMatchPatterns(ADDRESS_HOME_APT_NUM, context);
  base::span<const MatchPatternRef> overflow_patterns =
      GetMatchPatterns("OVERFLOW", context);
  base::span<const MatchPatternRef> overflow_and_landmark_patterns =
      GetMatchPatterns("OVERFLOW_AND_LANDMARK", context);
  base::span<const MatchPatternRef> between_streets_or_landmark_patterns =
      GetMatchPatterns("BETWEEN_STREETS_OR_LANDMARK", context);
  base::span<const MatchPatternRef> between_streets_patterns =
      GetMatchPatterns("BETWEEN_STREETS", context);
  base::span<const MatchPatternRef> between_streets_line_1_patterns =
      GetMatchPatterns("BETWEEN_STREETS_LINE_1", context);
  base::span<const MatchPatternRef> between_streets_line_2_patterns =
      GetMatchPatterns("BETWEEN_STREETS_LINE_2", context);

  AutofillField* old_street_location = street_location_;
  AutofillField* old_street_name = street_name_;
  AutofillField* old_overflow = overflow_;
  AutofillField* old_between_streets_or_landmark = between_streets_or_landmark_;
  AutofillField* old_overflow_and_landmark = overflow_and_landmark_;
  AutofillField* old_between_streets = between_streets_;
  AutofillField* old_between_streets_line_1 = between_streets_line_1_;
  AutofillField* old_between_streets_line_2 = between_streets_line_2_;
  AutofillField* old_house_number = house_number_;
  AutofillField* old_zip = zip_;
  AutofillField* old_zip4 = zip4_;
  AutofillField* old_apartment_number = apartment_number_;
  AutofillField* old_house_number_and_apt_ = house_number_and_apt_;

  AddressCountryCode country_code(context.client_country.value());

  while (!scanner->IsEnd()) {
    // We look for street location before street name, because the name/label of
    // a street location typically contains strings that match the regular
    // expressions for a street name as well.
    if (!street_location_ &&
        // TODO(crbug.com/40279279) Find a better way to gate street location
        // support. This is easy to confuse with with an address line 1 field.
        // This is currently allowlisted for MX which prefers pairs of
        // street location and address overflow fields.
        context.client_country == GeoIpCountryCode("MX") &&
        ParseField(context, scanner, street_location_patterns,
                   &street_location_, "ADDRESS_HOME_STREET_LOCATION")) {
      continue;
    }

    // TODO(crbug.com/40279279) Factor out these ParseFieldSpecifics into
    // ParseStreetName and similar functions.
    if (!street_name_ && !street_location_ &&
        ParseField(context, scanner, street_name_patterns, &street_name_,
                   "ADDRESS_HOME_STREET_NAME")) {
      continue;
    }

    if (ParseZipCode(context, scanner)) {
      continue;
    }
    if (!(between_streets_or_landmark_ || between_streets_ ||
          between_streets_line_1_ || between_streets_line_2_) &&
        i18n_model_definition::IsTypeEnabledForCountry(
            ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK, country_code) &&
        ParseField(context, scanner, between_streets_or_landmark_patterns,
                   &between_streets_or_landmark_,
                   "BETWEEN_STREETS_OR_LANDMARK")) {
      continue;
    }

    if (!(overflow_and_landmark_ || overflow_) &&
        i18n_model_definition::IsTypeEnabledForCountry(
            ADDRESS_HOME_OVERFLOW_AND_LANDMARK, country_code) &&
        ParseField(context, scanner, overflow_and_landmark_patterns,
                   &overflow_and_landmark_, "OVERFLOW_AND_LANDMARK")) {
      continue;
    }

    // Because `overflow_and_landmark_` and `overflow_` overflow in semantics
    // we don't want them both to be in the same form section. This would
    // probably point to some problem in the classification.
    if (!(overflow_and_landmark_ || overflow_) &&
        i18n_model_definition::IsTypeEnabledForCountry(ADDRESS_HOME_OVERFLOW,
                                                       country_code) &&
        ParseField(context, scanner, overflow_patterns, &overflow_,
                   "OVERFLOW")) {
      continue;
    }

    if (ParseFieldSpecificsForHouseNumberAndApt(context, scanner)) {
      continue;
    }

    if (!house_number_ && !street_location_ &&
        ParseField(context, scanner, house_number_patterns, &house_number_,
                   "ADDRESS_HOME_HOUSE_NUMBER")) {
      continue;
    }

    if (!apartment_number_ &&
        i18n_model_definition::IsTypeEnabledForCountry(ADDRESS_HOME_APT_NUM,
                                                       country_code) &&
        ParseField(context, scanner, apartment_number_patterns,
                   &apartment_number_, "ADDRESS_HOME_APT_NUM")) {
      continue;
    }

    if (i18n_model_definition::IsTypeEnabledForCountry(
            ADDRESS_HOME_BETWEEN_STREETS, country_code)) {
      if (!between_streets_ && !between_streets_line_1_ &&
          ParseField(context, scanner, between_streets_patterns,
                     &between_streets_, "BETWEEN_STREETS")) {
        continue;
      }

      if (!between_streets_line_1_ &&
          ParseField(context, scanner, between_streets_line_1_patterns,
                     &between_streets_line_1_, "BETWEEN_STREETS_LINE_1")) {
        continue;
      }

      if ((between_streets_ || between_streets_line_1_) &&
          !between_streets_line_2_ &&
          ParseField(context, scanner, between_streets_line_2_patterns,
                     &between_streets_line_2_, "BETWEEN_STREETS_LINE_2")) {
        continue;
      }
    }

    break;
  }

  // This is a safety mechanism: If no field was classified, we do not want to
  // return true, because the caller assumes that the cursor position moved.
  // Otherwise, we could end up in an infinite loop.
  if (scanner->CursorPosition() == saved_cursor_position) {
    return false;
  }

  if (PossiblyAStructuredAddressForm()) {
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
  if (PossiblyAStructuredAddressForm()) {
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

  base::span<const MatchPatternRef> address_line1_patterns =
      GetMatchPatterns("ADDRESS_LINE_1", context);

  // Address line 1 is skipped if a |street_name_|, |house_number_| combination
  // is present.
  if (!(street_name_ && house_number_) &&
      !ParseField(context, scanner, address_line1_patterns, &address1_,
                  "ADDRESS_LINE_1") &&
      !ParseField(context, scanner, address_line1_patterns, &street_address_,
                  "ADDRESS_LINE_1", [](const MatchParams& p) {
                    return WithFieldType(p, FormControlType::kTextArea);
                  })) {
    return false;
  }

  if (street_address_)
    return true;

  base::span<const MatchPatternRef> address_line2_patterns =
      GetMatchPatterns("ADDRESS_LINE_2", context);

  if (!ParseField(context, scanner, address_line2_patterns, &address2_,
                  "ADDRESS_LINE_2")) {
    return true;
  }

  base::span<const MatchPatternRef> address_line_extra_patterns =
      GetMatchPatterns("ADDRESS_LINE_EXTRA", context);

  // Optionally parse address line 3. This uses the same regexp as address 2
  // above.
  if (!ParseField(context, scanner, address_line_extra_patterns, &address3_,
                  "ADDRESS_LINE_EXTRA") &&
      !ParseField(context, scanner, address_line2_patterns, &address3_,
                  "ADDRESS_LINE_2")) {
    return true;
  }

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  while (ParseField(context, scanner, address_line_extra_patterns, nullptr,
                    "ADDRESS_LINE_EXTRA")) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressFieldParser::ParseZipCode(ParsingContext& context,
                                      AutofillScanner* scanner) {
  if (zip_)
    return false;

  base::span<const MatchPatternRef> zip_code_patterns =
      GetMatchPatterns("ZIP_CODE", context);
  base::span<const MatchPatternRef> four_digit_zip_code_patterns =
      GetMatchPatterns("ZIP_4", context);

  if (!ParseField(context, scanner, zip_code_patterns, &zip_, "ZIP_CODE")) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseField(context, scanner, four_digit_zip_code_patterns, &zip4_, "ZIP_4");
  return true;
}

bool AddressFieldParser::ParseCity(ParsingContext& context,
                                   AutofillScanner* scanner) {
  if (city_)
    return false;

  base::span<const MatchPatternRef> city_patterns =
      GetMatchPatterns("CITY", context);
  return ParseField(context, scanner, city_patterns, &city_, "CITY");
}

bool AddressFieldParser::ParseState(ParsingContext& context,
                                    AutofillScanner* scanner) {
  if (state_)
    return false;

  base::span<const MatchPatternRef> patterns_state =
      GetMatchPatterns("STATE", context);
  return ParseField(context, scanner, patterns_state, &state_, "STATE");
}

// static
AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelSeparately(
    ParsingContext& context,
    AutofillScanner* scanner,
    base::span<const MatchPatternRef> patterns,
    raw_ptr<AutofillField>* match,
    const char* regex_name) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  raw_ptr<AutofillField> cur_match = nullptr;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name =
      ParseField(context, scanner, patterns, &cur_match, regex_name,
                 [](const MatchParams& p) {
                   return WithoutAttribute(p, MatchAttribute::kLabel);
                 });
  scanner->RewindTo(saved_cursor);
  bool parsed_label =
      ParseField(context, scanner, patterns, &cur_match, regex_name,
                 [](const MatchParams& p) {
                   return WithoutAttribute(p, MatchAttribute::kName);
                 });
  if (parsed_name && parsed_label) {
    if (match)
      *match = cur_match;
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
        overflow_result}) {
    if (result != RESULT_MATCH_NONE)
      ++num_of_matches;
  }

  // Check if there is only one potential match.
  if (num_of_matches == 1) {
    if (dependent_locality_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &dependent_locality_);
    if (city_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &city_);
    if (state_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &state_);
    if (country_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &country_);
    if (between_streets_or_landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_or_landmark_);
    }
    if (overflow_and_landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &overflow_and_landmark_);
    }
    if (overflow_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &overflow_);
    }
    if (landmark_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &landmark_);
    }
    if (between_streets_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_);
    }
    if (between_street_lines12_result != RESULT_MATCH_NONE &&
        !between_streets_line_1_) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_line_1_);
    }
    if (between_street_lines12_result != RESULT_MATCH_NONE &&
        !between_streets_line_2_) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_line_2_);
    }
    if (admin_level2_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &admin_level2_);
    }
    if (zip_result != RESULT_MATCH_NONE)
      return ParseZipCode(context, scanner);
  }

  // If there is a clash between the country and the state, set the type of
  // the field to the country.
  if (num_of_matches == 2 && state_result != RESULT_MATCH_NONE &&
      country_result != RESULT_MATCH_NONE)
    return SetFieldAndAdvanceCursor(scanner, &country_);

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
      return SetFieldAndAdvanceCursor(scanner, &dependent_locality_);
    if (city_result == result)
      return SetFieldAndAdvanceCursor(scanner, &city_);
    if (state_result == result)
      return SetFieldAndAdvanceCursor(scanner, &state_);
    if (country_result == result)
      return SetFieldAndAdvanceCursor(scanner, &country_);
    if (between_streets_or_landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_or_landmark_);
    }
    if (overflow_and_landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &overflow_and_landmark_);
    }
    if (overflow_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &overflow_);
    }
    if (landmark_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &landmark_);
    }
    if (between_streets_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_);
    }
    if (between_street_lines12_result == result && !between_streets_line_1_) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_line_1_);
    }
    if (between_street_lines12_result == result && !between_streets_line_2_) {
      return SetFieldAndAdvanceCursor(scanner, &between_streets_line_2_);
    }
    if (admin_level2_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &admin_level2_);
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

  base::span<const MatchPatternRef> zip_code_patterns =
      GetMatchPatterns("ZIP_CODE", context);
  base::span<const MatchPatternRef> four_digit_zip_code_patterns =
      GetMatchPatterns("ZIP_4", context);

  ParseNameLabelResult result = ParseNameAndLabelSeparately(
      context, scanner, zip_code_patterns, &zip_, "ZIP_CODE");

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(context, scanner);
  if (found_non_zip4)
    city_ = nullptr;
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(context, scanner);
    if (found_non_zip4)
      state_ = nullptr;
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseField(context, scanner, four_digit_zip_code_patterns, &zip4_, "ZIP_4");
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

  base::span<const MatchPatternRef> dependent_locality_patterns =
      GetMatchPatterns(ADDRESS_HOME_DEPENDENT_LOCALITY, context.page_language,
                       context.pattern_file);
  return ParseNameAndLabelSeparately(
      context, scanner, dependent_locality_patterns, &dependent_locality_,
      "ADDRESS_HOME_DEPENDENT_LOCALITY");
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForCity(ParsingContext& context,
                                             AutofillScanner* scanner) {
  if (city_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> city_patterns =
      GetMatchPatterns("CITY", context);
  return ParseNameAndLabelSeparately(context, scanner, city_patterns, &city_,
                                     "CITY");
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForState(ParsingContext& context,
                                              AutofillScanner* scanner) {
  if (state_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> patterns_state =
      GetMatchPatterns("STATE", context);
  return ParseNameAndLabelSeparately(context, scanner, patterns_state, &state_,
                                     "STATE");
}

AddressFieldParser::ParseNameLabelResult
AddressFieldParser::ParseNameAndLabelForCountry(ParsingContext& context,
                                                AutofillScanner* scanner) {
  if (country_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> country_patterns =
      GetMatchPatterns("COUNTRY", context);
  base::span<const MatchPatternRef> country_location_patterns =
      GetMatchPatterns("COUNTRY_LOCATION", context);

  ParseNameLabelResult country_result = ParseNameAndLabelSeparately(
      context, scanner, country_patterns, &country_, "COUNTRY");
  if (country_result != RESULT_MATCH_NONE)
    return country_result;

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  return ParseNameAndLabelSeparately(context, scanner,
                                     country_location_patterns, &country_,
                                     "COUNTRY_LOCATION");
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

  base::span<const MatchPatternRef> between_streets_or_landmark_patterns =
      GetMatchPatterns("BETWEEN_STREETS_OR_LANDMARK", context);
  auto result = ParseNameAndLabelSeparately(
      context, scanner, between_streets_or_landmark_patterns,
      &between_streets_or_landmark_, "BETWEEN_STREETS_OR_LANDMARK");

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

  base::span<const MatchPatternRef> overflow_and_landmark_patterns =
      GetMatchPatterns("OVERFLOW_AND_LANDMARK", context);
  auto result = ParseNameAndLabelSeparately(
      context, scanner, overflow_and_landmark_patterns, &overflow_and_landmark_,
      "OVERFLOW_AND_LANDMARK");
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

  base::span<const MatchPatternRef> overflow_patterns =
      GetMatchPatterns("OVERFLOW", context);
  return ParseNameAndLabelSeparately(context, scanner, overflow_patterns,
                                     &overflow_, "OVERFLOW");
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

  base::span<const MatchPatternRef> landmark_patterns =
      GetMatchPatterns("LANDMARK", context);
  return ParseNameAndLabelSeparately(context, scanner, landmark_patterns,
                                     &landmark_, "LANDMARK");
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

  base::span<const MatchPatternRef> between_streets_patterns =
      GetMatchPatterns("BETWEEN_STREETS", context);
  return ParseNameAndLabelSeparately(context, scanner, between_streets_patterns,
                                     &between_streets_, "BETWEEN_STREETS");
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
    base::span<const MatchPatternRef> between_streets_patterns_line_1 =
        GetMatchPatterns("BETWEEN_STREETS_LINE_1", context.page_language,
                         context.pattern_file);
    return ParseNameAndLabelSeparately(
        context, scanner, between_streets_patterns_line_1,
        &between_streets_line_1_, "BETWEEN_STREETS_LINE_1");
  } else if (!between_streets_line_2_) {
    base::span<const MatchPatternRef> between_streets_patterns_line_2 =
        GetMatchPatterns("BETWEEN_STREETS_LINE_2", context.page_language,
                         context.pattern_file);
    return ParseNameAndLabelSeparately(
        context, scanner, between_streets_patterns_line_2,
        &between_streets_line_2_, "BETWEEN_STREETS_LINE_2");
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

  base::span<const MatchPatternRef> admin_level2_patterns =
      GetMatchPatterns("ADMIN_LEVEL_2", context);
  return ParseNameAndLabelSeparately(context, scanner, admin_level2_patterns,
                                     &admin_level2_, "ADMIN_LEVEL_2");
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

  base::span<const MatchPatternRef> house_number_and_apt_patterns =
      GetMatchPatterns("ADDRESS_HOME_HOUSE_NUMBER_AND_APT", context);
  return ParseField(context, scanner, house_number_and_apt_patterns,
                    &house_number_and_apt_,
                    "ADDRESS_HOME_HOUSE_NUMBER_AND_APT");
}

bool AddressFieldParser::PossiblyAStructuredAddressForm() const {
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

  return street_location_ &&
         (apartment_number_ || overflow_ || overflow_and_landmark_ ||
          between_streets_or_landmark_ || between_streets_ ||
          between_streets_line_1_ || between_streets_line_2_);
}

}  // namespace autofill
