// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_autofill_history.h"

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {

FormAutofillHistory::AutofillHistoryEntry::AutofillHistoryEntry() = default;

FormAutofillHistory::AutofillHistoryEntry::~AutofillHistoryEntry() = default;

base::flat_map<FieldGlobalId, ServerFieldType>
FormAutofillHistory::FillOperation::GetFieldTypeMap() const {
  return base::MakeFlatMap<FieldGlobalId, ServerFieldType>(
      iterator_->field_history_, {}, [](const auto& field_fill_entry) {
        const auto& [field_id, field_info] = field_fill_entry;
        return std::make_pair(field_id, field_info.type);
      });
}

const std::u16string* FormAutofillHistory::FillOperation::GetValue(
    FieldGlobalId field_id) const {
  auto it = iterator_->field_history_.find(field_id);
  return it != iterator_->field_history_.end() ? &it->second.value : nullptr;
}

FormAutofillHistory::FormAutofillHistory() = default;

FormAutofillHistory::~FormAutofillHistory() = default;

void FormAutofillHistory::AddFormFillEntry(
    base::span<std::pair<const FormFieldData*, const AutofillField*>>
        filled_fields,
    url::Origin filling_origin,
    bool is_refill) {
  // Intuitively, `if (!is_refill) history_.emplace_front()` suffices, but it
  // does not handle these corner cases correctly:
  // - If the original fill had `filled_fields.size() >
  //   kMaxStorableFieldFillHistory`, then `history_` might be empty.
  // - If a previous fill had `filled_fields.empty()`, we could save memory.
  if (history_.empty() ||
      (!is_refill && !history_.front().field_history_.empty())) {
    history_.emplace_front();
  }
  history_.front().filling_origin_ = filling_origin;
  for (const auto& [field, autofill_field] : filled_fields) {
    // During refills, a field that was previously filled in the original
    // fill operation, with initial value `A` and filled value `B`, might be
    // refilled with a newer value `C`. We do not store this so that upon
    // undoing Autofill, the field's value reverts from `C` to `A` directly as
    // this is what happened from a user's perspective.
    size_ += history_.front()
                 .field_history_
                 .emplace(field->global_id(),
                          FieldTypeAndValue{
                              .type = autofill_field->Type().GetStorableType(),
                              .value = field->value})
                 .second;
  }
  // Drop the last history entry while the history size exceeds the limit.
  while (size_ > kMaxStorableFieldFillHistory) {
    EraseFormFillEntry(FillOperation(--history_.end()));
  }
}

void FormAutofillHistory::EraseFormFillEntry(FillOperation fill_operation) {
  size_ -= fill_operation.iterator_->field_history_.size();
  history_.erase(fill_operation.iterator_);
}

FormAutofillHistory::FillOperation
FormAutofillHistory::GetLastFillingOperationForField(
    FieldGlobalId field_id) const {
  return FillOperation(base::ranges::find_if(
      history_, [&field_id](const AutofillHistoryEntry& operation) {
        return operation.field_history_.contains(field_id);
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
