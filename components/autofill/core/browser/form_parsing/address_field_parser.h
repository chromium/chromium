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
#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

class AddressFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  // Returns whether a stand-alone zip field is supported for `client_country`.
  // In some countries that's a prevalent UI (the user is first asked to enter
  // their zip code and then a lot of information is derived). This is not
  // enabled for all countries because there is a certain risk of false positive
  // classifications. We may reevaluate that decision in the future.
  static bool IsStandaloneZipSupported(const GeoIpCountryCode& client_country);

  static std::unique_ptr<FormFieldParser> ParseStandaloneZip(
      ParsingContext& context,
      AutofillScanner* scanner);

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

  bool ParseCompany(ParsingContext& context, AutofillScanner* scanner);

  bool ParseAddress(ParsingContext& context, AutofillScanner* scanner);

  bool ParseAddressFieldSequence(ParsingContext& context,
                                 AutofillScanner* scanner);

  bool ParseAddressLines(ParsingContext& context, AutofillScanner* scanner);

  bool ParseZipCode(ParsingContext& context, AutofillScanner* scanner);

  bool ParseCity(ParsingContext& context, AutofillScanner* scanner);

  bool ParseState(ParsingContext& context, AutofillScanner* scanner);

  // Parses the current field pointed to by |scanner|, if it exists, and tries
  // to determine if the field's type corresponds to one of the following:
  // dependent locality, city, state, country, zip, landmark, between streets,
  // admin level 2 or none of those.
  bool ParseAddressField(ParsingContext& context, AutofillScanner* scanner);

  // Parses the current field pointed to by |scanner|, if it exists, and tries
  // to match house_number_and_apt field type.
  bool ParseFieldSpecificsForHouseNumberAndApt(ParsingContext& context,
                                               AutofillScanner* scanner);

  // Like ParseField(), but applies `pattern` against the name and
  // label of the current field separately. If the return value is
  // RESULT_MATCH_NAME_LABEL, then `scanner` advances and `match` is filled if
  // it is non-NULL. Otherwise `scanner` does not advance and `match` does not
  // change.
  static ParseNameLabelResult ParseNameAndLabelSeparately(
      ParsingContext& context,
      AutofillScanner* scanner,
      base::span<const MatchPatternRef> patterns,
      raw_ptr<AutofillField>* match,
      const char* regex_name);

  // Run matches on the name and label separately. If the return result is
  // RESULT_MATCH_NAME_LABEL, then |scanner| advances and the field is set.
  // Otherwise |scanner| rewinds and the field is cleared.
  ParseNameLabelResult ParseNameAndLabelForZipCode(ParsingContext& context,
                                                   AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForDependentLocality(
      ParsingContext& context,
      AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForCity(ParsingContext& context,
                                                AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForCountry(ParsingContext& context,
                                                   AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForLandmark(ParsingContext& context,
                                                    AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForBetweenStreets(
      ParsingContext& context,
      AutofillScanner* scanner);

  // Run matches on the name and label for a field and sets
  // `between_streets_line_1_` and `between_streets_line_2_` respectively if a
  // match is found.
  ParseNameLabelResult ParseNameAndLabelForBetweenStreetsLines12(
      ParsingContext& context,
      AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForAdminLevel2(
      ParsingContext& context,
      AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForBetweenStreetsOrLandmark(
      ParsingContext& context,
      AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForOverflowAndLandmark(
      ParsingContext& context,
      AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForOverflow(ParsingContext& context,
                                                    AutofillScanner* scanner);

  ParseNameLabelResult ParseNameAndLabelForState(ParsingContext& context,
                                                 AutofillScanner* scanner);

  // Return true if the form being parsed shows an indication of being a
  // structured address form.
  bool PossiblyAStructuredAddressForm() const;

  raw_ptr<AutofillField> company_ = nullptr;
  raw_ptr<AutofillField> street_location_ = nullptr;
  raw_ptr<AutofillField> street_name_ = nullptr;
  raw_ptr<AutofillField> house_number_ = nullptr;
  raw_ptr<AutofillField> address1_ = nullptr;
  raw_ptr<AutofillField> address2_ = nullptr;
  raw_ptr<AutofillField> address3_ = nullptr;
  raw_ptr<AutofillField> street_address_ = nullptr;
  raw_ptr<AutofillField> apartment_number_ = nullptr;
  raw_ptr<AutofillField> dependent_locality_ = nullptr;
  raw_ptr<AutofillField> city_ = nullptr;
  raw_ptr<AutofillField> state_ = nullptr;
  raw_ptr<AutofillField> zip_ = nullptr;
  raw_ptr<AutofillField> zip4_ =
      nullptr;  // optional ZIP+4; we don't fill this yet.
  raw_ptr<AutofillField> country_ = nullptr;
  raw_ptr<AutofillField> landmark_ = nullptr;
  raw_ptr<AutofillField> between_streets_ = nullptr;
  raw_ptr<AutofillField> between_streets_line_1_ = nullptr;
  raw_ptr<AutofillField> between_streets_line_2_ = nullptr;
  raw_ptr<AutofillField> admin_level2_ = nullptr;
  raw_ptr<AutofillField> between_streets_or_landmark_ = nullptr;
  raw_ptr<AutofillField> overflow_and_landmark_ = nullptr;
  raw_ptr<AutofillField> overflow_ = nullptr;
  raw_ptr<AutofillField> house_number_and_apt_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_H_
