// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field_parser.h"

#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {
namespace {

// A form field that can parse a full name field.
class FullNameField : public NameFieldParser {
 public:
  static std::unique_ptr<FullNameField> Parse(ParsingContext& context,
                                              AutofillScanner& scanner);
  explicit FullNameField(FieldAndMatchInfo match);

  FullNameField(const FullNameField&) = delete;
  FullNameField& operator=(const FullNameField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FieldAndMatchInfo match_;
};

// A form field that parses a first name field and two last name fields as they
// are used in Hispanic/Latinx names.
class FirstTwoLastNamesField : public NameFieldParser {
 public:
  static std::unique_ptr<FirstTwoLastNamesField> ParseComponentNames(
      ParsingContext& context,
      AutofillScanner& scanner);
  static std::unique_ptr<FirstTwoLastNamesField> Parse(
      ParsingContext& context,
      AutofillScanner& scanner);

  FirstTwoLastNamesField(const FirstTwoLastNamesField&) = delete;
  FirstTwoLastNamesField& operator=(const FirstTwoLastNamesField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FirstTwoLastNamesField();

  // `honorific_prefix_` and `middle_name` are not required for a match.
  std::optional<FieldAndMatchInfo> honorific_prefix_;
  std::optional<FieldAndMatchInfo> first_name_;
  std::optional<FieldAndMatchInfo> middle_name_;
  std::optional<FieldAndMatchInfo> last_name_prefix_;
  std::optional<FieldAndMatchInfo> first_last_name_;
  std::optional<FieldAndMatchInfo> second_last_name_;
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameFieldParser {
 public:
  // Tries to match a series of name fields that follows the pattern "Name,
  // Surname".
  static std::unique_ptr<FirstLastNameField> ParseNameSurnameLabelSequence(
      ParsingContext& context,
      AutofillScanner& scanner);

  // Tries to match a series of fields with a shared label: The first field
  // needs to have a unspecific name label followed by up to two fields without
  // a label.
  static std::unique_ptr<FirstLastNameField> ParseSharedNameLabelSequence(
      ParsingContext& context,
      AutofillScanner& scanner);

  // Tries to match a series of fields with patterns that are specific to the
  // individual components of a name. Note that the order of the components does
  // not matter.
  static std::unique_ptr<FirstLastNameField> ParseSpecificComponentSequence(
      ParsingContext& context,
      AutofillScanner& scanner);

  // Probes the matching strategies defined above. Returns the result of the
  // first successful match. Returns a nullptr if no matches can be found.
  static std::unique_ptr<FirstLastNameField> Parse(ParsingContext& context,
                                                   AutofillScanner& scanner);

  FirstLastNameField(const FirstLastNameField&) = delete;
  FirstLastNameField& operator=(const FirstLastNameField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FirstLastNameField();

  // `honorific_prefix_` and `middle_name` are not required for a match.
  std::optional<FieldAndMatchInfo> honorific_prefix_;
  std::optional<FieldAndMatchInfo> first_name_;
  std::optional<FieldAndMatchInfo> middle_name_;
  std::optional<FieldAndMatchInfo> last_name_prefix_;
  std::optional<FieldAndMatchInfo> last_name_;
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.
};

}  // namespace

// static
std::unique_ptr<FormFieldParser> NameFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  if (scanner.IsEnd()) {
    return nullptr;
  }

  // Try |FirstLastNameField| and |FirstTwoLastNamesField| first since they are
  // more specific.
  std::unique_ptr<FormFieldParser> field;
  if (!field) {
    field = FirstTwoLastNamesField::Parse(context, scanner);
  }
  if (!field) {
    field = FirstLastNameField::Parse(context, scanner);
  }
  if (!field) {
    field = FullNameField::Parse(context, scanner);
  }
  return field;
}

// This is overridden in concrete subclasses.
void NameFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {}

// static
std::unique_ptr<FullNameField> FullNameField::Parse(ParsingContext& context,
                                                    AutofillScanner& scanner) {
  // Exclude e.g. "username" or "nickname" fields.
  const AutofillScanner::Position position = scanner.GetPosition();
  bool should_ignore = ParseField(context, scanner, "NAME_IGNORED") ||
                       ParseField(context, scanner, "ADDRESS_NAME_IGNORED");
  scanner.Restore(position);
  if (should_ignore) {
    return nullptr;
  }

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  std::optional<FieldAndMatchInfo> match;

  if (ParseField(context, scanner, "FULL_NAME", &match)) {
    return std::make_unique<FullNameField>(std::move(*match));
  }

  return nullptr;
}

void FullNameField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, NAME_FULL, kBaseNameParserScore, field_candidates);
}

FullNameField::FullNameField(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

FirstTwoLastNamesField::FirstTwoLastNamesField() = default;

// static
std::unique_ptr<FirstTwoLastNamesField> FirstTwoLastNamesField::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  return ParseComponentNames(context, scanner);
}

// static
std::unique_ptr<FirstTwoLastNamesField>
FirstTwoLastNamesField::ParseComponentNames(ParsingContext& context,
                                            AutofillScanner& scanner) {
  auto v = base::WrapUnique(new FirstTwoLastNamesField());
  const AutofillScanner::Position position = scanner.GetPosition();

  // Allow name fields to appear in any order.
  while (!scanner.IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, "ADDRESS_NAME_IGNORED")) {
      continue;
    }

    // Scan for the honorific prefix before checking for unrelated name fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    // TODO(crbug.com/40137264): Remove check once feature is launched or
    // removed.
    if (!v->honorific_prefix_ &&
        ParseField(context, scanner, "HONORIFIC_PREFIX",
                   &v->honorific_prefix_)) {
      continue;
    }

    // Skip over any unrelated fields, e.g. "username" or "nickname".
    if (ParseField(context, scanner, "NAME_IGNORED")) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(context, scanner, "FIRST_NAME", &v->first_name_)) {
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(context, scanner, "MIDDLE_NAME", &v->middle_name_)) {
      continue;
    }

    // TODO(crbug.com/386916943) Remove check once feature is launched or
    // removed.
    if (base::FeatureList::IsEnabled(
            features::kAutofillSupportLastNamePrefix) &&
        !v->last_name_prefix_ &&
        ParseField(context, scanner, "LAST_NAME_PREFIX",
                   &v->last_name_prefix_)) {
      continue;
    }

    if (!v->first_last_name_ &&
        ParseField(context, scanner, "LAST_NAME_FIRST", &v->first_last_name_)) {
      continue;
    }

    if (!v->second_last_name_ &&
        ParseField(context, scanner, "LAST_NAME_SECOND",
                   &v->second_last_name_)) {
      continue;
    }

    break;
  }

  // Consider the match to be successful if we detected both last names and the
  // surname.
  if (v->first_name_ && v->first_last_name_ && v->second_last_name_) {
    return v;
  }

  scanner.Restore(position);
  return nullptr;
}

void FirstTwoLastNamesField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(honorific_prefix_, NAME_HONORIFIC_PREFIX,
                    kBaseNameParserScore, field_candidates);
  AddClassification(first_name_, NAME_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(last_name_prefix_, NAME_LAST_PREFIX, kBaseNameParserScore,
                    field_candidates);
  AddClassification(first_last_name_, NAME_LAST_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(second_last_name_, NAME_LAST_SECOND, kBaseNameParserScore,
                    field_candidates);
  const FieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  AddClassification(middle_name_, type, kBaseNameParserScore, field_candidates);
}

std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseNameSurnameLabelSequence(ParsingContext& context,
                                                  AutofillScanner& scanner) {
  // Some pages have a generic name label that corresponds to a first name
  // followed by a last name label.
  // Example: Name [      ] Last Name [      ]
  auto v = base::WrapUnique(new FirstLastNameField());

  AutofillScanner::Position position = scanner.GetPosition();

  bool should_ignore = ParseField(context, scanner, "NAME_IGNORED") ||
                       ParseField(context, scanner, "ADDRESS_NAME_IGNORED");
  scanner.Restore(position);

  position = scanner.GetPosition();

  if (should_ignore) {
    return nullptr;
  }

  if (ParseField(context, scanner, "NAME_GENERIC", &v->first_name_)) {
    // Check for an optional middle name field.
    ParseField(context, scanner, "MIDDLE_NAME", &v->middle_name_);
    // TODO(crbug.com/386916943) Remove check once feature is launched or
    // removed.
    if (base::FeatureList::IsEnabled(
            features::kAutofillSupportLastNamePrefix)) {
      ParseField(context, scanner, "LAST_NAME_PREFIX", &v->last_name_prefix_);
    }
    if (ParseField(context, scanner, "LAST_NAME", &v->last_name_)) {
      return v;
    }
  }

  scanner.Restore(position);
  return nullptr;
}

std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseSharedNameLabelSequence(ParsingContext& context,
                                                 AutofillScanner& scanner) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  auto v = base::WrapUnique(new FirstLastNameField());
  const AutofillScanner::Position position = scanner.GetPosition();

  std::optional<FieldAndMatchInfo> next;
  if (ParseField(context, scanner, "NAME_GENERIC", &v->first_name_) &&
      ParseEmptyLabel(context, scanner, &next)) {
    if (ParseEmptyLabel(context, scanner, &v->last_name_)) {
      // There are three name fields; assume that the middle one is a
      // middle initial (it is, at least, on SmithsonianCheckout.html).
      v->middle_name_ = std::move(next);
      v->middle_initial_ = true;
    } else {  // only two name fields
      v->last_name_ = std::move(next);
    }

    return v;
  }

  scanner.Restore(position);
  return nullptr;
}

// static
std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseSpecificComponentSequence(ParsingContext& context,
                                                   AutofillScanner& scanner) {
  auto v = base::WrapUnique(new FirstLastNameField());
  const AutofillScanner::Position position = scanner.GetPosition();

  // A fair number of pages use the names "fname" and "lname" for naming
  // first and last name fields (examples from the test suite:
  // BESTBUY_COM - Sign In2.html; Crate and Barrel Check Out.html;
  // dell_checkout1.html).  At least one UK page (The China Shop2.html)
  // asks, in stuffy English style, for just initials and a surname,
  // so we match "initials" here (and just fill in a first name there,
  // American-style).
  // The ".*first$" matches fields ending in "first" (example in sample8.html).
  // The ".*last$" matches fields ending in "last" (example in sample8.html).

  // Allow name fields to appear in any order.

  while (!scanner.IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(context, scanner, "ADDRESS_NAME_IGNORED")) {
      continue;
    }

    // Scan for the honorific prefix before checking for unrelated fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    if (!v->honorific_prefix_ &&
        ParseField(context, scanner, "HONORIFIC_PREFIX",
                   &v->honorific_prefix_)) {
      continue;
    }

    // Skip over any unrelated name fields, e.g. "username" or "nickname".
    if (ParseField(context, scanner, "NAME_IGNORED")) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(context, scanner, "FIRST_NAME", &v->first_name_)) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(context, scanner, "MIDDLE_INITIAL", &v->middle_name_)) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(context, scanner, "MIDDLE_NAME", &v->middle_name_)) {
      continue;
    }

    // TODO(crbug.com/386916943) Remove check once feature is launched or
    // removed.
    if (base::FeatureList::IsEnabled(
            features::kAutofillSupportLastNamePrefix) &&
        !v->last_name_prefix_ &&
        ParseField(context, scanner, "LAST_NAME_PREFIX",
                   &v->last_name_prefix_)) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(context, scanner, "LAST_NAME", &v->last_name_)) {
      continue;
    }

    break;
  }

  // Consider the match to be successful if we detected both first and last name
  // fields.
  if (v->first_name_ && v->last_name_) {
    return v;
  }

  scanner.Restore(position);
  return nullptr;
}

// static
std::unique_ptr<FirstLastNameField> FirstLastNameField::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  std::unique_ptr<FirstLastNameField> field =
      ParseSharedNameLabelSequence(context, scanner);

  if (!field) {
    field = ParseNameSurnameLabelSequence(context, scanner);
  }
  if (!field) {
    field = ParseSpecificComponentSequence(context, scanner);
  }
  return field;
}

FirstLastNameField::FirstLastNameField() = default;

void FirstLastNameField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(honorific_prefix_, NAME_HONORIFIC_PREFIX,
                    kBaseNameParserScore, field_candidates);
  AddClassification(first_name_, NAME_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(last_name_prefix_, NAME_LAST_PREFIX, kBaseNameParserScore,
                    field_candidates);
  AddClassification(last_name_, last_name_prefix_ ? NAME_LAST_CORE : NAME_LAST,
                    kBaseNameParserScore, field_candidates);
  const FieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  AddClassification(middle_name_, type, kBaseNameParserScore, field_candidates);
}

}  // namespace autofill
