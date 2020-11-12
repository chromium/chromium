// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field.h"

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"

using base::UTF8ToUTF16;

namespace autofill {
namespace {

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  static std::unique_ptr<FullNameField> Parse(AutofillScanner* scanner,
                                              const std::string& page_language,
                                              LogManager* log_manager);
  explicit FullNameField(AutofillField* field);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(FullNameField);
};

// A form field that parses a first name field and two last name fields as they
// are used in Hispanic/Latinx names.
class FirstTwoLastNamesField : public NameField {
 public:
  static std::unique_ptr<FirstTwoLastNamesField> ParseComponentNames(
      AutofillScanner* scanner,
      const std::string& page_language,
      LogManager* log_manager);
  static std::unique_ptr<FirstTwoLastNamesField> Parse(
      AutofillScanner* scanner,
      const std::string& page_language,
      LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  FirstTwoLastNamesField();

  AutofillField* honorific_prefix_{nullptr};  // Optional.
  AutofillField* first_name_{nullptr};
  AutofillField* middle_name_{nullptr};  // Optional.
  AutofillField* first_last_name_{nullptr};
  AutofillField* second_last_name_{nullptr};
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstTwoLastNamesField);
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameField {
 public:
  static std::unique_ptr<FirstLastNameField> ParseSpecificName(
      AutofillScanner* scanner,
      const std::string& page_language,
      LogManager* log_manager);
  static std::unique_ptr<FirstLastNameField> ParseComponentNames(
      AutofillScanner* scanner,
      const std::string& page_language,
      LogManager* log_manager);
  static std::unique_ptr<FirstLastNameField> Parse(
      AutofillScanner* scanner,
      const std::string& page_language,
      LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  FirstLastNameField();

  AutofillField* honorific_prefix_{nullptr};  // Optional
  AutofillField* first_name_{nullptr};
  AutofillField* middle_name_{nullptr};  // Optional.
  AutofillField* last_name_{nullptr};
  bool middle_initial_{false};  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstLastNameField);
};

}  // namespace

// static
std::unique_ptr<FormField> NameField::Parse(AutofillScanner* scanner,
                                            const std::string& page_language,
                                            LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  // Try |FirstLastNameField| and |FirstTwoLastNamesField| first since they are
  // more specific.
  std::unique_ptr<FormField> field;
  if (!field && base::FeatureList::IsEnabled(
                    features::kAutofillEnableSupportForMoreStructureInNames))
    field = FirstTwoLastNamesField::Parse(scanner, page_language, log_manager);
  if (!field)
    field = FirstLastNameField::Parse(scanner, page_language, log_manager);
  if (!field)
    field = FullNameField::Parse(scanner, page_language, log_manager);
  return field;
}

// This is overriden in concrete subclasses.
void NameField::AddClassifications(FieldCandidatesMap* field_candidates) const {
}

// static
std::unique_ptr<FullNameField> FullNameField::Parse(
    AutofillScanner* scanner,
    const std::string& page_language,
    LogManager* log_manager) {
  // Exclude e.g. "username" or "nickname" fields.
  scanner->SaveCursor();
  auto& patterns_ni = PatternProvider::GetInstance().GetMatchPatterns(
      "NAME_IGNORED", page_language);
  bool should_ignore =
      ParseField(scanner, UTF8ToUTF16(kNameIgnoredRe), patterns_ni, nullptr,
                 {log_manager, "kNameIgnoredRe"});
  scanner->Rewind();
  if (should_ignore)
    return nullptr;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  AutofillField* field = nullptr;
  // In JSON : FULL_NAME (closest vatiant)
  auto& patterns_name = PatternProvider::GetInstance().GetMatchPatterns(
      "FULL_NAME", page_language);
  if (ParseField(scanner, UTF8ToUTF16(kNameRe), patterns_name, &field,
                 {log_manager, "kNameRe"}))
    return std::make_unique<FullNameField>(field);

  return nullptr;
}

void FullNameField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, NAME_FULL, kBaseNameParserScore, field_candidates);
}

FullNameField::FullNameField(AutofillField* field) : field_(field) {}

FirstTwoLastNamesField::FirstTwoLastNamesField() = default;

// static
std::unique_ptr<FirstTwoLastNamesField> FirstTwoLastNamesField::Parse(
    AutofillScanner* scanner,
    const std::string& page_language,
    LogManager* log_manager) {
  return ParseComponentNames(scanner, page_language, log_manager);
}

// static
std::unique_ptr<FirstTwoLastNamesField>
FirstTwoLastNamesField::ParseComponentNames(AutofillScanner* scanner,
                                            const std::string& page_language,
                                            LogManager* log_manager) {
  std::unique_ptr<FirstTwoLastNamesField> v(new FirstTwoLastNamesField);
  scanner->SaveCursor();

  auto& patterns_hp = PatternProvider::GetInstance().GetMatchPatterns(
      "HONORIFIC_PREFIX", page_language);
  auto& patterns_ni = PatternProvider::GetInstance().GetMatchPatterns(
      "NAME_IGNORED", page_language);
  auto& patterns_fn = PatternProvider::GetInstance().GetMatchPatterns(
      "FIRST_NAME", page_language);
  auto& patterns_mn = PatternProvider::GetInstance().GetMatchPatterns(
      "MIDDLE_NAME", page_language);
  auto& patterns_ln1 = PatternProvider::GetInstance().GetMatchPatterns(
      "LAST_NAME_FIRST", page_language);
  auto& patterns_ln2 = PatternProvider::GetInstance().GetMatchPatterns(
      "LAST_NAME_SECOND", page_language);

  // Allow name fields to appear in any order.
  while (!scanner->IsEnd()) {
    // Scan for the honorific prefix before checking for unrelated name fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    // TODO(crbug.com/1098943): Remove check once feature is launched or
    // removed.
    if (!v->honorific_prefix_ &&
        ParseField(scanner, UTF8ToUTF16(kHonorificPrefixRe), patterns_hp,
                   &v->honorific_prefix_,
                   {log_manager, "kHonorificPrefixRe"})) {
      continue;
    }

    // Skip over any unrelated fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kNameIgnoredRe),
                            MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
                            patterns_ni, nullptr,
                            {log_manager, "kNameIgnoredRe"})) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, UTF8ToUTF16(kFirstNameRe), patterns_fn,
                   &v->first_name_, {log_manager, "kFirstNameRe"})) {
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleNameRe), patterns_mn,
                   &v->middle_name_, {log_manager, "kMiddleNameRe"})) {
      continue;
    }

    if (!v->first_last_name_ &&
        ParseField(scanner, UTF8ToUTF16(kNameLastFirstRe), patterns_ln1,
                   &v->first_last_name_, {log_manager, "kNameLastFirstRe"})) {
      continue;
    }

    if (!v->second_last_name_ &&
        ParseField(scanner, UTF8ToUTF16(kNameLastSecondRe), patterns_ln2,
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
    FieldCandidatesMap* field_candidates) const {
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

std::unique_ptr<FirstLastNameField> FirstLastNameField::ParseSpecificName(
    AutofillScanner* scanner,
    const std::string& page_language,
    LogManager* log_manager) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  std::unique_ptr<FirstLastNameField> v(new FirstLastNameField);
  scanner->SaveCursor();

  AutofillField* next = nullptr;
  auto& patterns_ns = PatternProvider::GetInstance().GetMatchPatterns(
      "NAME_SPECIFIC", page_language);

  if (ParseField(scanner, UTF8ToUTF16(kNameSpecificRe), patterns_ns,
                 &v->first_name_, {log_manager, "kNameSpecificRe"}) &&
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
std::unique_ptr<FirstLastNameField> FirstLastNameField::ParseComponentNames(
    AutofillScanner* scanner,
    const std::string& page_language,
    LogManager* log_manager) {
  std::unique_ptr<FirstLastNameField> v(new FirstLastNameField);
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

  auto& patterns_hp = PatternProvider::GetInstance().GetMatchPatterns(
      "HONORIFIC_PREFIX", page_language);
  auto& patterns_ni = PatternProvider::GetInstance().GetMatchPatterns(
      "NAME_IGNORED", page_language);
  auto& patterns_fn = PatternProvider::GetInstance().GetMatchPatterns(
      "FIRST_NAME", page_language);
  auto& patterns_mi = PatternProvider::GetInstance().GetMatchPatterns(
      "MIDDLE_INITIAL", page_language);
  auto& patterns_mn = PatternProvider::GetInstance().GetMatchPatterns(
      "MIDDLE_NAME", page_language);
  auto& patterns_ln = PatternProvider::GetInstance().GetMatchPatterns(
      "LAST_NAME", page_language);

  while (!scanner->IsEnd()) {
    // Scan for the honorific prefix before checking for unrelated fields
    // because a honorific prefix field is expected to have very specific labels
    // including "Title:". The latter is matched with |kNameIgnoredRe|.
    // TODO(crbug.com/1098943): Remove branching once feature is launched or
    // removed.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForMoreStructureInNames)) {
      if (!v->honorific_prefix_ &&
          ParseField(scanner, UTF8ToUTF16(kHonorificPrefixRe), patterns_hp,
                     &v->honorific_prefix_,
                     {log_manager, "kHonorificPrefixRe"})) {
        continue;
      }
    }

    // Skip over any unrelated name fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kNameIgnoredRe),
                            MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
                            patterns_ni, nullptr,
                            {log_manager, "kNameIgnoredRe"})) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, UTF8ToUTF16(kFirstNameRe), patterns_fn,
                   &v->first_name_, {log_manager, "kFirstNameRe"})) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleInitialRe), patterns_mi,
                   &v->middle_name_, {log_manager, "kMiddleInitialRe"})) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleNameRe), patterns_mn,
                   &v->middle_name_, {log_manager, "kMiddleNameRe"})) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(scanner, UTF8ToUTF16(kLastNameRe), patterns_ln,
                   &v->last_name_, {log_manager, "kLastNameRe"})) {
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
    const std::string& page_language,
    LogManager* log_manager) {
  std::unique_ptr<FirstLastNameField> field =
      ParseSpecificName(scanner, page_language, log_manager);
  if (!field)
    field = ParseComponentNames(scanner, page_language, log_manager);
  return field;
}

FirstLastNameField::FirstLastNameField() = default;

void FirstLastNameField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
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
