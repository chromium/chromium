// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_address_editor_view_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {
namespace {

// size_t doesn't have a defined maximum value, so this is a trick to create one
// as is done for std::string::npos.
// http://www.cplusplus.com/reference/string/string/npos
const size_t kInvalidCountryIndex = static_cast<size_t>(-1);

}  // namespace

ShippingAddressEditorViewController::ShippingAddressEditorViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile,
    bool is_incognito)
    : EditorViewController(spec,
                           state,
                           dialog,
                           back_navigation_type,
                           is_incognito),
      on_edited_(std::move(on_edited)),
      on_added_(std::move(on_added)),
      profile_to_edit_(profile),
      chosen_country_index_(kInvalidCountryIndex),
      failed_to_load_region_data_(false) {
  if (profile_to_edit_)
    temporary_profile_ = *profile_to_edit_;
  UpdateCountries(/*model=*/nullptr);
  UpdateEditorFields();
}

ShippingAddressEditorViewController::~ShippingAddressEditorViewController() {}

bool ShippingAddressEditorViewController::IsEditingExistingItem() {
  return !!profile_to_edit_;
}

std::vector<EditorField>
ShippingAddressEditorViewController::GetFieldDefinitions() {
  return editor_fields_;
}

std::u16string ShippingAddressEditorViewController::GetInitialValueForType(
    autofill::FieldType type) {
  return GetValueForType(temporary_profile_, type);
}

bool ShippingAddressEditorViewController::ValidateModelAndSave() {
  // To validate the profile first, we use a temporary object. Note that the
  // address country gets set first during `SaveFieldsToProfile()`, therefore it
  // is okay to initially build this profile with an empty country.
  autofill::AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  if (!SaveFieldsToProfile(&profile, /*ignore_errors=*/false))
    return false;
  if (!profile_to_edit_) {
    // Add the profile (will not add a duplicate).
    if (!is_incognito())
      state()->GetPersonalDataManager()->address_data_manager().AddProfile(
          profile);
    std::move(on_added_).Run(profile);
    on_edited_.Reset();
  } else {
    // Fields are only updated in the autofill profile to avoid clearing the
    // parsed substructure.
    bool success = SaveFieldsToProfile(profile_to_edit_,
                                       /*ignore_errors=*/false);
    DCHECK(success);
    if (!is_incognito())
      state()->GetPersonalDataManager()->address_data_manager().UpdateProfile(
          *profile_to_edit_);
    state()->profile_comparator()->Invalidate(*profile_to_edit_);
    std::move(on_edited_).Run();
    on_added_.Reset();
  }

  return true;
}

std::unique_ptr<ValidationDelegate>
ShippingAddressEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return std::make_unique<
      ShippingAddressEditorViewController::ShippingAddressValidationDelegate>(
      weak_ptr_factory_.GetWeakPtr(), field);
}

std::unique_ptr<ui::ComboboxModel>
ShippingAddressEditorViewController::GetComboboxModelForType(
    const autofill::FieldType& type) {
  switch (type) {
    case autofill::ADDRESS_HOME_COUNTRY: {
      auto model = std::make_unique<autofill::CountryComboboxModel>();
      model->SetCountries(*state()->GetPersonalDataManager(),
                          base::RepeatingCallback<bool(const std::string&)>(),
                          state()->GetApplicationLocale());
      if (model->countries().size() != countries_.size())
        UpdateCountries(model.get());
      return model;
    }
    case autofill::ADDRESS_HOME_STATE: {
      auto model = std::make_unique<autofill::RegionComboboxModel>();
      region_model_ = model.get();
      if (chosen_country_index_ < countries_.size()) {
        model->LoadRegionData(countries_[chosen_country_index_].first,
                              state()->GetRegionDataLoader());
        if (!model->IsPendingRegionDataLoad()) {
          // If the data was already pre-loaded, the observer won't get notified
          // so we have to check for failure here.
          failed_to_load_region_data_ = model->failed_to_load_data();
        }
      } else {
        failed_to_load_region_data_ = true;
      }
      if (failed_to_load_region_data_) {
        // We can't update the view synchronously while building the view.
        OnDataChanged(/*synchronous=*/false);
      }
      return model;
    }
    default:
      NOTREACHED();
  }
}

void ShippingAddressEditorViewController::OnPerformAction(
    ValidatingCombobox* sender) {
  EditorViewController::OnPerformAction(sender);
  if (sender->GetID() != GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY))
    return;
  DCHECK(sender->GetSelectedIndex().has_value());
  if (chosen_country_index_ != sender->GetSelectedIndex()) {
    chosen_country_index_ = sender->GetSelectedIndex().value();
    failed_to_load_region_data_ = false;
    // View update must be asynchronous to let the combobox finish performing
    // the action.
    OnDataChanged(/*synchronous=*/false);
  }
}

void ShippingAddressEditorViewController::UpdateEditorView() {
  region_model_ = nullptr;
  EditorViewController::UpdateEditorView();
  if (chosen_country_index_ > 0UL &&
      chosen_country_index_ < countries_.size()) {
    views::Combobox* country_combo_box =
        static_cast<views::Combobox*>(dialog()->GetViewByID(
            GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combo_box);
    DCHECK_EQ(countries_.size(), country_combo_box->GetRowCount());
    country_combo_box->SetSelectedIndex(chosen_country_index_);
  } else if (countries_.size() > 0UL) {
    chosen_country_index_ = 0UL;
  } else {
    chosen_country_index_ = kInvalidCountryIndex;
  }
}

std::u16string ShippingAddressEditorViewController::GetSheetTitle() {
  // TODO(crbug.com/41313365): Editor title should reflect the missing
  // information in the case that one or more fields are missing.
  return profile_to_edit_ ? l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT_ADDRESS)
                          : l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_ADDRESS);
}

base::WeakPtr<PaymentRequestSheetController>
ShippingAddressEditorViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int ShippingAddressEditorViewController::GetPrimaryButtonId() {
  return static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON);
}

ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ShippingAddressValidationDelegate(
        base::WeakPtr<ShippingAddressEditorViewController> controller,
        const EditorField& field)
    : field_(field), controller_(controller) {}

ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ~ShippingAddressValidationDelegate() {}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ShouldFormat() {
  return field_.type == autofill::PHONE_HOME_WHOLE_NUMBER;
}

std::u16string
ShippingAddressEditorViewController::ShippingAddressValidationDelegate::Format(
    const std::u16string& text) {
  if (controller_ &&
      controller_->chosen_country_index_ < controller_->countries_.size()) {
    return base::UTF8ToUTF16(autofill::i18n::FormatPhoneForDisplay(
        base::UTF16ToUTF8(text),
        controller_->countries_[controller_->chosen_country_index_].first));
  } else {
    return text;
  }
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    IsValidTextfield(views::Textfield* textfield,
                     std::u16string* error_message) {
  return ValidateValue(textfield->GetText(), error_message);
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    IsValidCombobox(ValidatingCombobox* combobox,
                    std::u16string* error_message) {
  return ValidateValue(
      combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
      error_message);
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    TextfieldValueChanged(views::Textfield* textfield, bool was_blurred) {
  if (!was_blurred)
    return true;

  std::u16string error_message;
  bool is_valid = ValidateValue(textfield->GetText(), &error_message);
  if (controller_) {
    controller_->DisplayErrorMessageForField(field_.type, error_message);
  }
  return is_valid;
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ComboboxValueChanged(ValidatingCombobox* combobox) {
  std::u16string error_message;
  bool is_valid = ValidateValue(
      combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
      &error_message);
  if (controller_) {
    controller_->DisplayErrorMessageForField(field_.type, error_message);
  }
  return is_valid;
}

void ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ComboboxModelChanged(ValidatingCombobox* combobox) {
  if (controller_) {
    controller_->OnComboboxModelChanged(combobox);
  }
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ValidateValue(const std::u16string& value, std::u16string* error_message) {
  if (!controller_ || !controller_->spec())
    return false;

  // Show errors from merchant's retry() call. Note that changing the selected
  // shipping address will clear the validation errors from retry().
  autofill::AutofillProfile* invalid_shipping_profile =
      controller_->state()->invalid_shipping_profile();
  if (invalid_shipping_profile && error_message &&
      value == controller_->GetValueForType(*invalid_shipping_profile,
                                            field_.type)) {
    *error_message = controller_->spec()->GetShippingAddressError(field_.type);
    if (!error_message->empty())
      return false;
  }

  if (!value.empty()) {
    if (field_.type == autofill::PHONE_HOME_WHOLE_NUMBER &&
        controller_->chosen_country_index_ < controller_->countries_.size() &&
        !autofill::IsPossiblePhoneNumber(
            value, controller_->countries_[controller_->chosen_country_index_]
                       .first)) {
      if (error_message) {
        *error_message = l10n_util::GetStringUTF16(
            IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
      }
      return false;
    }

    if (field_.type == autofill::ADDRESS_HOME_STATE &&
        value == l10n_util::GetStringUTF16(IDS_AUTOFILL_LOADING_REGIONS)) {
      // Wait for the regions to be loaded or timeout before assessing validity.
      return false;
    }

    // As long as other field types are non-empty, they are valid.
    return true;
  }
  if (error_message && field_.required) {
    *error_message = l10n_util::GetStringUTF16(
        IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return !field_.required;
}

std::u16string ShippingAddressEditorViewController::GetValueForType(
    const autofill::AutofillProfile& profile,
    autofill::FieldType type) {
  if (type == autofill::PHONE_HOME_WHOLE_NUMBER) {
    return autofill::i18n::GetFormattedPhoneNumberForDisplay(
        profile, state()->GetApplicationLocale());
  }

  if (type == autofill::ADDRESS_HOME_STATE && region_model_) {
    // For the state, check if the initial value matches either a region code or
    // a region name.
    std::u16string initial_region =
        profile.GetInfo(type, state()->GetApplicationLocale());
    autofill::l10n::CaseInsensitiveCompare compare;

    for (const auto& region : region_model_->GetRegions()) {
      if (compare.StringsEqual(initial_region,
                               base::UTF8ToUTF16(region.first)) ||
          compare.StringsEqual(initial_region,
                               base::UTF8ToUTF16(region.second))) {
        return base::UTF8ToUTF16(region.second);
      }
    }

    return initial_region;
  }

  if (type == autofill::ADDRESS_HOME_STREET_ADDRESS) {
    std::string street_address_line;
    i18n::addressinput::GetStreetAddressLinesAsSingleLine(
        *autofill::i18n::CreateAddressDataFromAutofillProfile(
            profile, state()->GetApplicationLocale()),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile.GetInfo(type, state()->GetApplicationLocale());
}

bool ShippingAddressEditorViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::SHIPPING_ADDRESS_EDITOR_SHEET;
  return true;
}

void ShippingAddressEditorViewController::UpdateCountries(
    autofill::CountryComboboxModel* model) {
  autofill::CountryComboboxModel local_model;
  if (!model) {
    local_model.SetCountries(
        *state()->GetPersonalDataManager(),
        base::RepeatingCallback<bool(const std::string&)>(),
        state()->GetApplicationLocale());
    model = &local_model;
  }

  for (size_t i = 0; i < model->countries().size(); ++i) {
    autofill::AutofillCountry* country(model->countries()[i].get());
    if (country) {
      countries_.push_back(
          std::make_pair(country->country_code(), country->name()));
    } else {
      // Separator, kept to make sure the size of the vector stays the same.
      countries_.push_back(std::make_pair("", u""));
    }
  }
  // If there is a profile to edit, make sure to use its country for the initial
  // |chosen_country_index_|.
  if (IsEditingExistingItem()) {
    std::u16string chosen_country(temporary_profile_.GetInfo(
        autofill::ADDRESS_HOME_COUNTRY, state()->GetApplicationLocale()));
    for (chosen_country_index_ = 0; chosen_country_index_ < countries_.size();
         ++chosen_country_index_) {
      if (chosen_country == countries_[chosen_country_index_].second)
        break;
    }
    // Make sure the the country was actually found in |countries_| and was not
    // empty, otherwise set |chosen_country_index_| to index 0, which is the
    // default country based on the locale.
    if (chosen_country_index_ >= countries_.size() || chosen_country.empty()) {
      // But only if there is at least one country.
      if (!countries_.empty()) {
        LOG(ERROR) << "Unexpected country: " << chosen_country;
        chosen_country_index_ = 0;
        temporary_profile_.SetInfo(autofill::ADDRESS_HOME_COUNTRY,
                                   countries_[chosen_country_index_].second,
                                   state()->GetApplicationLocale());
      } else {
        LOG(ERROR) << "Unexpected empty country list!";
        chosen_country_index_ = kInvalidCountryIndex;
      }
    }
  } else if (!countries_.empty()) {
    chosen_country_index_ = 0;
  }
}

void ShippingAddressEditorViewController::UpdateEditorFields() {
  editor_fields_.clear();
  std::string chosen_country_code;
  if (chosen_country_index_ < countries_.size())
    chosen_country_code = countries_[chosen_country_index_].first;

  std::vector<std::vector<autofill::AutofillAddressUIComponent>> components;
  autofill::GetAddressComponents(
      chosen_country_code, state()->GetApplicationLocale(),
      /*include_literals=*/false, &components, &language_code_);

  // Insert the Country combobox at the top.
  editor_fields_.emplace_back(
      autofill::ADDRESS_HOME_COUNTRY,
      l10n_util::GetStringUTF16(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL),
      EditorField::LengthHint::HINT_SHORT, /*required=*/true,
      EditorField::ControlType::COMBOBOX);

  for (const std::vector<autofill::AutofillAddressUIComponent>& line :
       components) {
    for (const autofill::AutofillAddressUIComponent& component : line) {
      EditorField::LengthHint length_hint =
          component.length_hint ==
                  autofill::AutofillAddressUIComponent::HINT_LONG
              ? EditorField::LengthHint::HINT_LONG
              : EditorField::LengthHint::HINT_SHORT;

      EditorField::ControlType control_type =
          EditorField::ControlType::TEXTFIELD;
      if (component.field == autofill::ADDRESS_HOME_COUNTRY ||
          (component.field == autofill::ADDRESS_HOME_STATE &&
           !failed_to_load_region_data_)) {
        control_type = EditorField::ControlType::COMBOBOX;
      }
      editor_fields_.emplace_back(
          component.field, base::UTF8ToUTF16(component.name), length_hint,
          autofill::i18n::IsFieldRequired(component.field,
                                          chosen_country_code) ||
              component.field == autofill::NAME_FULL,
          control_type);
    }
  }
  // Always add phone number at the end.
  editor_fields_.emplace_back(
      autofill::PHONE_HOME_WHOLE_NUMBER,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_PHONE),
      EditorField::LengthHint::HINT_SHORT, /*required=*/true,
      EditorField::ControlType::TEXTFIELD_NUMBER);
}

void ShippingAddressEditorViewController::OnDataChanged(bool synchronous) {
  SaveFieldsToProfile(&temporary_profile_, /*ignore_errors*/ true);

  // Normalization is guaranteed to be synchronous and rules should have been
  // loaded already.
  state()->GetAddressNormalizer()->NormalizeAddressSync(&temporary_profile_);

  UpdateEditorFields();
  if (synchronous) {
    UpdateEditorView();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShippingAddressEditorViewController::UpdateEditorView,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ShippingAddressEditorViewController::SaveFieldsToProfile(
    autofill::AutofillProfile* profile,
    bool ignore_errors) {
  const std::string& locale = state()->GetApplicationLocale();
  // The country must be set first, because the profile uses the country to
  // interpret some of the data (e.g., phone numbers) passed to SetInfo.
  views::Combobox* combobox =
      static_cast<views::Combobox*>(dialog()->GetViewByID(
          GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY)));
  // The combobox can be null when saving to temporary profile while updating
  // the view.
  if (combobox) {
    std::u16string country(
        combobox->GetTextForRow(combobox->GetSelectedIndex().value()));
    bool success =
        profile->SetInfo(autofill::ADDRESS_HOME_COUNTRY, country, locale);
    LOG_IF(ERROR, !success && !ignore_errors)
        << "Can't set profile country to: " << country;
    if (!success && !ignore_errors)
      return false;
  }

  bool success = true;
  for (const auto& field : text_fields()) {
    // ValidatingTextfield* is the key, EditorField is the value.
    if (field.first->IsValid()) {
      success =
          profile->SetInfo(field.second.type, field.first->GetText(), locale);
    } else {
      success = false;
    }
    LOG_IF(ERROR, !success && !ignore_errors)
        << "Can't setinfo(" << field.second.type << ", "
        << field.first->GetText();
    if (!success && !ignore_errors)
      return false;
  }
  for (const auto& field : comboboxes()) {
    // ValidatingCombobox* is the key, EditorField is the value.
    ValidatingCombobox* validating_combobox = field.first;
    // The country has already been dealt with.
    if (validating_combobox->GetID() ==
        GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY))
      continue;
    if (validating_combobox->IsValid()) {
      success =
          profile->SetInfo(field.second.type,
                           validating_combobox->GetTextForRow(
                               validating_combobox->GetSelectedIndex().value()),
                           locale);
    } else {
      success = false;
    }
    LOG_IF(ERROR, !success && !ignore_errors)
        << "Can't setinfo(" << field.second.type << ", "
        << validating_combobox->GetTextForRow(
               validating_combobox->GetSelectedIndex().value());
    if (!success && !ignore_errors)
      return false;
  }
  profile->set_language_code(language_code_);
  return success;
}

void ShippingAddressEditorViewController::OnComboboxModelChanged(
    ValidatingCombobox* combobox) {
  if (combobox->GetID() != GetInputFieldViewId(autofill::ADDRESS_HOME_STATE))
    return;
  autofill::RegionComboboxModel* model =
      static_cast<autofill::RegionComboboxModel*>(combobox->GetModel());
  if (model->IsPendingRegionDataLoad())
    return;
  if (model->failed_to_load_data()) {
    failed_to_load_region_data_ = true;
    // It is safe to update synchronously since the change comes from the model
    // and not from the UI.
    OnDataChanged(/*synchronous=*/true);
  } else {
    std::u16string state_value =
        GetInitialValueForType(autofill::ADDRESS_HOME_STATE);
    if (!state_value.empty()) {
      combobox->SelectValue(state_value);
      OnPerformAction(combobox);
    }
  }
}

}  // namespace payments
