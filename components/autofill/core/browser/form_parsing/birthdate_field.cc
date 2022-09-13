// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/birthdate_field.h"

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

namespace {

// Checks if the `option`'s content or value represents a number with value
// contained in [a, b].
bool IsSelectValueBetween(const SelectOption& option, int a, int b) {
  auto IsValueBetween = [&](base::StringPiece16 string) {
    int value;
    return base::StringToInt(string, &value) && a <= value && value <= b;
  };
  return IsValueBetween(option.content) || IsValueBetween(option.value);
}

}  // namespace

BirthdateField::BirthdateField(const AutofillField* day,
                               const AutofillField* month,
                               const AutofillField* year)
    : day_(day), month_(month), year_(year) {}

// static
std::unique_ptr<FormField> BirthdateField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  // Currently only <select> elements are considered.
  AutofillField* day = nullptr;
  AutofillField* month = nullptr;
  AutofillField* year = nullptr;
  // Expect at most 31 days/12 months plus one placeholder.
  if (FormField::ParseInAnyOrder(
          scanner,
          {{&day, base::BindRepeating(&IsSelectWithIncreasingValues, scanner,
                                      28, 32)},
           {&month, base::BindRepeating(&IsSelectWithIncreasingValues, scanner,
                                        12, 13)},
           {&year, base::BindRepeating(&IsLikelyBirthdateYearSelectField,
                                       scanner)}})) {
    return base::WrapUnique(new BirthdateField(day, month, year));
  }
  return nullptr;
}

// static
bool BirthdateField::IsSelectWithIncreasingValues(AutofillScanner* scanner,
                                                  int max_value,
                                                  size_t max_options) {
  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(field->form_control_type,
                              {MatchFieldType::kSelect})) {
    return false;
  }
  auto options = field->options;
  if (options.empty() || options.size() > max_options)
    return false;
  auto it = options.cbegin();
  // Skip a possible placeholder.
  if (!IsSelectValueBetween(*it, 1, 1))
    it++;
  // Check that there are the enough options remaining.
  if (options.cend() - it < max_value)
    return false;
  for (int i = 1; i <= max_value; i++) {
    if (!IsSelectValueBetween(*it, i, i))
      return false;
    it++;
  }
  return true;
}

// static
bool BirthdateField::IsLikelyBirthdateYearSelectField(
    AutofillScanner* scanner) {
  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(field->form_control_type,
                              {MatchFieldType::kSelect})) {
    return false;
  }
  auto options = field->options;
  base::Time::Exploded current_date;
  AutofillClock::Now().UTCExplode(&current_date);
  DCHECK(current_date.HasValidValues());
  return !options.empty() &&
         base::ranges::all_of(options.begin() + 1, options.end(),
                              [&](const SelectOption& option) {
                                return IsSelectValueBetween(option, 1900,
                                                            current_date.year);
                              });
}

void BirthdateField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(day_, BIRTHDATE_DAY, kBaseBirthdateParserScore,
                    field_candidates);
  AddClassification(month_, BIRTHDATE_MONTH, kBaseBirthdateParserScore,
                    field_candidates);
  AddClassification(year_, BIRTHDATE_4_DIGIT_YEAR, kBaseBirthdateParserScore,
                    field_candidates);
}

}  // namespace autofill
