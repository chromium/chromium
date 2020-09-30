// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/phone_field.h"

#include <string.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {
namespace {

// Minimum limit on the number of the options of the select field for
// determining the field to be of |PHONE_HOME_COUNTRY_CODE| type.
constexpr int kMinSelectOptionsForCountryCode = 5;

// Maximum limit on the number of the options of the select field for
// determining the field to be of |PHONE_HOME_COUNTRY_CODE| type.
// Currently, there are approximately 250 countries that have been assigned a
// phone country code, therefore, 275 is taken as the upper bound.
constexpr int kMaxSelectOptionsForCountryCode = 275;

// Minimum percentage of options in select field that should look like a
// country code in order to classify the field as a |PHONE_HOME_COUNTRY_CODE|.
constexpr int kMinCandidatePercentageForCountryCode = 90;

// If a <select> element has <= |kHeuristicThresholdForCountryCode| options,
// all or all-but-one need to look like country code options. Otherwise,
// |kMinCandidatePercentageForCountryCode| is used to check for a fraction
// of country code like options.
constexpr int kHeuristicThresholdForCountryCode = 10;

// This string includes all area code separators, including NoText.
std::string GetAreaRegex() {
  std::string area_code = kAreaCodeRe;
  area_code.append("|");  // Regexp separator.
  area_code.append(kAreaCodeNotextRe);
  return area_code;
}

}  // namespace

PhoneField::~PhoneField() {}

// Phone field grammars - first matched grammar will be parsed. Grammars are
// separated by { REGEX_SEPARATOR, FIELD_NONE, 0 }. Suffix and extension are
// parsed separately unless they are necessary parts of the match.
// The following notation is used to describe the patterns:
// <cc> - country code field.
// <ac> - area code field.
// <phone> - phone or prefix.
// <suffix> - suffix.
// <ext> - extension.
// :N means field is limited to N characters, otherwise it is unlimited.
// (pattern <field>)? means pattern is optional and matched separately.
const PhoneField::Parser PhoneField::kPhoneFieldGrammars[] = {
    // Country code: <cc> Area Code: <ac> Phone: <phone> (- <suffix>
    // (Ext: <ext>)?)?
    {REGEX_COUNTRY, FIELD_COUNTRY_CODE, 0},
    {REGEX_AREA, FIELD_AREA_CODE, 0},
    {REGEX_PHONE, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // \( <ac> \) <phone>:3 <suffix>:4 (Ext: <ext>)?
    {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 3},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
    {REGEX_PHONE, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4 (Ext: <ext>)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    {REGEX_PHONE, FIELD_AREA_CODE, 3},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
    {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4 (Ext: <ext>)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
    {REGEX_PHONE, FIELD_AREA_CODE, 3},
    {REGEX_PHONE, FIELD_PHONE, 3},
    {REGEX_PHONE, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Area Code: <ac> Phone: <phone> (- <suffix> (Ext: <ext>)?)?
    {REGEX_AREA, FIELD_AREA_CODE, 0},
    {REGEX_PHONE, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <ac> <phone>:3 <suffix>:4 (Ext: <ext>)?
    {REGEX_PHONE, FIELD_AREA_CODE, 0},
    {REGEX_PHONE, FIELD_PHONE, 3},
    {REGEX_PHONE, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc> \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc> - <ac> - <phone> - <suffix> (Ext: <ext>)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Area code: <ac>:3 Prefix: <prefix>:3 Suffix: <suffix>:4 (Ext: <ext>)?
    {REGEX_AREA, FIELD_AREA_CODE, 3},
    {REGEX_PREFIX, FIELD_PHONE, 3},
    {REGEX_SUFFIX, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <ac> Prefix: <phone> Suffix: <suffix> (Ext: <ext>)?
    {REGEX_PHONE, FIELD_AREA_CODE, 0},
    {REGEX_PREFIX, FIELD_PHONE, 0},
    {REGEX_SUFFIX, FIELD_SUFFIX, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <ac> - <phone>:3 - <suffix>:4 (Ext: <ext>)?
    {REGEX_PHONE, FIELD_AREA_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
    {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc> - <ac> - <phone> (Ext: <ext>)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
    {REGEX_SUFFIX_SEPARATOR, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <ac> - <phone> (Ext: <ext>)?
    {REGEX_AREA, FIELD_AREA_CODE, 0},
    {REGEX_PHONE, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <cc>:3 - <phone>:10 (Ext: <ext>)?
    {REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
    {REGEX_PHONE, FIELD_PHONE, 14},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Ext: <ext>
    {REGEX_EXTENSION, FIELD_EXTENSION, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
    // Phone: <phone> (Ext: <ext>)?
    {REGEX_PHONE, FIELD_PHONE, 0},
    {REGEX_SEPARATOR, FIELD_NONE, 0},
};

// static
bool PhoneField::LikelyAugmentedPhoneCountryCode(
    AutofillScanner* scanner,
    AutofillField** matched_field) {
  // If the experiment |kAutofillEnableAugmentedPhoneCountryCode| is not
  // enabled, return false.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableAugmentedPhoneCountryCode))
    return false;

  AutofillField* field = scanner->Cursor();

  // Return false if the field is not a selection box.
  if (!MatchesFormControlType(field->form_control_type, MATCH_SELECT))
    return false;

  // If the number of the options is less than the minimum limit or more than
  // the maximum limit, return false.
  if (field->option_contents.size() < kMinSelectOptionsForCountryCode ||
      field->option_contents.size() >= kMaxSelectOptionsForCountryCode)
    return false;

  // |total_covered_options| stores the count of the options that are
  // compared with the regex.
  int total_num_options = static_cast<int>(field->option_contents.size());

  // |total_positive_options| stores the count of the options that match the
  // regex.
  int total_positive_options = 0;

  for (const auto& option : field->option_contents) {
    if (MatchesPattern(option,
                       base::ASCIIToUTF16(kAugmentedPhoneCountryCodeRe)))
      total_positive_options++;
  }

  // If the number of the options compared is less or equal to
  // |kHeuristicThresholdForCountryCode|, then either all the options or all
  // options but one should match the regex.
  if (total_num_options <= kHeuristicThresholdForCountryCode &&
      total_positive_options + 1 < total_num_options)
    return false;

  // If the number of the options compared is more than
  // |kHeuristicThresholdForCountryCode|,
  // |kMinCandidatePercentageForCountryCode|% of the options should match the
  // regex.
  if (total_num_options > kHeuristicThresholdForCountryCode &&
      total_positive_options * 100 <
          total_num_options * kMinCandidatePercentageForCountryCode)
    return false;

  // Assign the |matched_field| and advance the cursor.
  if (matched_field)
    *matched_field = field;
  scanner->Advance();
  return true;
}

// static
std::unique_ptr<FormField> PhoneField::Parse(AutofillScanner* scanner,
                                             const std::string& page_language,
                                             LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  size_t start_cursor = scanner->SaveCursor();

  // The form owns the following variables, so they should not be deleted.
  AutofillField* parsed_fields[FIELD_MAX];

  for (size_t i = 0; i < base::size(kPhoneFieldGrammars); ++i) {
    memset(parsed_fields, 0, sizeof(parsed_fields));
    size_t saved_cursor = scanner->SaveCursor();

    // Attempt to parse according to the next grammar.
    for (; i < base::size(kPhoneFieldGrammars) &&
           kPhoneFieldGrammars[i].regex != REGEX_SEPARATOR;
         ++i) {
      const bool is_country_code_field =
          kPhoneFieldGrammars[i].phone_part == FIELD_COUNTRY_CODE;

      // The field length comparison with |kPhoneFieldGrammars[i].max_size| is
      // not required in case of the selection boxes that are of phone country
      // code type.
      if (is_country_code_field &&
          LikelyAugmentedPhoneCountryCode(scanner,
                                          &parsed_fields[FIELD_COUNTRY_CODE]))
        continue;

      if (!ParsePhoneField(
              scanner, GetRegExp(kPhoneFieldGrammars[i].regex),
              &parsed_fields[kPhoneFieldGrammars[i].phone_part],
              {log_manager, GetRegExpName(kPhoneFieldGrammars[i].regex)},
              is_country_code_field))
        break;
      if (kPhoneFieldGrammars[i].max_size &&
          (!parsed_fields[kPhoneFieldGrammars[i].phone_part]->max_length ||
           kPhoneFieldGrammars[i].max_size <
               parsed_fields[kPhoneFieldGrammars[i].phone_part]->max_length)) {
        break;
      }
    }

    if (i >= base::size(kPhoneFieldGrammars)) {
      scanner->RewindTo(saved_cursor);
      return nullptr;  // Parsing failed.
    }
    if (kPhoneFieldGrammars[i].regex == REGEX_SEPARATOR)
      break;  // Parsing succeeded.

    // Proceed to the next grammar.
    do {
      ++i;
    } while (i < base::size(kPhoneFieldGrammars) &&
             kPhoneFieldGrammars[i].regex != REGEX_SEPARATOR);

    scanner->RewindTo(saved_cursor);
    if (i + 1 == base::size(kPhoneFieldGrammars)) {
      return nullptr;  // Tried through all the possibilities - did not match.
    }
  }

  if (!parsed_fields[FIELD_PHONE]) {
    scanner->RewindTo(start_cursor);
    return nullptr;
  }

  std::unique_ptr<PhoneField> phone_field(new PhoneField);
  for (int i = 0; i < FIELD_MAX; ++i)
    phone_field->parsed_phone_fields_[i] = parsed_fields[i];

  // Look for optional fields.

  // Look for a third text box.
  if (!phone_field->parsed_phone_fields_[FIELD_SUFFIX]) {
    if (!ParsePhoneField(scanner, kPhoneSuffixRe,
                         &phone_field->parsed_phone_fields_[FIELD_SUFFIX],
                         {log_manager, "kPhoneSuffixRe"},
                         /*is_country_code_field=*/false)) {
      ParsePhoneField(scanner, kPhoneSuffixSeparatorRe,
                      &phone_field->parsed_phone_fields_[FIELD_SUFFIX],
                      {log_manager, "kPhoneSuffixSeparatorRe"},
                      /*is_country_code_field=*/false);
    }
  }

  // Now look for an extension.
  // The extension is not actually used, so this just eats the field so other
  // parsers do not mistaken it for something else.
  ParsePhoneField(scanner, kPhoneExtensionRe,
                  &phone_field->parsed_phone_fields_[FIELD_EXTENSION],
                  {log_manager, "kPhoneExtensionRe"},
                  /*is_country_code_field=*/false);

  return std::move(phone_field);
}

void PhoneField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  DCHECK(parsed_phone_fields_[FIELD_PHONE]);  // Phone was correctly parsed.

  if ((parsed_phone_fields_[FIELD_COUNTRY_CODE]) ||
      (parsed_phone_fields_[FIELD_AREA_CODE]) ||
      (parsed_phone_fields_[FIELD_SUFFIX])) {
    if (parsed_phone_fields_[FIELD_COUNTRY_CODE]) {
      AddClassification(parsed_phone_fields_[FIELD_COUNTRY_CODE],
                        PHONE_HOME_COUNTRY_CODE, kBasePhoneParserScore,
                        field_candidates);
    }

    ServerFieldType field_number_type = PHONE_HOME_NUMBER;
    if (parsed_phone_fields_[FIELD_AREA_CODE]) {
      AddClassification(parsed_phone_fields_[FIELD_AREA_CODE],
                        PHONE_HOME_CITY_CODE, kBasePhoneParserScore,
                        field_candidates);
    } else if (parsed_phone_fields_[FIELD_COUNTRY_CODE]) {
      // Only if we can find country code without city code, it means the phone
      // number include city code.
      field_number_type = PHONE_HOME_CITY_AND_NUMBER;
    }
    // We tag the prefix as PHONE_HOME_NUMBER, then when filling the form
    // we fill only the prefix depending on the size of the input field.
    AddClassification(parsed_phone_fields_[FIELD_PHONE], field_number_type,
                      kBasePhoneParserScore, field_candidates);
    // We tag the suffix as PHONE_HOME_NUMBER, then when filling the form
    // we fill only the suffix depending on the size of the input field.
    if (parsed_phone_fields_[FIELD_SUFFIX]) {
      AddClassification(parsed_phone_fields_[FIELD_SUFFIX], PHONE_HOME_NUMBER,
                        kBasePhoneParserScore, field_candidates);
    }
  } else {
    AddClassification(parsed_phone_fields_[FIELD_PHONE],
                      PHONE_HOME_WHOLE_NUMBER, kBasePhoneParserScore,
                      field_candidates);
  }

  if (parsed_phone_fields_[FIELD_EXTENSION]) {
    AddClassification(parsed_phone_fields_[FIELD_EXTENSION],
                      PHONE_HOME_EXTENSION, kBasePhoneParserScore,
                      field_candidates);
  }
}

PhoneField::PhoneField() {
  memset(parsed_phone_fields_, 0, sizeof(parsed_phone_fields_));
}

// static
std::string PhoneField::GetRegExp(RegexType regex_id) {
  switch (regex_id) {
    case REGEX_COUNTRY:
      return kCountryCodeRe;
    case REGEX_AREA:
      return GetAreaRegex();
    case REGEX_AREA_NOTEXT:
      return kAreaCodeNotextRe;
    case REGEX_PHONE:
      return kPhoneRe;
    case REGEX_PREFIX_SEPARATOR:
      return kPhonePrefixSeparatorRe;
    case REGEX_PREFIX:
      return kPhonePrefixRe;
    case REGEX_SUFFIX_SEPARATOR:
      return kPhoneSuffixSeparatorRe;
    case REGEX_SUFFIX:
      return kPhoneSuffixRe;
    case REGEX_EXTENSION:
      return kPhoneExtensionRe;
    default:
      NOTREACHED();
      break;
  }
  return std::string();
}

// static
const char* PhoneField::GetRegExpName(RegexType regex_id) {
  switch (regex_id) {
    case REGEX_COUNTRY:
      return "kCountryCodeRe";
    case REGEX_AREA:
      return "kAreaCodeRe|kAreaCodeNotextRe";
    case REGEX_AREA_NOTEXT:
      return "kAreaCodeNotextRe";
    case REGEX_PHONE:
      return "kPhoneRe";
    case REGEX_PREFIX_SEPARATOR:
      return "kPhonePrefixSeparatorRe";
    case REGEX_PREFIX:
      return "kPhonePrefixRe";
    case REGEX_SUFFIX_SEPARATOR:
      return "kPhoneSuffixSeparatorRe";
    case REGEX_SUFFIX:
      return "kPhoneSuffixRe";
    case REGEX_EXTENSION:
      return "kPhoneExtensionRe";
    default:
      NOTREACHED();
      break;
  }
  return "";
}

// static
bool PhoneField::ParsePhoneField(AutofillScanner* scanner,
                                 const std::string& regex,
                                 AutofillField** field,
                                 const RegExLogging& logging,
                                 const bool is_country_code_field) {
  int match_type = MATCH_DEFAULT | MATCH_TELEPHONE | MATCH_NUMBER;
  // Include the selection boxes too for the matching of the phone country code.
  if (is_country_code_field)
    match_type |= MATCH_SELECT;

  return ParseFieldSpecifics(scanner, base::UTF8ToUTF16(regex), match_type,
                             field, logging);
}

}  // namespace autofill
