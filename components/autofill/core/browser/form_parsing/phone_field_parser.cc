// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/phone_field_parser.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
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

}  // namespace

PhoneFieldParser::~PhoneFieldParser() = default;

// Phone field grammars - first matched grammar will be parsed. Suffix and
// extension are parsed separately unless they are necessary parts of the match.
// The following notation is used to describe the patterns:
// <cc> - country code field.
// <ac> - area code field.
// TODO(crbug.com/40233246): Add a separate prefix type.
// <phone> - phone or prefix.
// <suffix> - suffix.
// :N means field is limited to N characters, otherwise it is unlimited.
// (pattern <field>)? means pattern is optional and matched separately.
// static
const std::vector<PhoneFieldParser::PhoneGrammar>&
PhoneFieldParser::GetPhoneGrammars() {
  static const base::NoDestructor<std::vector<PhoneGrammar>> grammars({
      // TODO(crbug.com/40233246): Check whether this first rule generates any
      // traffic. If not, we may want to drop it and rewrite the
      // FormFillerTest.FillPhoneNumber unittest.
      // Country code: <cc> Area Code: <ac> Prefix: <phone> Suffix: <suffix>
      {{REGEX_COUNTRY, FIELD_COUNTRY_CODE},
       {REGEX_AREA, FIELD_AREA_CODE},
       {REGEX_PREFIX, FIELD_PHONE},
       {REGEX_SUFFIX, FIELD_SUFFIX}},
      // Country code: <cc> Area Code: <ac> Phone: <phone>
      {{REGEX_COUNTRY, FIELD_COUNTRY_CODE},
       {REGEX_AREA, FIELD_AREA_CODE},
       {REGEX_PHONE, FIELD_PHONE}},
      // The following grammar was removed because, though it looks plausible,
      // it virtually never matched. The comment remains here for documentation
      // purposes.
      // \( <ac> \) <phone>:3 <suffix>:4
      // {{REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 3},
      //  {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
      //  {REGEX_PHONE, FIELD_SUFFIX, 4}},
      //
      // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4
      {{REGEX_PHONE, FIELD_COUNTRY_CODE},
       {REGEX_PHONE, FIELD_AREA_CODE, 3},
       {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
       {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4}},
      // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4
      {{REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
       {REGEX_PHONE, FIELD_AREA_CODE, 3},
       {REGEX_PHONE, FIELD_PHONE, 3},
       {REGEX_PHONE, FIELD_SUFFIX, 4}},
      // Area Code: <ac> Phone: <phone> - <suffix>
      {{REGEX_AREA, FIELD_AREA_CODE},
       {REGEX_PHONE, FIELD_PHONE},
       {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX}},
      // Area Code: <ac> Phone: <phone> Suffix <suffix>
      {{REGEX_AREA, FIELD_AREA_CODE},
       {REGEX_PHONE, FIELD_PHONE},
       {REGEX_SUFFIX, FIELD_SUFFIX}},
      // Area Code: <ac> Phone: <phone>
      {{REGEX_AREA, FIELD_AREA_CODE}, {REGEX_PHONE, FIELD_PHONE}},
      // Phone: <ac> <phone>:3 <suffix>:4
      {{REGEX_PHONE, FIELD_AREA_CODE},
       {REGEX_PHONE, FIELD_PHONE, 3},
       {REGEX_PHONE, FIELD_SUFFIX, 4}},
      // The following grammar was removed because, though it looks plausible,
      // it virtually never matched. The comment remains here for documentation
      // purposes.
      // Phone: <cc> \( <ac> \) <phone>
      // {{REGEX_PHONE, FIELD_COUNTRY_CODE},
      //  {REGEX_AREA_NOTEXT, FIELD_AREA_CODE},
      //  {REGEX_PREFIX_SEPARATOR, FIELD_PHONE}},
      //
      // The following grammar was removed because, though it looks plausible,
      // it virtually never matched. The comment remains here for documentation
      // purposes.
      // Phone: <cc> - <ac> - <phone> - <suffix>
      // {{REGEX_PHONE, FIELD_COUNTRY_CODE},
      //  {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE},
      //  {REGEX_PREFIX_SEPARATOR, FIELD_PHONE},
      //  {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX}},
      //
      // Area code: <ac>:3 Prefix: <prefix>:3 Suffix: <suffix>:4
      {{REGEX_AREA, FIELD_AREA_CODE, 3},
       {REGEX_PREFIX, FIELD_PHONE, 3},
       {REGEX_SUFFIX, FIELD_SUFFIX, 4}},
      // Phone: <ac> Prefix: <phone> Suffix: <suffix>
      {{REGEX_PHONE, FIELD_AREA_CODE},
       {REGEX_PREFIX, FIELD_PHONE},
       {REGEX_SUFFIX, FIELD_SUFFIX}},
      // Phone: <ac> - <phone>:3 - <suffix>:4
      {{REGEX_PHONE, FIELD_AREA_CODE},
       {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
       {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4}},
      // Phone: <cc> - <ac> - <phone>
      {{REGEX_PHONE, FIELD_COUNTRY_CODE},
       {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE},
       {REGEX_SUFFIX_SEPARATOR, FIELD_PHONE}},
      // Phone: <cc>:3 - <phone>
      {{REGEX_PHONE, FIELD_COUNTRY_CODE, 3}, {REGEX_PHONE, FIELD_PHONE}},
      // Phone: <ac> - <phone>
      {{REGEX_PHONE, FIELD_AREA_CODE}, {REGEX_PREFIX_SEPARATOR, FIELD_PHONE}},
      // Phone: <phone>
      {{REGEX_PHONE, FIELD_PHONE}},
  });
  return *grammars;
}

// static
bool PhoneFieldParser::LikelyAugmentedPhoneCountryCode(
    AutofillScanner* scanner,
    raw_ptr<AutofillField>* matched_field) {
  AutofillField* field = scanner->Cursor();

  // Return false if the field is not a selection box.
  if (!MatchesFormControlType(field->form_control_type(),
                              {FormControlType::kSelectOne})) {
    return false;
  }

  // If the number of the options is less than the minimum limit or more than
  // the maximum limit, return false.
  if (field->options().size() < kMinSelectOptionsForCountryCode ||
      field->options().size() >= kMaxSelectOptionsForCountryCode) {
    return false;
  }

  // |total_covered_options| stores the count of the options that are
  // compared with the regex.
  int total_num_options = static_cast<int>(field->options().size());

  // |total_positive_options| stores the count of the options that match the
  // regex.
  int total_positive_options =
      base::ranges::count_if(field->options(), [](const SelectOption& option) {
        return MatchesRegex<kAugmentedPhoneCountryCodeRe>(option.text);
      });

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
bool PhoneFieldParser::ParseGrammar(ParsingContext& context,
                                    const PhoneGrammar& grammar,
                                    ParsedPhoneFields& parsed_fields,
                                    AutofillScanner* scanner) {
  for (const auto& rule : grammar) {
    const bool is_country_code_field = rule.phone_part == FIELD_COUNTRY_CODE;

    // The field length comparison with |rule.max_size| is not required in case
    // of the selection boxes that are of phone country code type.
    if (is_country_code_field &&
        LikelyAugmentedPhoneCountryCode(scanner,
                                        &parsed_fields[FIELD_COUNTRY_CODE])) {
      continue;
    }

    if (!ParsePhoneField(context, scanner, &parsed_fields[rule.phone_part],
                         is_country_code_field, GetJSONFieldType(rule.regex))) {
      return false;
    }

    if (rule.max_size != 0 &&
        (parsed_fields[rule.phone_part]->max_length() == 0 ||
         rule.max_size < parsed_fields[rule.phone_part]->max_length())) {
      return false;
    }
  }
  return true;
}

// static
std::unique_ptr<FormFieldParser> PhoneFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return nullptr;

  size_t start_cursor = scanner->SaveCursor();
  ParsedPhoneFields parsed_fields;

  // Find the first matching grammar.
  bool found_matching_grammar = false;
  int grammar_id = 0;
  for (const PhoneGrammar& grammar : GetPhoneGrammars()) {
    base::ranges::fill(parsed_fields, nullptr);
    if (ParseGrammar(context, grammar, parsed_fields, scanner)) {
      found_matching_grammar = true;
      break;
    }
    scanner->RewindTo(start_cursor);
    grammar_id++;
  }
  if (!found_matching_grammar)
    return nullptr;
  // No grammar without FIELD_PHONE should be defined.
  DCHECK(parsed_fields[FIELD_PHONE] != nullptr);

  // If this CHECK fails, the number of grammar rules has changed and you need
  // to increment the version counter in the histogram name and its enum.
  CHECK_EQ(GetPhoneGrammars().size(), 15u);
  base::UmaHistogramExactLinear(
      "Autofill.FieldPrediction.PhoneNumberGrammarUsage2", grammar_id,
      /*exclusive_max=*/GetPhoneGrammars().size());

  // Now look for an extension.
  // The extension is unused, but it is parsed to prevent other parsers from
  // misclassifying it as something else.
  ParsePhoneField(context, scanner, &parsed_fields[FIELD_EXTENSION],
                  /*is_country_code_field=*/false, "PHONE_EXTENSION");

  return base::WrapUnique(new PhoneFieldParser(std::move(parsed_fields)));
}

void PhoneFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  DCHECK(parsed_phone_fields_[FIELD_PHONE]);  // Phone was correctly parsed.

  bool has_country_code = parsed_phone_fields_[FIELD_COUNTRY_CODE] != nullptr;
  if (has_country_code || parsed_phone_fields_[FIELD_AREA_CODE] ||
      parsed_phone_fields_[FIELD_SUFFIX]) {
    if (has_country_code) {
      AddClassification(parsed_phone_fields_[FIELD_COUNTRY_CODE],
                        PHONE_HOME_COUNTRY_CODE, kBasePhoneParserScore,
                        field_candidates);
    }

    FieldType field_number_type = PHONE_HOME_NUMBER;
    // Rationalization will pick the correct trunk-type, so this logic doesn't
    // need to distinguish.
    if (parsed_phone_fields_[FIELD_AREA_CODE]) {
      AddClassification(parsed_phone_fields_[FIELD_AREA_CODE],
                        PHONE_HOME_CITY_CODE, kBasePhoneParserScore,
                        field_candidates);
    } else if (has_country_code) {
      field_number_type = PHONE_HOME_CITY_AND_NUMBER;
    }
    // PHONE_HOME_NUMBER = PHONE_HOME_NUMBER_PREFIX + PHONE_HOME_NUMBER_SUFFIX
    // is technically dialable (seven-digit dialing), and thus not contained in
    // the area code branch.
    if (parsed_phone_fields_[FIELD_SUFFIX]) {
      // TODO(crbug.com/40233246): Ideally we want to DCHECK that
      // `parsed_phone_fields_[FIELD_AREA_CODE] || !has_country_code` here.
      // With the current grammars this can be violated, even though it
      // seemingly never happens in practice according to our metrics.
      field_number_type = PHONE_HOME_NUMBER_PREFIX;
      AddClassification(parsed_phone_fields_[FIELD_SUFFIX],
                        PHONE_HOME_NUMBER_SUFFIX, kBasePhoneParserScore,
                        field_candidates);
    }
    AddClassification(parsed_phone_fields_[FIELD_PHONE], field_number_type,
                      kBasePhoneParserScore, field_candidates);
  } else {
    const AutofillField* field = parsed_phone_fields_[FIELD_PHONE];
    if (field->label().find(u"+") != std::u16string::npos ||
        field->placeholder().find(u"+") != std::u16string::npos ||
        field->aria_description().find(u"+") != std::u16string::npos) {
      AddClassification(field, PHONE_HOME_WHOLE_NUMBER, kBasePhoneParserScore,
                        field_candidates);
    } else {
      AddClassification(field, PHONE_HOME_CITY_AND_NUMBER,
                        kBasePhoneParserScore, field_candidates);
    }
  }

  if (parsed_phone_fields_[FIELD_EXTENSION]) {
    AddClassification(parsed_phone_fields_[FIELD_EXTENSION],
                      PHONE_HOME_EXTENSION, kBasePhoneParserScore,
                      field_candidates);
  }
}

PhoneFieldParser::PhoneFieldParser(ParsedPhoneFields fields)
    : parsed_phone_fields_(std::move(fields)) {}

// Returns the string representation of |phonetype_id| as it is used to key to
// identify coressponding patterns.
std::string PhoneFieldParser::GetJSONFieldType(RegexType phonetype_id) {
  switch (phonetype_id) {
    case REGEX_COUNTRY:
      return "PHONE_COUNTRY_CODE";
    case REGEX_AREA:
      return "PHONE_AREA_CODE";
    case REGEX_AREA_NOTEXT:
      return "PHONE_AREA_CODE_NO_TEXT";
    case REGEX_PHONE:
      return "PHONE";
    case REGEX_PREFIX_SEPARATOR:
      return "PHONE_PREFIX_SEPARATOR";
    case REGEX_PREFIX:
      return "PHONE_PREFIX";
    case REGEX_SUFFIX_SEPARATOR:
      return "PHONE_SUFFIX_SEPARATOR";
    case REGEX_SUFFIX:
      return "PHONE_SUFFIX";
    case REGEX_EXTENSION:
      return "PHONE_EXTENSION";
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::string();
}

// static
bool PhoneFieldParser::ParsePhoneField(ParsingContext& context,
                                       AutofillScanner* scanner,
                                       raw_ptr<AutofillField>* field,
                                       const bool is_country_code_field,
                                       const std::string& json_field_type) {
  base::span<const MatchPatternRef> patterns = GetMatchPatterns(
      json_field_type, context.page_language, context.pattern_file);

  // Phone country code fields can be discovered via the generic "PHONE" regex
  // (see e.g. the "Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4" grammar rule).
  // However, for phone country code fields, <select> elements should also be
  // considered.
  if (is_country_code_field) {
    return ParseField(context, scanner, patterns, field,
                      /*regex_name=*/json_field_type.c_str(),
                      [](const MatchParams& p) {
                        return MatchParams(p.attributes,
                kDefaultMatchParamsWith<
        FormControlType::kInputTelephone, FormControlType::kInputNumber,
        FormControlType::kSelectOne>.field_types);
                      });
  }

  return ParseField(context, scanner, patterns, field,
                    /*regex_name=*/json_field_type.c_str());
}

}  // namespace autofill
