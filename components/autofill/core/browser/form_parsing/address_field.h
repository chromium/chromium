// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

class AddressField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);

  AddressField(const AddressField&) = delete;
  AddressField& operator=(const AddressField&) = delete;

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

  explicit AddressField(LogManager* log_manager);

  bool ParseCompany(AutofillScanner* scanner,
                    const LanguageCode& page_language,
                    PatternSource pattern_source);

  bool ParseAddress(AutofillScanner* scanner,
                    const LanguageCode& page_language,
                    PatternSource pattern_source);

  bool ParseAddressFieldSequence(AutofillScanner* scanner,
                                 const LanguageCode& page_language,
                                 PatternSource pattern_source);

  bool ParseAddressLines(AutofillScanner* scanner,
                         const LanguageCode& page_language,
                         PatternSource pattern_source);

  bool ParseCountry(AutofillScanner* scanner,
                    const LanguageCode& page_language,
                    PatternSource pattern_source);

  bool ParseZipCode(AutofillScanner* scanner,
                    const LanguageCode& page_language,
                    PatternSource pattern_source);

  bool ParseDependentLocality(AutofillScanner* scanner,
                              const LanguageCode& page_language,
                              PatternSource pattern_source);

  bool ParseCity(AutofillScanner* scanner,
                 const LanguageCode& page_language,
                 PatternSource pattern_source);

  bool ParseState(AutofillScanner* scanner,
                  const LanguageCode& page_language,
                  PatternSource pattern_source);

  // Parses the current field pointed to by |scanner|, if it exists, and tries
  // to determine if the field's type corresponds to one of the following:
  // dependent locality, city, state, country, zip, or none of those.
  bool ParseDependentLocalityCityStateCountryZipCode(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  // Like ParseFieldSpecifics(), but applies |pattern| against the name and
  // label of the current field separately. If the return value is
  // RESULT_MATCH_NAME_LABEL, then |scanner| advances and |match| is filled if
  // it is non-NULL. Otherwise |scanner| does not advance and |match| does not
  // change.
  ParseNameLabelResult ParseNameAndLabelSeparately(
      AutofillScanner* scanner,
      const std::u16string& pattern,
      MatchParams match_type,
      base::span<const MatchPatternRef> patterns,
      AutofillField** match,
      const RegExLogging& logging);

  // Run matches on the name and label separately. If the return result is
  // RESULT_MATCH_NAME_LABEL, then |scanner| advances and the field is set.
  // Otherwise |scanner| rewinds and the field is cleared.
  ParseNameLabelResult ParseNameAndLabelForZipCode(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  ParseNameLabelResult ParseNameAndLabelForDependentLocality(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  ParseNameLabelResult ParseNameAndLabelForCity(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  ParseNameLabelResult ParseNameAndLabelForCountry(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  ParseNameLabelResult ParseNameAndLabelForState(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source);

  raw_ptr<LogManager> log_manager_;
  AutofillField* company_ = nullptr;
  AutofillField* street_name_ = nullptr;
  AutofillField* house_number_ = nullptr;
  AutofillField* address1_ = nullptr;
  AutofillField* address2_ = nullptr;
  AutofillField* address3_ = nullptr;
  AutofillField* street_address_ = nullptr;
  AutofillField* apartment_number_ = nullptr;
  AutofillField* dependent_locality_ = nullptr;
  AutofillField* city_ = nullptr;
  AutofillField* state_ = nullptr;
  AutofillField* zip_ = nullptr;
  AutofillField* zip4_ = nullptr;  // optional ZIP+4; we don't fill this yet.
  AutofillField* country_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_H_
