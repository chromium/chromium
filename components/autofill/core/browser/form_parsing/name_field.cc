// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

using base::UTF8ToUTF16;

namespace autofill {
namespace {

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  static std::unique_ptr<FullNameField> Parse(AutofillScanner* scanner,
                                              LogManager* log_manager);
  explicit FullNameField(AutofillField* field);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(FullNameField);
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameField {
 public:
  static std::unique_ptr<FirstLastNameField> ParseSpecificName(
      AutofillScanner* scanner,
      LogManager* log_manager);
  static std::unique_ptr<FirstLastNameField> ParseComponentNames(
      AutofillScanner* scanner,
      LogManager* log_manager);
  static std::unique_ptr<FirstLastNameField> Parse(AutofillScanner* scanner,
                                                   LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  FirstLastNameField();

  AutofillField* first_name_;
  AutofillField* middle_name_;  // Optional.
  AutofillField* last_name_;
  bool middle_initial_;  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstLastNameField);
};

}  // namespace

// static
std::unique_ptr<FormField> NameField::Parse(AutofillScanner* scanner,
                                            LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  // Try FirstLastNameField first since it's more specific.
  std::unique_ptr<FormField> field =
      FirstLastNameField::Parse(scanner, log_manager);
  if (!field)
    field = FullNameField::Parse(scanner, log_manager);
  return field;
}

// This is overriden in concrete subclasses.
void NameField::AddClassifications(FieldCandidatesMap* field_candidates) const {
}

// static
std::unique_ptr<FullNameField> FullNameField::Parse(AutofillScanner* scanner,
                                                    LogManager* log_manager) {
  // Exclude e.g. "username" or "nickname" fields.
  scanner->SaveCursor();
  bool should_ignore = ParseField(scanner, UTF8ToUTF16(kNameIgnoredRe), nullptr,
                                  {log_manager, "kNameIgnoredRe"});
  scanner->Rewind();
  if (should_ignore)
    return nullptr;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  AutofillField* field = nullptr;
  if (ParseField(scanner, UTF8ToUTF16(kNameRe), &field,
                 {log_manager, "kNameRe"}))
    return std::make_unique<FullNameField>(field);

  return nullptr;
}

void FullNameField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, NAME_FULL, kBaseNameParserScore, field_candidates);
}

FullNameField::FullNameField(AutofillField* field) : field_(field) {}

std::unique_ptr<FirstLastNameField> FirstLastNameField::ParseSpecificName(
    AutofillScanner* scanner,
    LogManager* log_manager) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  std::unique_ptr<FirstLastNameField> v(new FirstLastNameField);
  scanner->SaveCursor();

  AutofillField* next = nullptr;
  if (ParseField(scanner, UTF8ToUTF16(kNameSpecificRe), &v->first_name_,
                 {log_manager, "kNameSpecificRe"}) &&
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
  while (!scanner->IsEnd()) {
    // Skip over any unrelated fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kNameIgnoredRe),
                            MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
                            nullptr, {log_manager, "kNameIgnoredRe"})) {
      continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, UTF8ToUTF16(kFirstNameRe), &v->first_name_,
                   {log_manager, "kFirstNameRe"})) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleInitialRe), &v->middle_name_,
                   {log_manager, "kMiddleInitialRe"})) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleNameRe), &v->middle_name_,
                   {log_manager, "kMiddleNameRe"})) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(scanner, UTF8ToUTF16(kLastNameRe), &v->last_name_,
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
    LogManager* log_manager) {
  std::unique_ptr<FirstLastNameField> field =
      ParseSpecificName(scanner, log_manager);
  if (!field)
    field = ParseComponentNames(scanner, log_manager);
  return field;
}

FirstLastNameField::FirstLastNameField()
    : first_name_(nullptr),
      middle_name_(nullptr),
      last_name_(nullptr),
      middle_initial_(false) {}

void FirstLastNameField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(first_name_, NAME_FIRST, kBaseNameParserScore,
                    field_candidates);
  AddClassification(last_name_, NAME_LAST, kBaseNameParserScore,
                    field_candidates);
  const ServerFieldType type =
      middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  AddClassification(middle_name_, type, kBaseNameParserScore, field_candidates);
}

}  // namespace autofill
