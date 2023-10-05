// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_autofill_history.h"

#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

FormAutofillHistory::FieldFillingEntry
FormAutofillHistory::FillOperation::GetAutofillValue(
    FieldGlobalId field_id) const {
  auto it = iterator_->find(field_id);
  CHECK(it != iterator_->end());
  return it->second;
}

FormAutofillHistory::FormAutofillHistory() = default;

FormAutofillHistory::~FormAutofillHistory() = default;

void FormAutofillHistory::AddFormFillEntry(
    base::span<const FormFieldData* const> filled_fields,
    bool is_refill) {
  // Intuitively, `if (!is_refill) history_.emplace_front()` suffices, but it
  // does not handle these corner cases correctly:
  // - If the original fill had `filled_fields.size() >
  //   kMaxStorableFieldFillHistory`, then `history_` might be empty.
  // - If a previous fill had `filled_fields.empty()`, we could save memory.
  if (history_.empty() || (!is_refill && !history_.front().empty())) {
    history_.emplace_front();
  }
  for (const FormFieldData* field : filled_fields) {
    // During refills, a field that was previously filled in the original
    // fill operation, with initial value `A` and filled value `B`, might be
    // refilled with a newer value `C`. We do not store this so that upon
    // undoing Autofill, the field's value reverts from `C` to `A` directly as
    // this is what happened from a user's perspective.
    size_ += history_.front()
                 .emplace(field->global_id(),
                          FieldFillingEntry(field->value, field->is_autofilled))
                 .second;
  }
  // Drop the last history entry while the history size exceeds the limit.
  while (size_ > kMaxStorableFieldFillHistory) {
    EraseFormFillEntry(FillOperation(--history_.end()));
  }
}

void FormAutofillHistory::EraseFormFillEntry(FillOperation fill_operation) {
  size_ -= fill_operation.iterator_->size();
  history_.erase(fill_operation.iterator_);
}

FormAutofillHistory::FillOperation
FormAutofillHistory::GetLastFillingOperationForField(
    FieldGlobalId field_id) const {
  return FillOperation(base::ranges::find_if(
      history_, [&field_id](const FormFillingEntry& operation) {
        return operation.contains(field_id);
      }));
}

bool FormAutofillHistory::HasHistory(FieldGlobalId field_id) const {
  return GetLastFillingOperationForField(field_id).iterator_ != history_.end();
}

void FormAutofillHistory::Reset() {
  size_ = 0;
  history_.clear();
}
}  // namespace autofill
