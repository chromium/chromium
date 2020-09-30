// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// A phone number in one of the following formats:
// - area code, prefix, suffix
// - area code, number
// - number
class PhoneField : public FormField {
 public:
  ~PhoneField() override;
  PhoneField(const PhoneField&) = delete;
  PhoneField& operator=(const PhoneField&) = delete;

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
  // This is for easy description of the possible parsing paths of the phone
  // fields.
  enum RegexType {
    REGEX_COUNTRY,
    REGEX_AREA,
    REGEX_AREA_NOTEXT,
    REGEX_PHONE,
    REGEX_PREFIX_SEPARATOR,
    REGEX_PREFIX,
    REGEX_SUFFIX_SEPARATOR,
    REGEX_SUFFIX,
    REGEX_EXTENSION,

    // Separates regexps in grammar.
    REGEX_SEPARATOR,
  };

  // Parsed fields.
  enum PhonePart {
    FIELD_NONE = -1,
    FIELD_COUNTRY_CODE,
    FIELD_AREA_CODE,
    FIELD_PHONE,
    FIELD_SUFFIX,
    FIELD_EXTENSION,

    FIELD_MAX,
  };

  struct Parser {
    RegexType regex;       // Field matching reg-ex.
    PhonePart phone_part;  // Index of the field.
    size_t max_size;       // Max size of the field to match. 0 means any.
  };

  static const Parser kPhoneFieldGrammars[];

  PhoneField();

  // Returns the regular expression string corresponding to |regex_id|
  static std::string GetRegExp(RegexType regex_id);

  // Returns the constant name of the regex corresponding to |regex_id|.
  // This is useful for logging purposes.
  static const char* GetRegExpName(RegexType regex_id);

  // Convenient wrapper for ParseFieldSpecifics().
  static bool ParsePhoneField(AutofillScanner* scanner,
                              const std::string& regex,
                              AutofillField** field,
                              const RegExLogging& logging,
                              const bool is_country_code_field);

  // Returns true if |scanner| points to a <select> field that appears to be the
  // phone country code by looking at its option contents.
  // "Augmented" refers to the fact that we are looking for select options that
  // contain not only a country code but also further text like "Germany (+49)".
  static bool LikelyAugmentedPhoneCountryCode(AutofillScanner* scanner,
                                              AutofillField** match);

  // FIELD_PHONE is always present; holds suffix if prefix is present.
  // The rest could be NULL.
  AutofillField* parsed_phone_fields_[FIELD_MAX];
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_
