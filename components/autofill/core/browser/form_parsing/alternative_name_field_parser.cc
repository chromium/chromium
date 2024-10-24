// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/alternative_name_field_parser.h"

#include <string>

#include "base/notimplemented.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {
namespace {

// A form field that can parse a full alternative name field.
class AlternativeFullNameField : public AlternativeNameFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  AlternativeFullNameField(const AlternativeFullNameField&) = delete;
  AlternativeFullNameField& operator=(const AlternativeFullNameField&) = delete;

 protected:
  AlternativeFullNameField() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<AutofillField> alternative_full_name_{nullptr};
};

// static
std::unique_ptr<FormFieldParser> AlternativeFullNameField::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  auto v = base::WrapUnique(new AlternativeFullNameField());
  scanner->SaveCursor();

  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", context.page_language,
                       context.pattern_file);
  base::span<const MatchPatternRef> alternative_full_name_patterns =
      GetMatchPatterns("ALTERNATIVE_FULL_NAME", context.page_language,
                       context.pattern_file);

  while (!scanner->IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, address_name_ignored_patterns, nullptr,
                   "ADDRESS_NAME_IGNORED")) {
      continue;
    }
    if (!v->alternative_full_name_ &&
        ParseField(context, scanner, alternative_full_name_patterns,
                   &v->alternative_full_name_, "ALTERNATIVE_FULL_NAME")) {
      continue;
    }
    break;
  }
  if (v->alternative_full_name_) {
    return v;
  }

  scanner->Rewind();
  return nullptr;
}

void AlternativeFullNameField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(alternative_full_name_, ALTERNATIVE_FULL_NAME,
                    kBaseNameParserScore, field_candidates);
}

// A form field that can parse a family alternative name field and then given
// alternative name field.
class AlternativeFamilyAndGivenNameField : public AlternativeNameFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  AlternativeFamilyAndGivenNameField(
      const AlternativeFamilyAndGivenNameField&) = delete;
  AlternativeFamilyAndGivenNameField& operator=(
      const AlternativeFamilyAndGivenNameField&) = delete;

 protected:
  AlternativeFamilyAndGivenNameField() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<AutofillField> alternative_given_name_{nullptr};
  raw_ptr<AutofillField> alternative_family_name_{nullptr};
};

// static
std::unique_ptr<FormFieldParser> AlternativeFamilyAndGivenNameField::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  auto v = base::WrapUnique(new AlternativeFamilyAndGivenNameField());
  scanner->SaveCursor();

  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", context.page_language,
                       context.pattern_file);
  base::span<const MatchPatternRef> alternative_family_name_patterns =
      GetMatchPatterns("ALTERNATIVE_FAMILY_NAME", context.page_language,
                       context.pattern_file);
  base::span<const MatchPatternRef> alternative_given_name_patterns =
      GetMatchPatterns("ALTERNATIVE_GIVEN_NAME", context.page_language,
                       context.pattern_file);

  while (!scanner->IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, address_name_ignored_patterns, nullptr,
                   "ADDRESS_NAME_IGNORED")) {
      continue;
    }
    if (!v->alternative_family_name_ &&
        ParseField(context, scanner, alternative_family_name_patterns,
                   &v->alternative_family_name_, "ALTERNATIVE_FAMILY_NAME")) {
      continue;
    }
    if (!v->alternative_given_name_ &&
        ParseField(context, scanner, alternative_given_name_patterns,
                   &v->alternative_given_name_, "ALTERNATIVE_GIVEN_NAME")) {
      continue;
    }

    break;
  }
  if (v->alternative_family_name_ && v->alternative_given_name_) {
    return v;
  }

  scanner->Rewind();
  return nullptr;
}

void AlternativeFamilyAndGivenNameField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(alternative_family_name_, ALTERNATIVE_FAMILY_NAME,
                    kBaseNameParserScore, field_candidates);
  AddClassification(alternative_given_name_, ALTERNATIVE_GIVEN_NAME,
                    kBaseNameParserScore, field_candidates);
}
}  // namespace

// static
std::unique_ptr<FormFieldParser> AlternativeNameFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return nullptr;
  }

  if (auto field = AlternativeFamilyAndGivenNameField::Parse(context, scanner);
      field) {
    return field;
  }
  if (auto field = AlternativeFullNameField::Parse(context, scanner); field) {
    return field;
  }
  return nullptr;
}

// This is overridden in concrete subclasses.
void AlternativeNameFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {}

}  // namespace autofill
