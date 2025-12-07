// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillScanner;

class AddressFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner& scanner);

  // Returns whether a stand-alone zip field is supported for `client_country`.
  // In some countries that's a prevalent UI (the user is first asked to enter
  // their zip code and then a lot of information is derived). This is not
  // enabled for all countries because there is a certain risk of false positive
  // classifications. We may reevaluate that decision in the future.
  static bool IsStandaloneZipSupported(const GeoIpCountryCode& client_country);

  static std::unique_ptr<FormFieldParser> ParseStandaloneZip(
      ParsingContext& context,
      AutofillScanner& scanner);

  ~AddressFieldParser() override;

  AddressFieldParser(const AddressFieldParser&) = delete;
  AddressFieldParser& operator=(const AddressFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  // When parsing a field's label and name separately with a given pattern:
  enum ParseNameLabelResult {
    RESULT_MATCH_NONE,       // No match with the label or name.
    RESULT_MATCH_LABEL,      // Only the label matches the pattern.
    RESULT_MATCH_NAME,       // Only the name matches the pattern.
    RESULT_MATCH_NAME_LABEL  // Name and label both match the pattern.
  };

  AddressFieldParser();

  bool ParseCompany(ParsingContext& context, AutofillScanner& scanner);

  bool ParseAddress(ParsingContext& context, AutofillScanner& scanner);

  bool ParseAddressFieldSequence(ParsingContext& context,
                                 AutofillScanner& scanner);

  bool ParseAddressLines(ParsingContext& context, AutofillScanner& scanner);

  bool ParseZipCode(ParsingContext& context, AutofillScanner& scanner);

  bool ParseZipCodeSuffix(ParsingContext& context, AutofillScanner& scanner);

  bool ParseCity(ParsingContext& context, AutofillScanner& scanner);

  bool ParseState(ParsingContext& context, AutofillScanner& scanner);

  bool ParseStreetLocation(ParsingContext& context, AutofillScanner& scanner);

  bool ParseDependentLocality(ParsingContext& context,
                              AutofillScanner& scanner);

  bool ParseLandmark(ParsingContext& context, AutofillScanner& scanner);

  bool ParseStreetName(ParsingContext& context, AutofillScanner& scanner);

  bool ParseHouseNumber(ParsingContext& context, AutofillScanner& scanner);

  bool ParseApartmentNumber(ParsingContext& context, AutofillScanner& scanner);

  bool ParseBetweenStreetsOrLandmark(ParsingContext& context,
                                     AutofillScanner& scanner);

  bool ParseOverflowAndLandmark(ParsingContext& context,
                                AutofillScanner& scanner);

  bool ParseOverflow(ParsingContext& context, AutofillScanner& scanner);

  bool ParseBetweenStreetsFields(ParsingContext& context,
                                 AutofillScanner& scanner);

  // Parses the current field pointed to by `scanner`, if it exists, and tries
  // to determine if the field's type corresponds to one of the following:
  // dependent locality, city, state, country, zip, landmark, between streets,
  // admin level 2 or none of those.
  bool ParseAddressField(ParsingContext& context, AutofillScanner& scanner);

  // Starting from the current field pointed to by `scanner`, tries to parse
  // sequence of house number followed by either a street name, an apartment
  // number, or both. Rewind the cursor if the sequence could not be parsed. It
  // is currently only supported in NL.
  bool ParseHouseNumAptNumStreetNameSequence(ParsingContext& context,
                                             AutofillScanner& scanner);

  // Parses the current field pointed to by `scanner`, if it exists, and tries
  // to match house_number_and_apt field type.
  bool ParseFieldSpecificsForHouseNumberAndApt(ParsingContext& context,
                                               AutofillScanner& scanner);

  // Like ParseField(), but applies pattern named with `regex_name` against the
  // name and label of the current field separately. If the return value is
  // RESULT_MATCH_NAME_LABEL, then `scanner` advances and `match` is filled if
  // it is non-NULL. Otherwise `scanner` does not advance and `match` does not
  // change.
  static ParseNameLabelResult ParseNameAndLabelSeparately(
      ParsingContext& context,
      AutofillScanner& scanner,
      const char* regex_name,
      std::optional<FieldAndMatchInfo>* match);

  // The following applies to all `ParseNameAndLabelForX()` functions:
  // Run matches on the name and label separately. If the return result is
  // RESULT_MATCH_NAME_LABEL, then `scanner` advances and the field is set.
  // Otherwise `scanner` rewinds and the field is cleared.
  ParseNameLabelResult ParseNameAndLabelForZipCode(ParsingContext& context,
                                                   AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForZipCodeSuffix(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForDependentLocality(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForCity(ParsingContext& context,
                                                AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForCountry(ParsingContext& context,
                                                   AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForLandmark(ParsingContext& context,
                                                    AutofillScanner& scanner);

  // Used in `ParseAddressField()` to parse `ADDRESS_HOME_STREET_LOCATION`
  // field. Currently only supported in India. Uses India specific regex
  // patterns.
  ParseNameLabelResult ParseNameAndLabelForStreetLocation(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForBetweenStreets(
      ParsingContext& context,
      AutofillScanner& scanner);

  // Run matches on the name and label for a field and sets
  // `between_streets_line_1_` and `between_streets_line_2_` respectively if a
  // match is found.
  ParseNameLabelResult ParseNameAndLabelForBetweenStreetsLines12(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForAdminLevel2(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForBetweenStreetsOrLandmark(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForOverflowAndLandmark(
      ParsingContext& context,
      AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForOverflow(ParsingContext& context,
                                                    AutofillScanner& scanner);

  ParseNameLabelResult ParseNameAndLabelForState(ParsingContext& context,
                                                 AutofillScanner& scanner);

  bool SetFieldAndAdvanceCursor(
      AutofillScanner& scanner,
      ParseNameLabelResult parse_result,
      std::optional<FormFieldParser::FieldAndMatchInfo>* match);

  // Return true if the form being parsed shows an indication of being a
  // structured address form. `country_code` is currently only used for India
  // where the `street_location_`, `dependent_locality_` and `landmark_` fields
  // are required.
  bool PossiblyAStructuredAddressForm(GeoIpCountryCode country_code) const;

  std::optional<FieldAndMatchInfo> company_;
  std::optional<FieldAndMatchInfo> street_location_;
  std::optional<FieldAndMatchInfo> street_name_;
  std::optional<FieldAndMatchInfo> house_number_;
  std::optional<FieldAndMatchInfo> address1_;
  std::optional<FieldAndMatchInfo> address2_;
  std::optional<FieldAndMatchInfo> address3_;
  std::optional<FieldAndMatchInfo> street_address_;
  std::optional<FieldAndMatchInfo> apartment_number_;
  std::optional<FieldAndMatchInfo> dependent_locality_;
  std::optional<FieldAndMatchInfo> city_;
  std::optional<FieldAndMatchInfo> state_;
  std::optional<FieldAndMatchInfo> zip_;
  std::optional<FieldAndMatchInfo> zip_suffix_;
  std::optional<FieldAndMatchInfo> country_;
  std::optional<FieldAndMatchInfo> landmark_;
  std::optional<FieldAndMatchInfo> between_streets_;
  std::optional<FieldAndMatchInfo> between_streets_line_1_;
  std::optional<FieldAndMatchInfo> between_streets_line_2_;
  std::optional<FieldAndMatchInfo> admin_level2_;
  std::optional<FieldAndMatchInfo> between_streets_or_landmark_;
  std::optional<FieldAndMatchInfo> overflow_and_landmark_;
  std::optional<FieldAndMatchInfo> overflow_;
  std::optional<FieldAndMatchInfo> house_number_and_apt_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_H_
