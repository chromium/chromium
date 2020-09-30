// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

class AddressField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const std::string& page_language,
                                          LogManager* log_manager);

#if defined(UNIT_TEST)
  // Assign types to the fields for the testing purposes.
  void AddClassificationsForTesting(
      FieldCandidatesMap* field_candidates_for_testing) const {
    AddClassifications(field_candidates_for_testing);
  }
#endif

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  // When parsing a field's label and name separately with a given pattern:
  enum ParseNameLabelResult {
    RESULT_MATCH_NONE,       // No match with the label or name.
    RESULT_MATCH_LABEL,      // Only the label matches the pattern.
    RESULT_MATCH_NAME,       // Only the name matches the pattern.
    RESULT_MATCH_NAME_LABEL  // Name and label both match the pattern.
  };

  static const int kZipCodeMatchType;
  static const int kCityMatchType;
  static const int kStateMatchType;

  explicit AddressField(LogManager* log_manager);

  bool ParseCompany(AutofillScanner* scanner);
  bool ParseAddress(AutofillScanner* scanner);
  bool ParseAddressFieldSequence(AutofillScanner* scanner);
  bool ParseAddressLines(AutofillScanner* scanner);
  bool ParseCountry(AutofillScanner* scanner);
  bool ParseZipCode(AutofillScanner* scanner);
  bool ParseCity(AutofillScanner* scanner);
  bool ParseState(AutofillScanner* scanner);

  // Parses the current field pointed to by |scanner|, if it exists, and tries
  // to figure out whether the field's type: city, state, zip, or none of those.
  // TODO(crbug.com/1073555) Delete this once experiment
  // |kAutofillUseParseCityStateCountryZipCodeInHeuristic| has been launched.
  bool ParseCityStateZipCode(AutofillScanner* scanner);

  // Parses the current field pointed to by |scanner|, if it exists, and tries
  // to figure out whether the field's type: city, state, country, zip, or
  // none of those.
  bool ParseCityStateCountryZipCode(AutofillScanner* scanner);

  // Like ParseFieldSpecifics(), but applies |pattern| against the name and
  // label of the current field separately. If the return value is
  // RESULT_MATCH_NAME_LABEL, then |scanner| advances and |match| is filled if
  // it is non-NULL. Otherwise |scanner| does not advance and |match| does not
  // change.
  ParseNameLabelResult ParseNameAndLabelSeparately(
      AutofillScanner* scanner,
      const base::string16& pattern,
      int match_type,
      AutofillField** match,
      const RegExLogging& logging);

  // Run matches on the name and label separately. If the return result is
  // RESULT_MATCH_NAME_LABEL, then |scanner| advances and the field is set.
  // Otherwise |scanner| rewinds and the field is cleared.
  ParseNameLabelResult ParseNameAndLabelForZipCode(AutofillScanner* scanner);
  ParseNameLabelResult ParseNameAndLabelForCity(AutofillScanner* scanner);
  ParseNameLabelResult ParseNameAndLabelForCountry(AutofillScanner* scanner);
  ParseNameLabelResult ParseNameAndLabelForState(AutofillScanner* scanner);

  LogManager* log_manager_;
  AutofillField* company_ = nullptr;
  AutofillField* street_name_ = nullptr;
  AutofillField* house_number_ = nullptr;
  AutofillField* address1_ = nullptr;
  AutofillField* address2_ = nullptr;
  AutofillField* address3_ = nullptr;
  AutofillField* street_address_ = nullptr;
  AutofillField* city_ = nullptr;
  AutofillField* state_ = nullptr;
  AutofillField* zip_ = nullptr;
  AutofillField* zip4_ = nullptr;  // optional ZIP+4; we don't fill this yet.
  AutofillField* country_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AddressField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_
