// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_sectioning_util.h"

#include <iterator>
#include <memory>
#include <utility>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

bool HaveSeenSimilarType(ServerFieldType type,
                         const ServerFieldTypeSet& seen_types) {
  // Forms sometimes have a different format of inputting names in
  // different sections. If we believe a new name is being entered, assume
  // it is a new section.
  ServerFieldTypeSet first_last_name = {NAME_FIRST, NAME_LAST};
  if ((type == NAME_FULL && seen_types.contains_any(first_last_name)) ||
      (first_last_name.contains(type) && seen_types.contains(NAME_FULL))) {
    return true;
  }
  return seen_types.count(type) > 0;
}

// Sectionable fields are all the fields that are in a non-default section.
// Generally, only focusable fields are assigned a section. As an exception,
// unfocusable <select> elements get a section, as hidden <select> elements are
// common in custom select elements.
bool IsSectionable(const AutofillField& field) {
  return field.IsFocusable() || field.form_control_type == "select-one";
}

// Assign all credit card fields without a valid autocomplete attribute section
// to one, separate section based on the first credit card field.
void AssignCreditCardSections(
    base::span<std::unique_ptr<AutofillField>> fields,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  auto first_cc_field = base::ranges::find_if(
      fields, [&](const std::unique_ptr<AutofillField>& field) {
        return field->Type().group() == FieldTypeGroup::kCreditCard &&
               !field->section;
      });
  if (first_cc_field == fields.end())
    return;
  Section cc_section =
      Section::FromFieldIdentifier(**first_cc_field, frame_token_ids);
  for (const auto& field : fields) {
    if (field->Type().group() == FieldTypeGroup::kCreditCard && !field->section)
      field->section = cc_section;
  }
}

void AssignAutocompleteSections(
    base::span<std::unique_ptr<AutofillField>> fields) {
  for (const auto& field : fields) {
    if (field->parsed_autocomplete) {
      Section autocomplete_section = Section::FromAutocomplete(
          {.section = field->parsed_autocomplete->section,
           .mode = field->parsed_autocomplete->mode});
      if (autocomplete_section)
        field->section = autocomplete_section;
    }
  }
}

void AssignFieldIdentifierSections(
    base::span<std::unique_ptr<AutofillField>> section,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  Section s = Section::FromFieldIdentifier(**section.begin(), frame_token_ids);
  for (const auto& field : section) {
    if (!field->section && IsSectionable(*field))
      field->section = s;
  }
}

bool ShouldStartNewSection(const ServerFieldTypeSet& seen_types,
                           const AutofillField& current_field,
                           const AutofillField& previous_field) {
  DCHECK(!previous_field.section);
  DCHECK(seen_types.contains(previous_field.Type().GetStorableType()));

  const ServerFieldType current_type = current_field.Type().GetStorableType();
  if (current_type == UNKNOWN_TYPE)
    return false;

  // Some forms have adjacent fields of the same type. Two common examples:
  //  * Forms with two email fields, where the second is meant to "confirm"
  //    the first.
  //  * Forms with a <select> menu for states in some countries, and a freeform
  //    <input> field for states in other countries. (Usually, only one  of
  //    these two will be visible for any given choice of country.)
  // Generally, adjacent fields of the same type belong in the same logical
  // section.
  if (current_type == previous_field.Type().GetStorableType()) {
    return false;
  }

  // There are many phone number field types and their classification is
  // generally a little bit off. Furthermore, forms often ask for multiple phone
  // numbers, e.g. both a daytime and evening phone number.
  if (AutofillType(current_type).group() == FieldTypeGroup::kPhoneHome)
    return false;

  return HaveSeenSimilarType(current_type, seen_types);
}

// Finds the longest prefix of [begin, end) that belongs to the same section,
// according to `ShouldStartNewSection()`.
base::span<std::unique_ptr<AutofillField>>::iterator FindEndOfNextSection(
    base::span<std::unique_ptr<AutofillField>>::iterator begin,
    base::span<std::unique_ptr<AutofillField>>::iterator end) {
  // Keeps track of the focusable types we've seen in this section.
  ServerFieldTypeSet seen_types;
  // The `prev_field` is from the section whose end we are currently searching.
  const AutofillField* prev_field = nullptr;
  for (auto it = begin; it != end; it++) {
    const AutofillField& field = **it;
    if (field.section || !IsSectionable(field))
      continue;
    if (prev_field && ShouldStartNewSection(seen_types, field, *prev_field))
      return it;
    seen_types.insert(field.Type().GetStorableType());
    prev_field = &field;
  }
  return end;
}

}  // namespace

void AssignSections(base::span<std::unique_ptr<AutofillField>> fields) {
  for (const auto& field : fields)
    field->section = Section();

  // Create a unique identifier based on the field for the section.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;

  AssignAutocompleteSections(fields);
  AssignCreditCardSections(fields, frame_token_ids);

  auto begin = fields.begin();
  while (begin != fields.end()) {
    auto end = FindEndOfNextSection(begin, fields.end());
    AssignFieldIdentifierSections({begin, end}, frame_token_ids);
    begin = end;
  }
}

}  // namespace autofill
