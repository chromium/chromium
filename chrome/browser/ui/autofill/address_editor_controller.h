// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_EDITOR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"

namespace autofill {
class CountryComboboxModel;
class PersonalDataManager;

// Field definition for an editor field, used to build the UI.
struct EditorField {
  enum class LengthHint : int { HINT_LONG, HINT_SHORT };
  enum class ControlType : int { TEXTFIELD, TEXTFIELD_NUMBER, COMBOBOX };

  EditorField(FieldType type,
              std::u16string label,
              LengthHint length_hint,
              bool is_required,
              ControlType control_type = ControlType::TEXTFIELD)
      : type(type),
        label(std::move(label)),
        length_hint(length_hint),
        is_required(is_required),
        control_type(control_type) {}

  // Data type in the field.
  FieldType type;
  // Label to be shown alongside the field.
  std::u16string label;
  // Hint about the length of this field's contents.
  LengthHint length_hint;
  // Whether this field should be validated against the "is required" rule.
  bool is_required;
  // The control type.
  ControlType control_type;
};

class AddressEditorController {
 public:
  using OnIsValidChangeCallbackList = base::RepeatingCallbackList<void(bool)>;

  AddressEditorController(const AutofillProfile& profile_to_edit,
                          PersonalDataManager* pdm,
                          bool is_validatable);
  ~AddressEditorController();

  const std::vector<EditorField>& editor_fields() { return editor_fields_; }

  bool is_validatable() const { return is_validatable_; }

  // Returns `std::nullopt` if the form was not validated (`ValidateAllFields()`
  // was not triggered on the view and it didn't call `SetIsValid()` on its
  // turn) yet, its validity state is unknown. Otherwise returns the state
  // according to the validation performed in the view.
  // Note that the view doesn't perform validation if the `is_validatable()` is
  // `false`, in which case this always returns `std::nullopt`.
  std::optional<bool> is_valid() const { return is_valid_; }

  void SetIsValid(bool is_valid);

  CountryComboboxModel& GetCountryComboboxModel();

  // Updates `editor_fields_` based on the current selected country.
  void UpdateEditorFields(const std::string& country_code);

  void SetProfileInfo(FieldType type, const std::u16string& value);

  std::u16string GetProfileInfo(FieldType type);

  const AutofillProfile& GetAddressProfile();

  [[nodiscard]] base::CallbackListSubscription AddIsValidChangedCallback(
      OnIsValidChangeCallbackList::CallbackType callback);

  bool IsValid(const EditorField& field, const std::u16string& value);

 private:
  // Returns whether unsupported countries should be filtered out, which is
  // true iff the profile is an account address profile.
  bool should_filter_out_unsupported_countries() const {
    // Validation is turned on only for account address profiles.
    return is_validatable_;
  }

  AutofillProfile profile_to_edit_;

  const raw_ref<PersonalDataManager> pdm_;

  CountryComboboxModel countries_;

  // The language code to be format this address, reset every time the current
  // country changes.
  std::string language_code_;

  // The browser locale, used to compute the values of profile fields. (e.g. the
  // locale in which to return the country name).
  const std::string locale_;

  std::vector<EditorField> editor_fields_;

  // Whether the editor should perform validation.
  const bool is_validatable_ = false;

  std::optional<bool> is_valid_;

  // List of external callbacks subscribed to validity updates.
  OnIsValidChangeCallbackList on_is_valid_change_callbacks_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_EDITOR_CONTROLLER_H_
