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

namespace autofill {
class CountryComboboxModel;
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}

namespace ui {
class ComboboxModel;
}

const size_t kInvalidCountryIndex = static_cast<size_t>(-1);

// Field definition for an editor field, used to build the UI.
struct EditorField {
  enum class LengthHint : int { HINT_LONG, HINT_SHORT };
  enum class ControlType : int { TEXTFIELD, TEXTFIELD_NUMBER, COMBOBOX };

  EditorField(autofill::ServerFieldType type,
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
  autofill::ServerFieldType type;
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

  AddressEditorController(const autofill::AutofillProfile& profile_to_edit,
                          content::WebContents* web_contents,
                          bool is_validatable);
  ~AddressEditorController();

  const std::vector<EditorField>& editor_fields() { return editor_fields_; }
  size_t chosen_country_index() { return chosen_country_index_; }
  void set_chosen_country_index(size_t chosen_country_index) {
    chosen_country_index_ = chosen_country_index;
  }

  bool get_is_validatable() const { return is_validatable_; }

  bool get_is_valid() const { return is_valid_; }

  void SetIsValid(bool is_valid);

  size_t GetCountriesSize();

  std::unique_ptr<ui::ComboboxModel> GetCountryComboboxModel();

  // Updates |editor_fields_| based on the current country.
  void UpdateEditorFields();

  void SetProfileInfo(autofill::ServerFieldType type,
                      const std::u16string& value);

  std::u16string GetProfileInfo(autofill::ServerFieldType type);

  const autofill::AutofillProfile& GetAddressProfile();

  [[nodiscard]] base::CallbackListSubscription AddIsValidChangedCallback(
      OnIsValidChangeCallbackList::CallbackType callback);

  bool IsValid(const EditorField& field, const std::u16string& value);

 private:
  // Validation is turned on only for account address profiles.
  bool is_filter_out_unsupported_countries() const { return is_validatable_; }

  // Updates |countries_| with the content of |model| if it's not null,
  // otherwise use a local model.
  void UpdateCountries(autofill::CountryComboboxModel* model);

  autofill::AutofillProfile profile_to_edit_;

  raw_ptr<autofill::PersonalDataManager> pdm_;

  // The currently chosen country. Defaults to an invalid constant until
  // |countries_| is properly initialized and then 0 as the first entry in
  // |countries_|, which is the generated default value received from
  // autofill::CountryComboboxModel::countries() which is documented to always
  // have the default country at the top as well as within the sorted list. If
  // |profile_to_edit_| is not null, then use the country from there to set
  // |chosen_country_index_|.
  size_t chosen_country_index_ = kInvalidCountryIndex;

  // The list of country codes and names as ordered in the country combobox
  // model.
  std::vector<std::pair<std::string, std::u16string>> countries_;

  // The language code to be format this address, reset every time the current
  // country changes.
  std::string language_code_;

  // The browser locale, used to compute the values of profile fields. (e.g. the
  // locale in which to return the country name).
  const std::string locale_;

  std::vector<EditorField> editor_fields_;

  // Whether the editor should perform validation.
  bool is_validatable_ = false;

  bool is_valid_ = true;

  // List of external callbacks subscribed to validity updates.
  OnIsValidChangeCallbackList on_is_valid_change_callbacks_;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_EDITOR_CONTROLLER_H_
