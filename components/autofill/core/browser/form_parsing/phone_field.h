// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_

#include <stddef.h>

#include <memory>
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

  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ParseOneLinePhone);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ParseTwoLinePhone);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumber);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, CountryAndCityAndPhoneNumber);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest,
                           CountryAndCityAndPhoneNumberWithLongerMaxLength);

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
                              const RegExLogging& logging);

  // FIELD_PHONE is always present; holds suffix if prefix is present.
  // The rest could be NULL.
  AutofillField* parsed_phone_fields_[FIELD_MAX];

  DISALLOW_COPY_AND_ASSIGN(PhoneField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PHONE_FIELD_H_
