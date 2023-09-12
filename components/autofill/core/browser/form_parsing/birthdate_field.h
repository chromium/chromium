// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_BIRTHDATE_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_BIRTHDATE_FIELD_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

// Birthdate fields are currently not filled, but identifying them will help to
// reduce the number of false positive credit card expiration dates.
class BirthdateField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  BirthdateField(const BirthdateField&) = delete;
  BirthdateField& operator=(const BirthdateField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  BirthdateField(const AutofillField* day,
                 const AutofillField* month,
                 const AutofillField* year);

  // Checks if the scanner's current field is a <select> and if its options
  // contains the values [1, `max_value`] in increasing order, possibly after a
  // placeholder. Moreover checks that at most max_options options are present.
  static bool IsSelectWithIncreasingValues(AutofillScanner* scanner,
                                           int max_value,
                                           size_t max_options);

  // Checks if the scanner's current field is a <select> and if all but the
  // first of its options represents a numerical value in [1900, current-year].
  // The first option might contain a placeholder.
  static bool IsLikelyBirthdateYearSelectField(AutofillScanner* scanner);

 private:
  raw_ptr<const AutofillField> day_;
  raw_ptr<const AutofillField> month_;
  raw_ptr<const AutofillField> year_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_BIRTHDATE_FIELD_H_
