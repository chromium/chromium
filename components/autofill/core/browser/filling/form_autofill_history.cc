// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/form_autofill_history.h"

#include <algorithm>

#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

FormAutofillHistory::FieldFillingEntry::FieldFillingEntry(
    std::u16string field_value,
    bool field_is_autofilled,
    std::optional<std::string> field_autofill_source_profile_guid,
    std::optional<FieldType> field_autofilled_type,
    FillingProduct field_filling_product,
    bool ignore_is_autofilled)
    : value(field_value),
      is_autofilled(field_is_autofilled),
      autofill_source_profile_guid(
          std::move(field_autofill_source_profile_guid)),
      autofilled_type(std::move(field_autofilled_type)),
      filling_product(field_filling_product),
      ignore_is_autofilled(ignore_is_autofilled) {}

FormAutofillHistory::FieldFillingEntry::~FieldFillingEntry() = default;

FormAutofillHistory::FieldFillingEntry::FieldFillingEntry(
    const FieldFillingEntry&) = default;

FormAutofillHistory::FieldFillingEntry::FieldFillingEntry(FieldFillingEntry&&) =
    default;

FormAutofillHistory::FormAutofillHistory() = default;

FormAutofillHistory::~FormAutofillHistory() = default;

void FormAutofillHistory::AddFormFillingEntry(
    base::span<const FormFieldData* const> filled_fields,
    base::span<const AutofillField* const> filled_autofill_fields,
    FillingProduct filling_product,
    bool is_refill) {
  // Intuitively, `if (!is_refill) history_.emplace_front()` suffices, but it
  // does not handle these corner cases correctly:
  // - If the original fill had `filled_fields.size() >
  //   kMaxStorableFieldFillHistory`, then `history_` might be empty.
  // - If a previous fill had `filled_fields.empty()`, we could save memory.
  if (history_.empty() || (!is_refill && !history_.front().empty())) {
    history_.emplace_front();
  }

  for (const auto [field, autofill_field] :
       base::zip(filled_fields, filled_autofill_fields)) {
    // During refills, a field that was previously filled in the original
    // fill operation, with initial value `A` and filled value `B`, might be
    // refilled with a newer value `C`. We do not store this so that upon
    // undoing Autofill, the field's value reverts from `C` to `A` directly as
    // this is what happened from a user's perspective.
    size_ +=
        history_.front()
            .emplace(field->global_id(),
                     FieldFillingEntry(
                         field->value(), field->is_autofilled(),
                         autofill_field->autofill_source_profile_guid(),
                         autofill_field->autofilled_type(),
                         autofill_field->filling_product(),
                         // `FormAutofillHistory::AddFormFillingEntry` only gets
                         // fields that were autofilled. In case a field has an
                         // empty value, this means Autofill intentionally
                         // filled an empty string into it, and therefore when
                         // undoing changes to this field we should not look at
                         // `FormFieldData::is_autofilled`.
                         /*ignore_is_autofilled=*/field->value().empty()))
            .second;
  }
  // Drop the last history entry while the history size exceeds the limit.
  while (size_ > kMaxStorableFieldFillHistory) {
    EraseFormFillEntry(--history_.end());
  }
}

void FormAutofillHistory::EraseFieldFillingEntry(
    std::list<FormFillingEntry>::iterator fill_operation,
    FieldGlobalId field_id) {
  fill_operation->erase(field_id);
  if (fill_operation->empty()) {
    EraseFormFillEntry(fill_operation);
  }
}

void FormAutofillHistory::EraseFormFillEntry(
    std::list<FormFillingEntry>::iterator filling_entry) {
  size_ -= filling_entry->size();
  history_.erase(filling_entry);
}

std::list<FormAutofillHistory::FormFillingEntry>::iterator
FormAutofillHistory::GetLastFormFillingEntryForField(FieldGlobalId field_id) {
  return std::ranges::find_if(history_,
                              [&field_id](const FormFillingEntry& operation) {
                                return operation.contains(field_id);
                              });
}

bool FormAutofillHistory::HasHistory(FieldGlobalId field_id) const {
  return GetLastFormFillingEntryForField(field_id) != history_.end();
}

void FormAutofillHistory::Reset() {
  size_ = 0;
  history_.clear();
}
}  // namespace autofill
