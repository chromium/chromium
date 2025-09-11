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
                                                AutofillScanner& scanner);

  AlternativeFullNameField(const AlternativeFullNameField&) = delete;
  AlternativeFullNameField& operator=(const AlternativeFullNameField&) = delete;

 protected:
  AlternativeFullNameField() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  std::optional<FieldAndMatchInfo> alternative_full_name_;
};

// static
std::unique_ptr<FormFieldParser> AlternativeFullNameField::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  auto v = base::WrapUnique(new AlternativeFullNameField());
  const AutofillScanner::Position position = scanner.GetPosition();

  while (!scanner.IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, "ADDRESS_NAME_IGNORED")) {
      continue;
    }
    if (!v->alternative_full_name_ &&
        ParseField(context, scanner, "ALTERNATIVE_FULL_NAME",
                   &v->alternative_full_name_)) {
      continue;
    }
    break;
  }
  if (v->alternative_full_name_) {
    return v;
  }

  scanner.Restore(position);
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
                                                AutofillScanner& scanner);

  AlternativeFamilyAndGivenNameField(
      const AlternativeFamilyAndGivenNameField&) = delete;
  AlternativeFamilyAndGivenNameField& operator=(
      const AlternativeFamilyAndGivenNameField&) = delete;

 protected:
  AlternativeFamilyAndGivenNameField() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  std::optional<FieldAndMatchInfo> alternative_given_name_;
  std::optional<FieldAndMatchInfo> alternative_family_name_;
};

// static
std::unique_ptr<FormFieldParser> AlternativeFamilyAndGivenNameField::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  auto v = base::WrapUnique(new AlternativeFamilyAndGivenNameField());
  const AutofillScanner::Position position = scanner.GetPosition();

  while (!scanner.IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, "ADDRESS_NAME_IGNORED")) {
      continue;
    }
    if (!v->alternative_family_name_ &&
        ParseField(context, scanner, "ALTERNATIVE_FAMILY_NAME",
                   &v->alternative_family_name_)) {
      continue;
    }
    if (!v->alternative_given_name_ &&
        ParseField(context, scanner, "ALTERNATIVE_GIVEN_NAME",
                   &v->alternative_given_name_)) {
      continue;
    }

    break;
  }
  if (v->alternative_family_name_ && v->alternative_given_name_) {
    return v;
  }

  scanner.Restore(position);
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
    AutofillScanner& scanner) {
  if (scanner.IsEnd()) {
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
