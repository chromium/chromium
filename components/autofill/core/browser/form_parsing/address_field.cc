// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

namespace {

bool SetFieldAndAdvanceCursor(AutofillScanner* scanner,
                              raw_ptr<AutofillField>* field) {
  *field = scanner->Cursor();
  scanner->Advance();
  return true;
}

// Removes a MatchAttribute from a MatchingPattern.
// TODO(crbug/1142936): This is necessary for
// AddressField::ParseNameAndLabelSeparately().
MatchingPattern WithoutAttribute(MatchingPattern p, MatchAttribute attribute) {
  DenseSet<MatchAttribute> match_field_attributes = p.match_field_attributes;
  match_field_attributes.erase(attribute);
  return {
      .positive_pattern = p.positive_pattern,
      .negative_pattern = p.negative_pattern,
      .positive_score = p.positive_score,
      .match_field_attributes = match_field_attributes,
      .match_field_input_types = p.match_field_input_types,
  };
}

// Removes a MatchAttribute from a MatchParams.
// TODO(crbug/1142936): This is necessary for
// AddressField::ParseNameAndLabelSeparately().
MatchParams WithoutAttribute(MatchParams match_type, MatchAttribute attribute) {
  match_type.attributes.erase(attribute);
  return match_type;
}

// Adds a MatchFieldType to a MatchingPattern.
// TODO(crbug/1142936): This is necessary for AddressField::ParseAddressLines()
// and AddressField::Parse().
MatchingPattern WithFieldType(MatchingPattern p, MatchFieldType field_type) {
  DenseSet<MatchFieldType> match_field_input_types = p.match_field_input_types;
  match_field_input_types.insert(field_type);
  return {
      .positive_pattern = p.positive_pattern,
      .negative_pattern = p.negative_pattern,
      .positive_score = p.positive_score,
      .match_field_attributes = p.match_field_attributes,
      .match_field_input_types = match_field_input_types,
  };
}

// Some sites use type="tel" for zip fields (to get a numerical input).
// http://crbug.com/426958
constexpr MatchParams kZipCodeMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTelephone,
                            MatchFieldType::kNumber>;

constexpr MatchParams kDependentLocalityMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kSelect,
                            MatchFieldType::kSearch,
                            MatchFieldType::kTextArea>;

constexpr MatchParams kStreetLocationMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;

// Select fields are allowed here.  This occurs on top-100 site rediff.com.
constexpr MatchParams kCityMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kSelect, MatchFieldType::kSearch>;

constexpr MatchParams kStateMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kSelect, MatchFieldType::kSearch>;

constexpr MatchParams kLandmarkMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;

constexpr MatchParams kBetweenStreetsOrLandmarkMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;

constexpr MatchParams kBetweenStreetsMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;

constexpr MatchParams kAdminLevel2MatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea,
                            MatchFieldType::kSearch,
                            MatchFieldType::kSelect>;
constexpr MatchParams kOverflowMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;
constexpr MatchParams kOverflowAndLandmarkMatchType =
    kDefaultMatchParamsWith<MatchFieldType::kTextArea, MatchFieldType::kSearch>;

}  // namespace

// static
std::unique_ptr<FormField> AddressField::Parse(
    AutofillScanner* scanner,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  std::unique_ptr<AddressField> address_field(new AddressField(log_manager));
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  base::span<const MatchPatternRef> email_patterns =
      GetMatchPatterns("EMAIL_ADDRESS", page_language, pattern_source);

  base::span<const MatchPatternRef> address_patterns =
      GetMatchPatterns("ADDRESS_LOOKUP", page_language, pattern_source);

  base::span<const MatchPatternRef> address_ignore_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", page_language, pattern_source);

  base::span<const MatchPatternRef> attention_ignore_patterns =
      GetMatchPatterns("ATTENTION_IGNORED", page_language, pattern_source);

  base::span<const MatchPatternRef> region_ignore_patterns =
      GetMatchPatterns("REGION_IGNORED", page_language, pattern_source);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    // Ignore "Address Lookup" field. http://crbug.com/427622
    if (ParseField(scanner, kAddressLookupRe, address_patterns, nullptr,
                   {log_manager, "kAddressLookupRe"}) ||
        ParseField(scanner, kAddressNameIgnoredRe, address_ignore_patterns,
                   nullptr, {log_manager, "kAddressNameIgnoreRe"})) {
      continue;
      // Ignore email addresses.
    } else if (ParseFieldSpecifics(
                   scanner, kEmailRe,
                   kDefaultMatchParamsWith<MatchFieldType::kTextArea>,
                   email_patterns, nullptr, {log_manager, "kEmailRe"},
                   [](const MatchingPattern& p) {
                     return WithFieldType(p, MatchFieldType::kTextArea);
                   })) {
      continue;
    } else if (address_field->ParseAddress(scanner, client_country,
                                           page_language, pattern_source) ||
               address_field->ParseAddressField(
                   scanner, client_country, page_language, pattern_source) ||
               address_field->ParseCompany(scanner, page_language,
                                           pattern_source)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(scanner, kAttentionIgnoredRe,
                          attention_ignore_patterns, nullptr,
                          {log_manager, "kAttentionIgnoredRe"}) ||
               ParseField(scanner, kRegionIgnoredRe, region_ignore_patterns,
                          nullptr, {log_manager, "kRegionIgnoredRe"})) {
      // We ignore the following:
      // * Attention.
      // * Province/Region/Other.
      continue;
    } else if (scanner->Cursor() != initial_field &&
               ParseEmptyLabel(scanner, nullptr)) {
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
      address_field->between_streets_ || address_field->admin_level2_ ||
      address_field->between_streets_or_landmark_ ||
      address_field->overflow_and_landmark_ || address_field->overflow_ ||
      address_field->street_location_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

// static
bool AddressField::IsStandaloneZipSupported(
    const GeoIpCountryCode& client_country) {
  return client_country == GeoIpCountryCode("BR") ||
         client_country == GeoIpCountryCode("MX");
}

// static
std::unique_ptr<FormField> AddressField::ParseStandaloneZip(
    AutofillScanner* scanner,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableZipOnlyAddressForms)) {
    return nullptr;
  }

  if (scanner->IsEnd()) {
    return nullptr;
  }

  std::unique_ptr<AddressField> address_field(new AddressField(log_manager));
  size_t saved_cursor = scanner->SaveCursor();

  address_field->ParseZipCode(scanner, page_language, pattern_source);
  if (address_field->zip_) {
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

AddressField::AddressField(LogManager* log_manager)
    : log_manager_(log_manager) {}

void AddressField::AddClassifications(
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
  AddClassification(admin_level2_, ADDRESS_HOME_ADMIN_LEVEL2,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(between_streets_or_landmark_,
                    ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(overflow_and_landmark_, ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(overflow_, ADDRESS_HOME_OVERFLOW, kBaseAddressParserScore,
                    field_candidates);
}

bool AddressField::ParseCompany(AutofillScanner* scanner,
                                const LanguageCode& page_language,
                                PatternSource pattern_source) {
  if (company_)
    return false;

  base::span<const MatchPatternRef> company_patterns =
      GetMatchPatterns("COMPANY_NAME", page_language, pattern_source);

  return ParseField(scanner, kCompanyRe, company_patterns, &company_,
                    {log_manager_, "kCompanyRe"});
}

bool AddressField::ParseAddressFieldSequence(
    AutofillScanner* scanner,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
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

  base::span<const MatchPatternRef> street_location_patterns = GetMatchPatterns(
      ADDRESS_HOME_STREET_LOCATION, page_language, pattern_source);
  base::span<const MatchPatternRef> street_name_patterns =
      GetMatchPatterns(ADDRESS_HOME_STREET_NAME, page_language, pattern_source);
  base::span<const MatchPatternRef> house_number_patterns = GetMatchPatterns(
      ADDRESS_HOME_HOUSE_NUMBER, page_language, pattern_source);
  base::span<const MatchPatternRef> apartment_number_patterns =
      GetMatchPatterns(ADDRESS_HOME_APT_NUM, page_language, pattern_source);
  base::span<const MatchPatternRef> overflow_patterns =
      GetMatchPatterns("OVERFLOW", page_language, pattern_source);
  base::span<const MatchPatternRef> overflow_and_landmark_patterns =
      GetMatchPatterns("OVERFLOW_AND_LANDMARK", page_language, pattern_source);
  base::span<const MatchPatternRef> between_streets_or_landmark_patterns =
      GetMatchPatterns("BETWEEN_STREETS_OR_LANDMARK", page_language,
                       pattern_source);
  base::span<const MatchPatternRef> between_streets_patterns =
      GetMatchPatterns("BETWEEN_STREETS", page_language, pattern_source);

  AutofillField* old_street_location = street_location_;
  AutofillField* old_street_name = street_name_;
  AutofillField* old_overflow = overflow_;
  AutofillField* old_between_streets_or_landmark = between_streets_or_landmark_;
  AutofillField* old_overflow_and_landmark = overflow_and_landmark_;
  AutofillField* old_between_streets = between_streets_;
  AutofillField* old_house_number = house_number_;
  AutofillField* old_zip = zip_;
  AutofillField* old_zip4 = zip4_;
  AutofillField* old_apartment_number = apartment_number_;

  while (!scanner->IsEnd()) {
    // We look for street location before street name, because the name/label of
    // a street location typically contains strings that match the regular
    // expressions for a street name as well.
    if (!street_location_ && client_country == GeoIpCountryCode("MX") &&
        ParseFieldSpecifics(scanner, kStreetLocationRe,
                            kStreetLocationMatchType, street_location_patterns,
                            &street_location_,
                            {log_manager_, "kStreetLocationRe"})) {
      continue;
    }

    // TODO(crbug.com/1474308) Factor out these ParseFieldSpecifics into
    // ParseStreetName and similar functions.
    if (!street_name_ && !street_location_ &&
        ParseFieldSpecifics(scanner, kStreetNameRe,
                            kDefaultMatchParamsWith<MatchFieldType::kSearch>,
                            street_name_patterns, &street_name_,
                            {log_manager_, "kStreetNameRe"})) {
      continue;
    }

    if (ParseZipCode(scanner, page_language, pattern_source)) {
      continue;
    }

    if (!(between_streets_or_landmark_ || between_streets_) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForBetweenStreetsOrLandmark) &&
        ParseFieldSpecifics(scanner, kBetweenStreetsOrLandmarkRe,
                            kBetweenStreetsOrLandmarkMatchType,
                            between_streets_or_landmark_patterns,
                            &between_streets_or_landmark_,
                            {log_manager_, "kBetweenStreetsOrLandmarkRe"})) {
      continue;
    }

    if (!(overflow_and_landmark_ || overflow_) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForAddressOverflowAndLandmark) &&
        ParseFieldSpecifics(
            scanner, kOverflowAndLandmarkRe, kOverflowAndLandmarkMatchType,
            overflow_and_landmark_patterns, &overflow_and_landmark_,
            {log_manager_, "kOverflowAndLandmarkRe"})) {
      continue;
    }

    // Because `overflow_and_landmark_` and `overflow_` overflow in semantics
    // we don't want them both to be in the same form section. This would
    // probably point to some problem in the classification.
    if (!(overflow_and_landmark_ || overflow_) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForAddressOverflow) &&
        ParseFieldSpecifics(scanner, kOverflowRe, kOverflowMatchType,
                            overflow_patterns, &overflow_,
                            {log_manager_, "kOverflowRe"})) {
      continue;
    }

    if (!house_number_ && !street_location_ &&
        ParseFieldSpecifics(scanner, kHouseNumberRe,
                            kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                                    MatchFieldType::kTelephone>,
                            house_number_patterns, &house_number_,
                            {log_manager_, "kHouseNumberRe"})) {
      continue;
    }

    // TODO(crbug.com/1153715): Remove finch guard once launched.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForApartmentNumbers) &&
        !apartment_number_ &&
        ParseFieldSpecifics(scanner, kApartmentNumberRe,
                            kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                                    MatchFieldType::kTelephone>,
                            apartment_number_patterns, &apartment_number_,
                            {log_manager_, "kApartmentNumberRe"})) {
      continue;
    }

    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForBetweenStreets) &&
        !between_streets_ &&
        ParseFieldSpecifics(scanner, kBetweenStreetsRe,
                            kBetweenStreetsMatchType, between_streets_patterns,
                            &between_streets_,
                            {log_manager_, "kBetweenStreetsRe"})) {
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

  // Record success if the house number and at least one of the other
  // fields were found because that indicates a structured address form.
  if (house_number_ &&
      (street_name_ || zip_ || overflow_ || overflow_and_landmark_ ||
       between_streets_or_landmark_ || apartment_number_ || between_streets_)) {
    // Keep this in sync with the corresponding if-statement in
    // AddressField::ParseAddress to prevent repetitive work.
    return true;
  }
  if (street_location_ &&
      (apartment_number_ || overflow_ || overflow_and_landmark_ ||
       between_streets_or_landmark_ || between_streets_)) {
    // Keep this in sync with the corresponding if-statement in
    // AddressField::ParseAddress to prevent repetitive work.
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
  zip_ = old_zip;
  zip4_ = old_zip4;
  apartment_number_ = old_apartment_number;

  scanner->RewindTo(saved_cursor_position);
  return false;
}

bool AddressField::ParseAddress(AutofillScanner* scanner,
                                const GeoIpCountryCode& client_country,
                                const LanguageCode& page_language,
                                PatternSource pattern_source) {
  // The following if-statements ensure in particular that we don't try to parse
  // a form as an address-line 1, 2, 3 form because we have collected enough
  // evidence that the current form is a structured form. If structured form
  // fields are missing, they will be discovered later via
  // AddressField::ParseAddressField.
  if (house_number_ &&
      (street_name_ || zip_ || overflow_ || overflow_and_landmark_ ||
       between_streets_or_landmark_ || apartment_number_ || between_streets_)) {
    return false;
  }
  if (street_location_ &&
      (apartment_number_ || overflow_ || overflow_and_landmark_ ||
       between_streets_or_landmark_ || between_streets_)) {
    return false;
  }
  // Do not inline these calls: After passing an address field sequence, there
  // might be an additional address line 2 to parse afterwards.
  bool has_field_sequence = ParseAddressFieldSequence(
      scanner, client_country, page_language, pattern_source);
  if (base::FeatureList::IsEnabled(
          features::kAutofillStructuredFieldsDisableAddressLines) &&
      has_field_sequence) {
    return true;
  }
  bool has_address_lines =
      ParseAddressLines(scanner, page_language, pattern_source);
  return has_field_sequence || has_address_lines;
}

bool AddressField::ParseAddressLines(AutofillScanner* scanner,
                                     const LanguageCode& page_language,
                                     PatternSource pattern_source) {
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

  std::u16string pattern = kAddressLine1Re;
  std::u16string label_pattern = kAddressLine1LabelRe;

  base::span<const MatchPatternRef> address_line1_patterns =
      GetMatchPatterns("ADDRESS_LINE_1", page_language, pattern_source);

  // TODO(crbug.com/1121990): Remove duplicate calls when launching
  // AutofillParsingPatternProvider. The old code calls ParseFieldSpecifics()
  // for two different patterns, |pattern| and |label_pattern|. The new code
  // handles both patterns at once in the |address_line1_patterns|.
  // Address line 1 is skipped if a |street_name_|, |house_number_| combination
  // is present.
  if (!(street_name_ && house_number_) &&
      !ParseFieldSpecifics(scanner, pattern,
                           kDefaultMatchParamsWith<MatchFieldType::kSearch>,
                           address_line1_patterns, &address1_,
                           {log_manager_, "kAddressLine1Re"}) &&
      !ParseFieldSpecifics(
          scanner, label_pattern,
          MatchParams({MatchAttribute::kLabel},
                      {MatchFieldType::kSearch, MatchFieldType::kText}),
          address_line1_patterns, &address1_,
          {log_manager_, "kAddressLine1LabelRe"}) &&
      !ParseFieldSpecifics(scanner, pattern,
                           kDefaultMatchParamsWith<MatchFieldType::kSearch,
                                                   MatchFieldType::kTextArea>,
                           address_line1_patterns, &street_address_,
                           {log_manager_, "kAddressLine1Re"},
                           [](const MatchingPattern& p) {
                             return WithFieldType(p, MatchFieldType::kTextArea);
                           }) &&
      !ParseFieldSpecifics(
          scanner, label_pattern,
          MatchParams({MatchAttribute::kLabel},
                      {MatchFieldType::kSearch, MatchFieldType::kTextArea}),
          address_line1_patterns, &street_address_,
          {log_manager_, "kAddressLine1LabelRe"}, [](const MatchingPattern& p) {
            return WithFieldType(p, MatchFieldType::kTextArea);
          })) {
    return false;
  }

  if (street_address_)
    return true;

  // This code may not pick up pages that have an address field consisting of a
  // sequence of unlabeled address fields. If we need to add this, see
  // discussion on https://codereview.chromium.org/741493003/
  pattern = kAddressLine2Re;
  label_pattern = kAddressLine2LabelRe;

  base::span<const MatchPatternRef> address_line2_patterns =
      GetMatchPatterns("ADDRESS_LINE_2", page_language, pattern_source);

  if (!ParseField(scanner, pattern, address_line2_patterns, &address2_,
                  {log_manager_, "kAddressLine2Re"}) &&
      !ParseFieldSpecifics(
          scanner, label_pattern,
          MatchParams({MatchAttribute::kLabel}, {MatchFieldType::kText}),
          address_line2_patterns, &address2_,
          {log_manager_, "kAddressLine2LabelRe"}))
    return true;

  base::span<const MatchPatternRef> address_line_extra_patterns =
      GetMatchPatterns("ADDRESS_LINE_EXTRA", page_language, pattern_source);

  // Optionally parse address line 3. This uses the same label regexp as
  // address 2 above.
  pattern = kAddressLinesExtraRe;
  if (!ParseField(scanner, pattern, address_line_extra_patterns, &address3_,
                  {log_manager_, "kAddressLinesExtraRe"}) &&
      !ParseFieldSpecifics(
          scanner, label_pattern,
          MatchParams({MatchAttribute::kLabel}, {MatchFieldType::kText}),
          address_line2_patterns, &address3_,
          {log_manager_, "kAddressLine2LabelRe"})) {
    return true;
  }

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  pattern = kAddressLinesExtraRe;
  while (ParseField(scanner, pattern, address_line_extra_patterns, nullptr,
                    {log_manager_, "kAddressLinesExtraRe"})) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressField::ParseZipCode(AutofillScanner* scanner,
                                const LanguageCode& page_language,
                                PatternSource pattern_source) {
  if (zip_)
    return false;

  base::span<const MatchPatternRef> zip_code_patterns =
      GetMatchPatterns("ZIP_CODE", page_language, pattern_source);

  base::span<const MatchPatternRef> four_digit_zip_code_patterns =
      GetMatchPatterns("ZIP_4", page_language, pattern_source);
  if (!ParseFieldSpecifics(scanner, kZipCodeRe, kZipCodeMatchType,
                           zip_code_patterns, &zip_,
                           {log_manager_, "kZipCodeRe"})) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseFieldSpecifics(scanner, kZip4Re, kZipCodeMatchType,
                      four_digit_zip_code_patterns, &zip4_,
                      {log_manager_, "kZip4Re"});
  return true;
}

bool AddressField::ParseCity(AutofillScanner* scanner,
                             const LanguageCode& page_language,
                             PatternSource pattern_source) {
  if (city_)
    return false;

  base::span<const MatchPatternRef> city_patterns =
      GetMatchPatterns("CITY", page_language, pattern_source);
  return ParseFieldSpecifics(scanner, kCityRe, kCityMatchType, city_patterns,
                             &city_, {log_manager_, "kCityRe"});
}

bool AddressField::ParseState(AutofillScanner* scanner,
                              const LanguageCode& page_language,
                              PatternSource pattern_source) {
  if (state_)
    return false;

  base::span<const MatchPatternRef> patterns_state =
      GetMatchPatterns("STATE", page_language, pattern_source);
  return ParseFieldSpecifics(scanner, kStateRe, kStateMatchType, patterns_state,
                             &state_, {log_manager_, "kStateRe"});
}

// static
AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelSeparately(
    AutofillScanner* scanner,
    const std::u16string& pattern,
    MatchParams match_type,
    base::span<const MatchPatternRef> patterns,
    raw_ptr<AutofillField>* match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  raw_ptr<AutofillField> cur_match = nullptr;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name = ParseFieldSpecifics(
      scanner, pattern, WithoutAttribute(match_type, MatchAttribute::kLabel),
      patterns, &cur_match, logging, [](const MatchingPattern& p) {
        return WithoutAttribute(p, MatchAttribute::kLabel);
      });
  scanner->RewindTo(saved_cursor);
  bool parsed_label = ParseFieldSpecifics(
      scanner, pattern, WithoutAttribute(match_type, MatchAttribute::kName),
      patterns, &cur_match, logging, [](const MatchingPattern& p) {
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

bool AddressField::ParseAddressField(AutofillScanner* scanner,
                                     const GeoIpCountryCode& client_country,
                                     const LanguageCode& page_language,
                                     PatternSource pattern_source) {
  // The |scanner| is not pointing at a field.
  if (scanner->IsEnd())
    return false;

  // Check for matches to both the name and the label.
  ParseNameLabelResult dependent_locality_result =
      ParseNameAndLabelForDependentLocality(scanner, page_language,
                                            pattern_source);
  if (dependent_locality_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult city_result =
      ParseNameAndLabelForCity(scanner, page_language, pattern_source);
  if (city_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult state_result =
      ParseNameAndLabelForState(scanner, page_language, pattern_source);
  if (state_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult country_result =
      ParseNameAndLabelForCountry(scanner, page_language, pattern_source);
  if (country_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult between_streets_or_landmark_result =
      ParseNameAndLabelForBetweenStreetsOrLandmark(scanner, page_language,
                                                   pattern_source);
  if (between_streets_or_landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult overflow_and_landmark_result =
      ParseNameAndLabelForOverflowAndLandmark(scanner, page_language,
                                              pattern_source);
  if (overflow_and_landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  // This line bears some potential problems. A field with
  // <label>Complemento e referência: <input name="complemento"><label>
  // will match the "overflow" in the label and name. The function would
  // exit here. Instead of later recognizing that "Complemento e referência"
  // points to a different type.
  ParseNameLabelResult overflow_result =
      ParseNameAndLabelForOverflow(scanner, page_language, pattern_source);
  if (overflow_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult landmark_result =
      ParseNameAndLabelForLandmark(scanner, page_language, pattern_source);
  if (landmark_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult between_streets_result =
      ParseNameAndLabelForBetweenStreets(scanner, page_language,
                                         pattern_source);
  if (between_streets_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult admin_level2_result =
      ParseNameAndLabelForAdminLevel2(scanner, page_language, pattern_source);
  if (admin_level2_result == RESULT_MATCH_NAME_LABEL) {
    return true;
  }
  ParseNameLabelResult zip_result =
      ParseNameAndLabelForZipCode(scanner, page_language, pattern_source);
  if (zip_result == RESULT_MATCH_NAME_LABEL)
    return true;

  int num_of_matches = 0;
  for (const auto result :
       {dependent_locality_result, city_result, state_result, country_result,
        zip_result, landmark_result, between_streets_result,
        admin_level2_result, between_streets_or_landmark_result,
        overflow_and_landmark_result, overflow_result}) {
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
    if (admin_level2_result != RESULT_MATCH_NONE) {
      return SetFieldAndAdvanceCursor(scanner, &admin_level2_);
    }
    if (zip_result != RESULT_MATCH_NONE)
      return ParseZipCode(scanner, page_language, pattern_source);
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
  // even though that should be mapped to a "Cuidad").
  if (page_language == LanguageCode("tr") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLabelPrecedenceForTurkishAddresses)) {
    std::swap(results_to_match[0], results_to_match[1]);
  } else if (client_country == GeoIpCountryCode("MX") &&
             base::FeatureList::IsEnabled(
                 features::kAutofillPreferLabelsInSomeCountries)) {
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
    if (admin_level2_result == result) {
      return SetFieldAndAdvanceCursor(scanner, &admin_level2_);
    }
    if (zip_result == result)
      return ParseZipCode(scanner, page_language, pattern_source);
  }

  return false;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForZipCode(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (zip_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> zip_code_patterns =
      GetMatchPatterns("ZIP_CODE", page_language, pattern_source);

  base::span<const MatchPatternRef> four_digit_zip_code_patterns =
      GetMatchPatterns("ZIP_4", page_language, pattern_source);

  ParseNameLabelResult result = ParseNameAndLabelSeparately(
      scanner, kZipCodeRe, kZipCodeMatchType, zip_code_patterns, &zip_,
      {log_manager_, "kZipCodeRe"});

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(scanner, page_language, pattern_source);
  if (found_non_zip4)
    city_ = nullptr;
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(scanner, page_language, pattern_source);
    if (found_non_zip4)
      state_ = nullptr;
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseFieldSpecifics(scanner, kZip4Re, kZipCodeMatchType,
                        four_digit_zip_code_patterns, &zip4_,
                        {log_manager_, "kZip4Re"});
  }
  return result;
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForDependentLocality(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  const bool is_enabled_dependent_locality_parsing =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableDependentLocalityParsing);
  // TODO(crbug.com/1157405) Remove feature check when launched.
  if (dependent_locality_ || !is_enabled_dependent_locality_parsing)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> dependent_locality_patterns =
      GetMatchPatterns("ADDRESS_HOME_DEPENDENT_LOCALITY", page_language,
                       pattern_source);
  return ParseNameAndLabelSeparately(
      scanner, kDependentLocalityRe, kDependentLocalityMatchType,
      dependent_locality_patterns, &dependent_locality_,
      {log_manager_, "kDependentLocalityRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCity(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (city_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> city_patterns =
      GetMatchPatterns("CITY", page_language, pattern_source);
  return ParseNameAndLabelSeparately(scanner, kCityRe, kCityMatchType,
                                     city_patterns, &city_,
                                     {log_manager_, "kCityRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForState(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (state_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> patterns_state =
      GetMatchPatterns("STATE", page_language, pattern_source);
  return ParseNameAndLabelSeparately(scanner, kStateRe, kStateMatchType,
                                     patterns_state, &state_,
                                     {log_manager_, "kStateRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCountry(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (country_)
    return RESULT_MATCH_NONE;

  base::span<const MatchPatternRef> country_patterns =
      GetMatchPatterns("COUNTRY", page_language, pattern_source);

  base::span<const MatchPatternRef> country_location_patterns =
      GetMatchPatterns("COUNTRY_LOCATION", page_language, pattern_source);

  ParseNameLabelResult country_result = ParseNameAndLabelSeparately(
      scanner, kCountryRe,
      kDefaultMatchParamsWith<MatchFieldType::kSelect, MatchFieldType::kSearch>,
      country_patterns, &country_, {log_manager_, "kCountryRe"});
  if (country_result != RESULT_MATCH_NONE)
    return country_result;

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  return ParseNameAndLabelSeparately(
      scanner, kCountryLocationRe,
      MatchParams({MatchAttribute::kLabel, MatchAttribute::kName},
                  {MatchFieldType::kSelect, MatchFieldType::kSearch}),
      country_location_patterns, &country_,
      {log_manager_, "kCountryLocationRe"});
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForBetweenStreetsOrLandmark(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (between_streets_or_landmark_ || landmark_ || between_streets_ ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreetsOrLandmark)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> between_streets_or_landmark_patterns =
      GetMatchPatterns("BETWEEN_STREETS_OR_LANDMARK", page_language,
                       pattern_source);
  auto result = ParseNameAndLabelSeparately(
      scanner, kBetweenStreetsOrLandmarkRe, kBetweenStreetsOrLandmarkMatchType,
      between_streets_or_landmark_patterns, &between_streets_or_landmark_,
      {log_manager_, "kBetweenStreetsOrLandmarkRe"});

  return result;
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForOverflowAndLandmark(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  //  TODO(crbug.com/1441904) Remove feature check when launched.
  if (overflow_and_landmark_ || overflow_ ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflowAndLandmark)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> overflow_and_landmark_patterns =
      GetMatchPatterns("OVERFLOW_AND_LANDMARK", page_language, pattern_source);
  auto result = ParseNameAndLabelSeparately(
      scanner, kOverflowAndLandmarkRe, kOverflowAndLandmarkMatchType,
      overflow_and_landmark_patterns, &overflow_and_landmark_,
      {log_manager_, "kOverflowAndLandmarkRe"});
  return result;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForOverflow(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  // TODO(crbug.com/1441904) Remove feature check when launched.
  if (overflow_and_landmark_ || overflow_ ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflow)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> overflow_patterns =
      GetMatchPatterns("OVERFLOW", page_language, pattern_source);
  return ParseNameAndLabelSeparately(scanner, kOverflowRe, kOverflowMatchType,
                                     overflow_patterns, &overflow_,
                                     {log_manager_, "kOverflowRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForLandmark(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  // TODO(crbug.com/1441904) Remove feature check when launched.
  if (landmark_ || !base::FeatureList::IsEnabled(
                       features::kAutofillEnableSupportForLandmark)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> landmark_patterns =
      GetMatchPatterns("LANDMARK", page_language, pattern_source);
  return ParseNameAndLabelSeparately(scanner, kLandmarkRe, kLandmarkMatchType,
                                     landmark_patterns, &landmark_,
                                     {log_manager_, "kLandmarkRe"});
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForBetweenStreets(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  // TODO(crbug.com/1441904) Remove feature check when launched.
  if (between_streets_ ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> between_streets_patterns =
      GetMatchPatterns("BETWEEN_STREETS", page_language, pattern_source);
  return ParseNameAndLabelSeparately(
      scanner, kBetweenStreetsRe, kBetweenStreetsMatchType,
      between_streets_patterns, &between_streets_,
      {log_manager_, "kBetweenStreetsRe"});
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForAdminLevel2(AutofillScanner* scanner,
                                              const LanguageCode& page_language,
                                              PatternSource pattern_source) {
  // TODO(crbug.com/1441904) Remove feature check when launched.
  if (admin_level2_ || !base::FeatureList::IsEnabled(
                           features::kAutofillEnableSupportForAdminLevel2)) {
    return RESULT_MATCH_NONE;
  }

  base::span<const MatchPatternRef> admin_level2_patterns =
      GetMatchPatterns("ADMIN_LEVEL_2", page_language, pattern_source);
  return ParseNameAndLabelSeparately(
      scanner, kAdminLevel2Re, kAdminLevel2MatchType, admin_level2_patterns,
      &admin_level2_, {log_manager_, "kAdminLevel2Re"});
}

}  // namespace autofill
