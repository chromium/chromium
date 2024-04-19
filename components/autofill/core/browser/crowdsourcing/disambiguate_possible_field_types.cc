// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/disambiguate_possible_field_types.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Returns `true` if `field` contains multiple possible types.
bool MayHaveAmbiguousPossibleTypes(const AutofillField& field) {
  return field.possible_types().size() > 1;
}

// Returns whether the `field` is predicted as being any kind of name.
bool IsNameType(const AutofillField& field) {
  return field.Type().group() == FieldTypeGroup::kName ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FULL ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FIRST ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_LAST;
}

// Selects the probable name types for the possible field types of the `field`.
// If `is_credit_card` is true, credit card name types are retained and
// address name types are discarded. If `is_credit_card` is false, the opposite
// happens. This is called when we have multiple possible name types from mixed
// field type groups.
void SelectProbableNameTypes(AutofillField& field,
                             bool select_credit_card_names) {
  FieldTypeSet types_to_keep;
  for (FieldType type : field.possible_types()) {
    FieldTypeGroup group = GroupTypeOfFieldType(type);
    if ((select_credit_card_names && group == FieldTypeGroup::kCreditCard) ||
        (!select_credit_card_names && group == FieldTypeGroup::kName)) {
      types_to_keep.insert(type);
    }
  }

  field.set_possible_types(types_to_keep);
}

// If a field was autofilled on form submission and the value was accepted, set
// possible types to the autofilled type.
void SetPossibleTypesToAutofilledTypeIfAvailable(AutofillField& field) {
  if (field.is_autofilled() && field.autofilled_type() &&
      base::FeatureList::IsEnabled(
          features::kAutofillDisambiguateContradictingFieldTypes)) {
    field.set_possible_types({*field.autofilled_type()});
  }
}

// Disambiguates name types from mixed field type groups when the name exists in
// both a profile and a credit card. `current_field_index` is the index of the
// field whose possible types are attempted to be disambiguated.
// Note that generally a name field can legitimately have multiple types
// according to the types tree structure, e.g. `NAME_FULL`, `NAME_LAST` and
// `NAME_LAST_{FIRST,SECOND}` at the same time.
void MaybeDisambiguateNameTypes(FormStructure& form,
                                size_t current_field_index) {
  // Disambiguation is only applicable if there is a mixture of one or more
  // address related name types ('one or more' because e.g. NAME_LAST and
  // NAME_LAST_SECOND can be identical) and exactly one credit card related name
  // type ('exactly one' because credit cards names only support a single last
  // name).
  AutofillField& field = *form.field(current_field_index);
  const size_t credit_card_type_count =
      NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kCreditCard);
  const size_t name_type_count =
      NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kName);
  if (field.possible_types().size() !=
          (credit_card_type_count + name_type_count) ||
      credit_card_type_count != 1 || name_type_count == 0) {
    return;
  }

  // This case happens when both a profile and a credit card have the same
  // name, and when we have exactly two possible types.

  // If the ambiguous field has either a previous or next field that is
  // not name related, use that information to determine whether the field
  // is a name or a credit card name.
  // If the ambiguous field has both a previous or next field that is not
  // name related, if they are both from the same group, use that group to
  // decide this field's type. Otherwise, there is no safe way to
  // disambiguate.

  // Look for a previous non name related field.
  bool has_found_previous_type = false;
  bool is_previous_credit_card = false;
  size_t index = current_field_index;
  while (index != 0 && !has_found_previous_type) {
    --index;
    AutofillField* prev_field = form.field(index);
    if (!IsNameType(*prev_field)) {
      has_found_previous_type = true;
      is_previous_credit_card =
          prev_field->Type().group() == FieldTypeGroup::kCreditCard;
    }
  }

  // Look for a next non name related field.
  bool has_found_next_type = false;
  bool is_next_credit_card = false;
  index = current_field_index;
  while (++index < form.field_count() && !has_found_next_type) {
    AutofillField* next_field = form.field(index);
    if (!IsNameType(*next_field)) {
      has_found_next_type = true;
      is_next_credit_card =
          next_field->Type().group() == FieldTypeGroup::kCreditCard;
    }
  }

  // At least a previous or next field type must have been found in order to
  // disambiguate this field.
  if (has_found_previous_type || has_found_next_type) {
    // If both a previous type and a next type are found and not from the same
    // name group there is no sure way to disambiguate.
    if (has_found_previous_type && has_found_next_type &&
        (is_previous_credit_card != is_next_credit_card)) {
      return;
    }

    // Otherwise, use the previous (if it was found) or next field group to
    // decide whether the field is a name or a credit card name.
    if (has_found_previous_type) {
      SelectProbableNameTypes(field, is_previous_credit_card);
    } else {
      SelectProbableNameTypes(field, is_next_credit_card);
    }
  }
}

}  // namespace

void DisambiguatePossibleFieldTypes(FormStructure& form) {
  for (size_t i = 0; i < form.field_count(); ++i) {
    AutofillField& field = *form.field(i);
    if (!MayHaveAmbiguousPossibleTypes(field)) {
      continue;
    }
    // Setting possible types to an autofilled value that was accepted by the
    // user should have the highest priority among disambiguation methods
    // because it represents user intent the most.
    SetPossibleTypesToAutofilledTypeIfAvailable(field);

    if (!MayHaveAmbiguousPossibleTypes(field)) {
      continue;
    }
    MaybeDisambiguateNameTypes(form, i);
  }
}

}  // namespace autofill
