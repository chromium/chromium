// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {
namespace {

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  static std::unique_ptr<FullNameField> Parse(AutofillScanner* scanner,
                                              const LanguageCode& page_language,
                                              PatternSource pattern_source,
                                              LogManager* log_manager);
  explicit FullNameField(AutofillField* field);

  FullNameField(const FullNameField&) = delete;
  FullNameField& operator=(const FullNameField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<AutofillField> field_;
};

// A form field that parses a first name field and two last name fields as they
// are used in Hispanic/Latinx names.
class FirstTwoLastNamesField : public NameField {
 public:
  static std::unique_ptr<FirstTwoLastNamesField> ParseComponentNames(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);
  static std::unique_ptr<FirstTwoLastNamesField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  FirstTwoLastNamesField(const FirstTwoLastNamesField&) = delete;
  FirstTwoLastNamesField& operator=(const FirstTwoLastNamesField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FirstTwoLastNamesField();

  raw_ptr<AutofillField> honorific_prefix_{nullptr};  // Optional.
  raw_ptr<AutofillField> first_name_{nullptr};
  raw_ptr<AutofillField> middle_name_{nullptr};  // Optional.
  raw_ptr<AutofillField> first_last_name_{nullptr};
  raw_ptr<AutofillField> second_last_name_{nullptr};
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameField {
 public:
  // Tries to match a series of name fields that follows the pattern "Name,
  // Surname".
  static std::unique_ptr<FirstLastNameField> ParseNameSurnameLabelSequence(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  // Tries to match a series of fields with a shared label: The first field
  // needs to have a unspecific name label followed by up to two fields without
  // a label.
  static std::unique_ptr<FirstLastNameField> ParseSharedNameLabelSequence(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  // Tries to match a series of fields with patterns that are specific to the
  // individual components of a name. Note that the order of the components does
  // not matter.
  static std::unique_ptr<FirstLastNameField> ParseSpecificComponentSequence(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  // Probes the matching strategies defined above. Returns the result of the
  // first successful match. Returns a nullptr if no matches can be found.
  static std::unique_ptr<FirstLastNameField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  FirstLastNameField(const FirstLastNameField&) = delete;
  FirstLastNameField& operator=(const FirstLastNameField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FirstLastNameField();

  raw_ptr<AutofillField> honorific_prefix_{nullptr};  // Optional
  raw_ptr<AutofillField> first_name_{nullptr};
  raw_ptr<AutofillField> middle_name_{nullptr};  // Optional.
  raw_ptr<AutofillField> last_name_{nullptr};
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.
};

}  // namespace

// static
std::unique_ptr<FormField> NameField::Parse(AutofillScanner* scanner,
                                            const LanguageCode& page_language,
                                            PatternSource pattern_source,
                                            LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  // Try |FirstLastNameField| and |FirstTwoLastNamesField| first since they are
  // more specific.
  std::unique_ptr<FormField> field;
  if (!field) {
    field = FirstTwoLastNamesField::Parse(scanner, page_language,
                                          pattern_source, log_manager);
  }
  if (!field) {
    field = FirstLastNameField::Parse(scanner, page_language, pattern_source,
                                      log_manager);
  }
  if (!field) {
    field = FullNameField::Parse(scanner, page_language, pattern_source,
                                 log_manager);
  }
  return field;
}

// This is overridden in concrete subclasses.
void NameField::AddClassifications(FieldCandidatesMap& field_candidates) const {
}

// static
std::unique_ptr<FullNameField> FullNameField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  // Exclude e.g. "username" or "nickname" fields.
  scanner->SaveCursor();
  base::span<const MatchPatternRef> name_ignored_patterns =
      GetMatchPatterns("NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", page_language, pattern_source);
  bool should_ignore =
      ParseField(scanner, kNameIgnoredRe, name_ignored_patterns, nullptr,
                 {log_manager, "kNameIgnoredRe"}) ||
      ParseField(scanner, kAddressNameIgnoredRe, address_name_ignored_patterns,
                 nullptr, {log_manager, "kAddressNameIgnoredRe"});
  scanner->Rewind();
  if (should_ignore)
    return nullptr;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  raw_ptr<AutofillField> field = nullptr;

  base::span<const MatchPatternRef> name_patterns =
      GetMatchPatterns("FULL_NAME", page_language, pattern_source);
  if (ParseField(scanner, kFullNameRe, name_patterns, &field,
                 {log_manager, "kFullNameRe"}))
    return std::make_unique<FullNameField>(field);

  return nullptr;
}

void FullNameField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, NAME_FULL, kBaseNameParserScore, field_candidates);
}

FullNameField::FullNameField(AutofillField* field) : field_(field) {}

FirstTwoLastNamesField::FirstTwoLastNamesField() = default;

// static
std::unique_ptr<FirstTwoLastNamesField> FirstTwoLastNamesField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  return ParseComponentNames(scanner, page_language, pattern_source,
                             log_manager);
}

// static
std::unique_ptr<FirstTwoLastNamesField>
FirstTwoLastNamesField::ParseComponentNames(AutofillScanner* scanner,
                                            const LanguageCode& page_language,
                                            PatternSource pattern_source,
                                            LogManager* log_manager) {
  auto v = base::WrapUnique(new FirstTwoLastNamesField());
  scanner->SaveCursor();

  base::span<const MatchPatternRef> honorific_prefix_patterns =
      GetMatchPatterns("HONORIFIC_PREFIX", page_language, pattern_source);
  base::span<const MatchPatternRef> name_ignored_patterns =
      GetMatchPatterns("NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> first_name_patterns =
      GetMatchPatterns("FIRST_NAME", page_language, pattern_source);
  base::span<const MatchPatternRef> middle_name_patterns =
      GetMatchPatterns("MIDDLE_NAME", page_language, pattern_source);
  base::span<const MatchPatternRef> first_last_name_patterns =
      GetMatchPatterns("LAST_NAME_FIRST", page_language, pattern_source);
  base::span<const MatchPatternRef> second_last_name_patterns =
      GetMatchPatterns("LAST_NAME_SECOND", page_language, pattern_source);

  // Allow name fields to appear in any order.
  while (!scanner->IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(scanner, kAddressNameIgnoredRe,
                   address_name_ignored_patterns, nullptr,
                   {log_manager, "kAddressNameIgnoredRe"})) {
      continue;
    }

    // Scan for the honorific prefix before checking for unrelated name fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    // TODO(crbug.com/1098943): Remove check once feature is launched or
    // removed.
    if (!v->honorific_prefix_ &&
        ParseField(scanner, kHonorificPrefixRe, honorific_prefix_patterns,
                   &v->honorific_prefix_,
                   {log_manager, "kHonorificPrefixRe"})) {
      continue;
    }

    // Skip over any unrelated fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, kNameIgnoredRe,
                            kDefaultMatchParamsWith<MatchFieldType::kSelect,
                                                    MatchFieldType::kSearch>,
                            name_ignored_patterns, nullptr,
                            {log_manager, "kNameIgnoredRe"})) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, kFirstNameRe, first_name_patterns, &v->first_name_,
                   {log_manager, "kFirstNameRe"})) {
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, kMiddleNameRe, middle_name_patterns,
                   &v->middle_name_, {log_manager, "kMiddleNameRe"})) {
      continue;
    }

    if (!v->first_last_name_ &&
        ParseField(scanner, kNameLastFirstRe, first_last_name_patterns,
                   &v->first_last_name_, {log_manager, "kNameLastFirstRe"})) {
      continue;
    }

    if (!v->second_last_name_ &&
        ParseField(scanner, kNameLastSecondRe, second_last_name_patterns,
                   &v->second_last_name_,
                   {log_manager, "kNameLastSecondtRe"})) {
      continue;
    }

    break;
  }

  // Consider the match to be successful if we detected both last names and the
  // surname.
  if (v->first_name_ && v->first_last_name_ && v->second_last_name_)
    return v;

  scanner->Rewind();
  return nullptr;
}

void FirstTwoLastNamesField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(honorific_prefix_, NAME_HONORIFIC_PREFIX,
                    kBaseNameParserScore, field_candidates);
  AddClassification(first_name_, NAME_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(first_last_name_, NAME_LAST_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(second_last_name_, NAME_LAST_SECOND, kBaseNameParserScore,
                    field_candidates);
  const ServerFieldType type =
      middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  AddClassification(middle_name_, type, kBaseNameParserScore, field_candidates);
}

std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseNameSurnameLabelSequence(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  // Some pages have a generic name label that corresponds to a first name
  // followed by a last name label.
  // Example: Name [      ] Last Name [      ]
  auto v = base::WrapUnique(new FirstLastNameField());

  base::span<const MatchPatternRef> name_specific_patterns =
      GetMatchPatterns("NAME_GENERIC", page_language, pattern_source);
  base::span<const MatchPatternRef> middle_name_patterns =
      GetMatchPatterns("MIDDLE_NAME", page_language, pattern_source);
  base::span<const MatchPatternRef> last_name_patterns =
      GetMatchPatterns("LAST_NAME", page_language, pattern_source);
  // Check that the field should not be ignored.

  base::span<const MatchPatternRef> name_ignored_patterns =
      GetMatchPatterns("NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", page_language, pattern_source);
  scanner->SaveCursor();

  bool should_ignore =
      ParseField(scanner, kNameIgnoredRe, name_ignored_patterns, nullptr,
                 {log_manager, "kNameIgnoredRe"}) ||
      ParseField(scanner, kAddressNameIgnoredRe, address_name_ignored_patterns,
                 nullptr, {log_manager, "kAddressNameIgnoredRe"});
  scanner->Rewind();

  scanner->SaveCursor();

  if (should_ignore)
    return nullptr;

  if (ParseField(scanner, kNameGenericRe, name_specific_patterns,
                 &v->first_name_, {log_manager, "kNameGenericRe"})) {
    // Check for an optional middle name field.
    ParseField(scanner, kMiddleNameRe, middle_name_patterns, &v->middle_name_,
               {log_manager, "kMiddleNameRe"});
    if (ParseField(scanner, kLastNameRe, last_name_patterns, &v->last_name_,
                   {log_manager, "kLastNameRe"})) {
      return v;
    }
  }

  scanner->Rewind();
  return nullptr;
}

std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseSharedNameLabelSequence(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  auto v = base::WrapUnique(new FirstLastNameField());
  scanner->SaveCursor();

  raw_ptr<AutofillField> next = nullptr;
  base::span<const MatchPatternRef> name_specific_patterns =
      GetMatchPatterns("NAME_GENERIC", page_language, pattern_source);

  if (ParseField(scanner, kNameGenericRe, name_specific_patterns,
                 &v->first_name_, {log_manager, "kNameGenericRe"}) &&
      ParseEmptyLabel(scanner, &next)) {
    if (ParseEmptyLabel(scanner, &v->last_name_)) {
      // There are three name fields; assume that the middle one is a
      // middle initial (it is, at least, on SmithsonianCheckout.html).
      v->middle_name_ = next;
      v->middle_initial_ = true;
    } else {  // only two name fields
      v->last_name_ = next;
    }

    return v;
  }

  scanner->Rewind();
  return nullptr;
}

// static
std::unique_ptr<FirstLastNameField>
FirstLastNameField::ParseSpecificComponentSequence(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  auto v = base::WrapUnique(new FirstLastNameField());
  scanner->SaveCursor();

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

  base::span<const MatchPatternRef> honorific_prefix_patterns =
      GetMatchPatterns("HONORIFIC_PREFIX", page_language, pattern_source);
  base::span<const MatchPatternRef> name_ignored_patterns =
      GetMatchPatterns("NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> address_name_ignored_patterns =
      GetMatchPatterns("ADDRESS_NAME_IGNORED", page_language, pattern_source);
  base::span<const MatchPatternRef> first_name_patterns =
      GetMatchPatterns("FIRST_NAME", page_language, pattern_source);
  base::span<const MatchPatternRef> middle_name_initial_patterns =
      GetMatchPatterns("MIDDLE_INITIAL", page_language, pattern_source);
  base::span<const MatchPatternRef> middle_name_patterns =
      GetMatchPatterns("MIDDLE_NAME", page_language, pattern_source);
  base::span<const MatchPatternRef> last_name_patterns =
      GetMatchPatterns("LAST_NAME", page_language, pattern_source);

  while (!scanner->IsEnd()) {
    // Skip over address label fields, which can have misleading names
    // e.g. "title" or "name".
    if (ParseField(scanner, kAddressNameIgnoredRe,
                   address_name_ignored_patterns, nullptr,
                   {log_manager, "kAddressNameIgnoredRe"})) {
      continue;
    }

    // Scan for the honorific prefix before checking for unrelated fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    if (!v->honorific_prefix_ &&
        ParseField(scanner, kHonorificPrefixRe, honorific_prefix_patterns,
                   &v->honorific_prefix_,
                   {log_manager, "kHonorificPrefixRe"})) {
      continue;
    }

    // Skip over any unrelated name fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, kNameIgnoredRe,
                            kDefaultMatchParamsWith<MatchFieldType::kSelect,
                                                    MatchFieldType::kSearch>,
                            name_ignored_patterns, nullptr,
                            {log_manager, "kNameIgnoredRe"})) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, kFirstNameRe, first_name_patterns, &v->first_name_,
                   {log_manager, "kFirstNameRe"})) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(scanner, kMiddleInitialRe, middle_name_initial_patterns,
                   &v->middle_name_, {log_manager, "kMiddleInitialRe"})) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, kMiddleNameRe, middle_name_patterns,
                   &v->middle_name_, {log_manager, "kMiddleNameRe"})) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(scanner, kLastNameRe, last_name_patterns, &v->last_name_,
                   {log_manager, "kLastNameRe"})) {
      continue;
    }

    break;
  }

  // Consider the match to be successful if we detected both first and last name
  // fields.
  if (v->first_name_ && v->last_name_)
    return v;

  scanner->Rewind();
  return nullptr;
}

// static
std::unique_ptr<FirstLastNameField> FirstLastNameField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  std::unique_ptr<FirstLastNameField> field = ParseSharedNameLabelSequence(
      scanner, page_language, pattern_source, log_manager);

  if (!field) {
    field = ParseNameSurnameLabelSequence(scanner, page_language,
                                          pattern_source, log_manager);
  }
  if (!field) {
    field = ParseSpecificComponentSequence(scanner, page_language,
                                           pattern_source, log_manager);
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
  AddClassification(last_name_, NAME_LAST, kBaseNameParserScore,
                    field_candidates);
  const ServerFieldType type =
      middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  AddClassification(middle_name_, type, kBaseNameParserScore, field_candidates);
}

}  // namespace autofill
