// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_AUTOFILL_HISTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_AUTOFILL_HISTORY_H_

#include <list>
#include <map>
#include <optional>
#include <string>

#include "components/autofill/core/browser/filling/filling_product.h"
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
        FillingProduct filling_product,
        bool ignore_is_autofilled);

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

    // The undo functionality is supposed to work only on autofilled fields.
    // However when this boolean is enabled, this check is relaxed to include
    // non-autofilled fields as well provided that the undo history contains a
    // value for the field.
    bool ignore_is_autofilled;
  };

  using FormFillingEntry = std::map<FieldGlobalId, FieldFillingEntry>;

  FormAutofillHistory();

  FormAutofillHistory(FormAutofillHistory&) = delete;

  FormAutofillHistory& operator=(FormAutofillHistory&) = delete;

  ~FormAutofillHistory();

  // Adds a new history entry in the beginning of the list.
  // FormFieldData's are needed to get the most recent value of a field.
  // AutofillField's are needed to get the type of a field.
  // TODO(crbug.com/40232021): Only pass AutofillFields.
  void AddFormFillingEntry(
      base::span<const FormFieldData* const> filled_fields,
      base::span<const AutofillField* const> filled_autofill_fields,
      FillingProduct filling_product,
      bool is_refill);

  // Erases the field history information corresponding to `field_id` in
  // `fill_operation`. If the form filling entry becomes empty afterwards, the
  // function also removes it from `history_`.
  void EraseFieldFillingEntry(
      std::list<FormFillingEntry>::iterator fill_operation,
      FieldGlobalId field_id);

  // Returns the first entry in `history_` (corresponding to the last
  // chronological entry) that has information about the field represented by
  // `field_id`, if any exist. Note that this means that the returned iterator
  // is either `history_.end()` or satisfies  `it->contains(field_id)`.
  std::list<FormFillingEntry>::iterator GetLastFormFillingEntryForField(
      FieldGlobalId field_id);
  std::list<FormFillingEntry>::const_iterator GetLastFormFillingEntryForField(
      FieldGlobalId field_id) const {
    return const_cast<FormAutofillHistory*>(this)
        ->GetLastFormFillingEntryForField(field_id);
  }

  // Checks whether the field represented by `field_id` has some registered
  // value in any history entry.
  bool HasHistory(FieldGlobalId field_id) const;

  // Clears the list of history entries and resets the history's size.
  void Reset();

  size_t size() const { return history_.size(); }
  bool empty() const { return history_.empty(); }

 private:
  // Erases the history entry from the list represented by `fill_operation`.
  void EraseFormFillEntry(std::list<FormFillingEntry>::iterator filling_entry);

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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_AUTOFILL_HISTORY_H_
