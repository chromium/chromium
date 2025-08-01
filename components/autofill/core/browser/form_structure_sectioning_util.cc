// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_sectioning_util.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

bool HaveSeenSimilarType(const FieldTypeSet& types,
                         const FieldTypeSet& seen_types) {
  // Forms sometimes have a different format of inputting names in
  // different sections. If we believe a new name is being entered, assume
  // it is a new section.
  static constexpr FieldTypeSet kFirstLastName = {NAME_FIRST, NAME_LAST};
  if ((types.contains(NAME_FULL) && seen_types.contains_any(kFirstLastName)) ||
      (types.contains_any(kFirstLastName) && seen_types.contains(NAME_FULL))) {
    return true;
  }
  return seen_types.contains_any(types);
}

// Some forms have adjacent fields of the same or very similar type. These
// generally belong in the same section. Common examples:
//  * Forms with two email fields, where the second is meant to "confirm"
//    the first.
//  * Forms with a <select> field for states in some countries, and a freeform
//    <input> field for states in other countries. (Usually, only one of these
//    two will be visible for any given choice of country.)
//  * In Japan, forms commonly have separate inputs for phonetic names. In
//    practice this means consecutive name field types (e.g. first name and last
//    name).
bool ConsecutiveSimilarFieldType(const AutofillType& current_type,
                                 const AutofillType& previous_type) {
  const FieldTypeSet common_types =
      Intersection(current_type.GetTypes(), previous_type.GetTypes());
  if (!common_types.empty()) {
    return true;
  }
  if (current_type.GetGroups().contains(FieldTypeGroup::kName) &&
      previous_type.GetGroups().contains(FieldTypeGroup::kName)) {
    return true;
  }
  static constexpr FieldTypeSet kSimilar = {
      ADDRESS_HOME_ZIP,        ADDRESS_HOME_ZIP_PREFIX,
      ADDRESS_HOME_ZIP_SUFFIX, ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,       ADDRESS_HOME_ADMIN_LEVEL2,
      ADDRESS_HOME_STATE,      ADDRESS_HOME_COUNTRY};
  if (kSimilar.contains_any(previous_type.GetTypes()) &&
      kSimilar.contains_any(current_type.GetTypes())) {
    return true;
  }
  return false;
}

// Sectionable fields are all the fields that are in a non-default section.
// Generally, only focusable fields are assigned a section. As an exception,
// unfocusable <select> elements get a section, as hidden <select> elements are
// common in custom select elements. To confine the impact of hidden <select>
// elements, this exception only applies if their type is actually autofillable.
bool IsSectionable(const AutofillField& field) {
  return field.IsFocusable() ||
         (field.IsSelectElement() && field.IsFieldFillable());
}

// Assign all credit card fields without a valid autocomplete attribute section
// to one, separate section based on the first credit card field.
void AssignCreditCardSections(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  auto first_cc_field = std::ranges::find_if(
      fields, [](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetGroups().contains(
                   FieldTypeGroup::kCreditCard) &&
               !field->section();
      });
  if (first_cc_field == fields.end())
    return;
  Section cc_section =
      Section::FromFieldIdentifier(**first_cc_field, frame_token_ids);
  for (const auto& field : fields) {
    if (field->Type().GetGroups().contains(FieldTypeGroup::kCreditCard) &&
        !field->section()) {
      field->set_section(cc_section);
    }
  }
}

void AssignAutocompleteSections(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  for (const auto& field : fields) {
    if (field->parsed_autocomplete()) {
      Section autocomplete_section = Section::FromAutocomplete(
          {.section = field->parsed_autocomplete()->section,
           .mode = field->parsed_autocomplete()->mode});
      if (autocomplete_section)
        field->set_section(autocomplete_section);
    }
  }
}

void AssignFieldIdentifierSections(
    base::span<const std::unique_ptr<AutofillField>> section,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  if (section.empty())
    return;
  Section s = Section::FromFieldIdentifier(**section.begin(), frame_token_ids);
  for (const auto& field : section) {
    if (!field->section() && IsSectionable(*field)) {
      field->set_section(s);
    }
  }
}

bool BelongsToCurrentSection(const FieldTypeSet& seen_types,
                             const AutofillField& current_field,
                             const AutofillField& previous_field) {
  if (current_field.section()) {
    return true;
  }

  const AutofillType current_type = current_field.Type();
  if (current_type.GetTypes().contains(UNKNOWN_TYPE)) {
    return true;
  }

  // Generally, adjacent fields of the same or very similar type belong in the
  // same logical section.
  if (ConsecutiveSimilarFieldType(current_type, previous_field.Type())) {
    return true;
  }

  // There are many phone number field types and their classification is
  // generally a little bit off. Furthermore, forms often ask for multiple phone
  // numbers, e.g. both a daytime and evening phone number.
  if (current_type.GetGroups().contains(FieldTypeGroup::kPhone)) {
    return true;
  }

  return !HaveSeenSimilarType(current_type.GetTypes(), seen_types);
}

// Finds the first focusable field that doesn't have a section assigned.
//
// We look for a focusable, rather than sectionable, field because starting a
// section too early may also lead to finishing the section too early. In
// particular, if a hidden section is followed by an identical visible section
// and these sections contain a <select>, then the first (invisible) <select>
// would start a section and the equivalent, visible <select> would erroneously
// finish the section due to the repeated type.
base::span<const std::unique_ptr<AutofillField>>::iterator
FindBeginOfNextSection(
    base::span<const std::unique_ptr<AutofillField>>::iterator begin,
    base::span<const std::unique_ptr<AutofillField>>::iterator end) {
  while (begin != end && ((*begin)->section() || !(*begin)->IsFocusable())) {
    begin++;
  }
  return begin;
}

// Finds the longest prefix of [begin, end) that belongs to the same section,
// according to `BelongsToCurrentSection()`.
base::span<const std::unique_ptr<AutofillField>>::iterator FindEndOfNextSection(
    base::span<const std::unique_ptr<AutofillField>>::iterator begin,
    base::span<const std::unique_ptr<AutofillField>>::iterator end) {
  // Keeps track of the focusable types we've seen in this section.
  FieldTypeSet seen_types;
  // The `prev_field` is from the section whose end we are currently searching.
  const AutofillField* prev_field = nullptr;
  for (auto it = begin; it != end; it++) {
    const AutofillField& field = **it;
    if (!IsSectionable(field))
      continue;
    if (prev_field &&
        !BelongsToCurrentSection(seen_types, field, *prev_field)) {
      return it;
    }
    if (!field.section()) {
      seen_types.insert_all(field.Type().GetTypes());
      prev_field = &field;
    }
  }
  return end;
}

}  // namespace

void AssignSections(base::span<const std::unique_ptr<AutofillField>> fields) {
  for (const auto& field : fields)
    field->set_section(Section());

  // Create a unique identifier based on the field for the section.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;

  AssignAutocompleteSections(fields);
  AssignCreditCardSections(fields, frame_token_ids);

  auto begin = fields.begin();
  while (begin != fields.end()) {
    begin = FindBeginOfNextSection(begin, fields.end());
    auto end = FindEndOfNextSection(begin, fields.end());
    DCHECK(begin != end || end == fields.end());
    // SAFETY: The iterators are from the same container.
    AssignFieldIdentifierSections(UNSAFE_BUFFERS({begin, end}),
                                  frame_token_ids);
    begin = end;
  }
}

}  // namespace autofill
