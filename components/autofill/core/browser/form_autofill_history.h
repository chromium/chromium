// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_AUTOFILL_HISTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_AUTOFILL_HISTORY_H_

#include <list>
#include <map>
#include <string>

#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Holds history of Autofill filling operations so that they can be undone
// later. The class is used to add, remove and access filling operations, which
// are maps from fields to their corresponding value and state before filling.
// It is assumed here that between a fill and a refill no user interaction
// happens with the form. Owned by `BrowserAutofillManager`.
class FormAutofillHistory {
 private:
  // This represents the value of a field as well as its autofill state.
  using FieldFillingEntry = std::pair<std::u16string, bool>;

  using FormFillingEntry = std::map<FieldGlobalId, FieldFillingEntry>;

 public:
  class FillOperation {
   public:
    // Returns the field value and autofill state stored in history for
    // `field_id`. Assumes the underlying map contains a entry with key
    // `field_id`.
    FieldFillingEntry GetAutofillValue(FieldGlobalId field_id) const;

    friend bool operator==(const FillOperation& lhs, const FillOperation& rhs) {
      return lhs.iterator_ == rhs.iterator_;
    }
    friend bool operator!=(const FillOperation& lhs, const FillOperation& rhs) {
      return !(lhs == rhs);
    }

   private:
    friend class FormAutofillHistory;

    explicit FillOperation(std::list<FormFillingEntry>::const_iterator iterator)
        : iterator_(iterator) {}

    std::list<FormFillingEntry>::const_iterator iterator_;
  };

  FormAutofillHistory();

  FormAutofillHistory(FormAutofillHistory&) = delete;

  FormAutofillHistory& operator=(FormAutofillHistory&) = delete;

  ~FormAutofillHistory();

  // Adds a new history entry in the beginning of the list.
  // FormFieldData's are needed to get the most recent value of a field.
  // AutofillField's are needed to get the type of a field.
  void AddFormFillEntry(base::span<const FormFieldData* const> filled_fields,
                        bool is_refill);

  // Erases the history entry from the list represented by `fill_operation`.
  void EraseFormFillEntry(FillOperation fill_operation);

  // Finds the latest history entry where the field represented by `field_id`
  // was affected.
  FillOperation GetLastFillingOperationForField(FieldGlobalId field_id) const;

  // Checks whether the field represented by `field_id` has some registered
  // value in any history entry.
  bool HasHistory(FieldGlobalId field_id) const;

  // Clears the list of history entries and resets the history's size.
  void Reset();

  size_t size() const { return history_.size(); }
  bool empty() const { return history_.empty(); }

 private:
  // Holds, for each filling operation in reverse chronological order, a map
  // from the IDs of the fields that were affected by the corresponding filling
  // operation to the value and autofill state of the field prior to the
  // filling.
  std::list<FormFillingEntry> history_;

  // Holds the number of field entries stored in `history`
  // which is the sum of sizes of each individual map.
  size_t size_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_AUTOFILL_HISTORY_H_
