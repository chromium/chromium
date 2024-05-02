// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_AUTOFILL_HISTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_AUTOFILL_HISTORY_H_

#include <list>
#include <map>
#include <optional>
#include <string>

#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;

// Holds history of Autofill filling operations so that they can be undone
// later. The class is used to add, remove and access filling operations, which
// are maps from fields to their corresponding value and state before filling.
// It is assumed here that between a fill and a refill no user interaction
// happens with the form. Owned by `BrowserAutofillManager`.
class FormAutofillHistory {
 public:
  // This holds the field attributes that should be reset during an undo.
  struct FieldFillingEntry {
    FieldFillingEntry(
        std::u16string field_value,
        bool field_is_autofilled,
        std::optional<std::string> field_autofill_source_profile_guid,
        std::optional<FieldType> field_autofilled_type,
        FillingProduct filling_product);

    ~FieldFillingEntry();
    FieldFillingEntry(const FieldFillingEntry&);
    FieldFillingEntry(FieldFillingEntry&&);
    FieldFillingEntry& operator=(const FieldFillingEntry&) = default;
    FieldFillingEntry& operator=(FieldFillingEntry&&) = default;

    bool operator==(const FieldFillingEntry& rhs) const = default;

    // Value of the field prior to the Undo operation.
    std::u16string value;

    // Autofill state of the field prior to the Undo operation. This is stored
    // because fields that are autofilled might be reset to still autofilled
    // field, considering cases where autofill is allowed to override autofilled
    // fields.
    bool is_autofilled;

    // ID of the last profile used to fill the field, if any. This is stored so
    // the field doesn't track undone autofill operations, which can cause
    // problems. (see crbug.com/1491872)
    std::optional<std::string> autofill_source_profile_guid;

    // Field type used to fill the field. This is stored so that after undoing
    // an autofill operation, AutofillField does not store outdated information,
    // especially if the field is reverted with Undo to a previous autofilled
    // state.
    std::optional<FieldType> autofilled_type;

    // Last product used to fill the field. This is stored so that Autofill
    // stores accurate information about the last modifier of the field,
    // especially since a single field can now be filled via different products.
    FillingProduct filling_product;
  };

  struct FormFillingEntry {
    FormFillingEntry();
    ~FormFillingEntry();

    FillingProduct filling_product = FillingProduct::kNone;
    std::map<FieldGlobalId, FieldFillingEntry> field_filling_entries = {};
  };

  class FillOperation {
   public:
    // Returns the field value and autofill state stored in history for
    // `field_id`. Assumes the underlying map contains a entry with key
    // `field_id`.
    const FieldFillingEntry& GetFieldFillingEntry(FieldGlobalId field_id) const;

    FillingProduct get_filling_product() const {
      return iterator_->filling_product;
    }

    friend bool operator==(const FillOperation& lhs,
                           const FillOperation& rhs) = default;

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
  // TODO(crbug.com/40232021): Only pass AutofillFields.
  void AddFormFillEntry(
      base::span<const FormFieldData* const> filled_fields,
      base::span<const AutofillField* const> filled_autofill_fields,
      FillingProduct filling_product,
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
