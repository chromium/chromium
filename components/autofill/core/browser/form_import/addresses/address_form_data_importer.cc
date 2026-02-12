// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

#include "base/check_deref.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

namespace {

// Struct storing a field's value for import and selected option value, if
// present.
struct ValueForImport {
  // The return value of `AutofillField::value_for_import()`.
  std::u16string value_for_import;
  // The value of the selected option. Only set for <select> fields and where
  // `FormFieldData::selected_option()` doesn't return std::nullopt.
  std::optional<std::u16string> selected_option_value;
};

// Determines if a field's value matches a previously observed field of the same
// type, allowing for duplicate fields with identical values or <select>/<input>
// value mirroring.
bool FieldValueMatchesPrecedingField(const ValueForImport& current_values,
                                     const ValueForImport& preceding_values) {
  bool field_values_match = false;
  // Checks for exact value match between current and previously observed
  // fields.
  if (preceding_values.value_for_import == current_values.value_for_import) {
    field_values_match = true;
  }

  // Check if the selected option value of the current field (if it's a <select>
  // field) matches the value previously observed for a field of the same type.
  // This handles the case where a <select> option's value is stored in a
  // separate <input> field.
  // Example:
  // <select id="country">
  //   <option value="US">United States</option>
  // </select>
  // <input type="text" name="country_code" value="US">
  // Here, `selected_option_value` would be "US", and
  // `observed_field.value_for_import` from the input would also be "US".
  if (current_values.selected_option_value ==
      preceding_values.value_for_import) {
    field_values_match = true;
  }

  // Check if the value of the selected option from a previously observed
  // <select> field matches the value intended for import from the current
  // field.
  // Example:
  // <input type="text" name="country_code" value="US">
  // <select id="country">
  //   <option value="US">United States</option>
  // </select>
  // Here, `observed_field.selected_option_value` would be "US", and
  // `value_for_import` from the <select> would also be "US".
  if (preceding_values.selected_option_value ==
      current_values.value_for_import) {
    field_values_match = true;
  }
  return field_values_match;
}

// Return true if the `field_type` and `current_values` are valid within the
// context of importing a form.
bool IsValidFieldTypeAndValue(
    const base::flat_map<FieldType, ValueForImport>& preceding_values,
    FieldType field_type,
    const ValueForImport& current_values,
    LogBuffer* import_log_buffer) {
  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS &&
      IsValidEmailAddress(current_values.value_for_import)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Email address found in field of different type: "
        << FieldTypeToStringView(field_type) << CTag{};
    return false;
  }

  // Allow the import if `field_type` wasn't observed before. Also, allow it for
  // duplicate fields with identical field values.
  // TODO(crbug.com/395855125): Clean up when launched.
  if (auto it = preceding_values.find(field_type);
      it == preceding_values.end() ||
      FieldValueMatchesPrecedingField(current_values,
                                      preceding_values.at(field_type))) {
    return true;
  }

  // Allow the import for duplicate EMAIL_ADDRESS fields because it is common to
  // see a second 'confirm email address' field.
  if (field_type == EMAIL_ADDRESS) {
    return true;
  }

  // Allow the import for duplicate phone number component fields because a form
  // might request several phone numbers.
  // TODO(crbug.com/40735892) Remove feature check when launched.
  if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
    return true;
  }

  // Abandon the import if two fields of the same type are encountered (after
  // prior exception checks). This indicates ambiguous data or miscategorization
  // of types.
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromFormFailed
      << "Multiple fields of type " << FieldTypeToStringView(field_type) << "."
      << CTag{};
  return false;
}

}  // namespace

AddressFormDataImporter::AddressFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AddressFormDataImporter::~AddressFormDataImporter() = default;

AddressDataManager& AddressFormDataImporter::address_data_manager() {
  return client_->GetPersonalDataManager().address_data_manager();
}

AddressFormDataImporter::ExtractedAddressProfile::ExtractedAddressProfile() =
    default;
AddressFormDataImporter::ExtractedAddressProfile::ExtractedAddressProfile(
    const AddressFormDataImporter::ExtractedAddressProfile& other) = default;
AddressFormDataImporter::ExtractedAddressProfile::~ExtractedAddressProfile() =
    default;

base::flat_map<FieldType, std::u16string>
AddressFormDataImporter::GetAddressObservedFieldValues(
    base::span<const AutofillField* const> section_fields,
    ProfileImportMetadata& import_metadata,
    LogBuffer* import_log_buffer,
    bool& has_invalid_field_types,
    bool& has_multiple_distinct_email_addresses,
    bool& has_address_related_fields) const {
  AutofillPlusAddressDelegate* plus_address_delegate =
      client_->GetPlusAddressDelegate();
  base::flat_map<FieldType, ValueForImport> preceding_values;

  // Tracks if subsequent phone number fields should be ignored,
  // since they do not belong to the first phone number in the form.
  bool ignore_phone_number_fields = false;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const AutofillField* const field : section_fields) {
    std::u16string value = field->value_for_import();
    base::TrimWhitespace(value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    // If the field was filled with a fallback type, skip it in order to not
    // introduce noise to the map's data, as this would add an entry for
    // field type X with a value retrieved from another field type Y.
    if (field->WasAutofilledWithFallback()) {
      continue;
    }
    // When the experimental plus addresses feature is enabled, and the value is
    // a plus address, exclude it from the resulting address profile.
    if (plus_address_delegate &&
        (plus_address_delegate->IsPlusAddress(base::UTF16ToUTF8(value)) ||
         plus_address_delegate->MatchesPlusAddressFormat(value))) {
      continue;
    }

    FieldType field_type = field->Type().GetAddressType();
    // Only address types are relevant in this function, other types are treated
    // in different flows.
    if (field_type == UNKNOWN_TYPE) {
      continue;
    }
    has_address_related_fields = true;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    if (field_type == EMAIL_ADDRESS) {
      auto email_it = preceding_values.find(EMAIL_ADDRESS);
      if (email_it != preceding_values.end() &&
          email_it->second.value_for_import != value) {
        LOG_AF(import_log_buffer)
            << LogMessage::kImportAddressProfileFromFormFailed
            << "Multiple different email addresses present." << CTag{};
        has_multiple_distinct_email_addresses = true;
      }
    }
    std::optional<std::u16string> selected_option_value = std::nullopt;
    if (base::optional_ref<const SelectOption> o = field->selected_option()) {
      selected_option_value = o->value;
    }
    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(
            preceding_values, field_type,
            {.value_for_import = value,
             .selected_option_value = selected_option_value},
            import_log_buffer)) {
      has_invalid_field_types = true;
    }

    const auto field_and_value_it = preceding_values.find(field_type);
    // Found phone number component field.
    // TODO(crbug.com/40735892) Remove feature check when launched.
    if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
      if (ignore_phone_number_fields) {
        continue;
      }
      // Each phone number related type only occurs once per number. Seeing a
      // type a second time implies that it belongs to a new number. Since
      // Autofill currently supports storing only one phone number per profile,
      // ignore this and all subsequent phone number fields.
      if (field_and_value_it != preceding_values.end()) {
        ignore_phone_number_fields = true;
        continue;
      }
    }
    // Ensure that for <select> fields, the selected option's displayed text
    // (which is typically user-friendly) is prioritized over the potentially
    // less readable option value that might be present in a corresponding
    // <input> field.
    if (field_and_value_it == preceding_values.end() ||
        !field_and_value_it->second.selected_option_value.has_value() ||
        selected_option_value.has_value()) {
      preceding_values.insert_or_assign(
          field_type, ValueForImport{.value_for_import = std::move(value),
                                     .selected_option_value =
                                         std::move(selected_option_value)});
    }

    if (field->parsed_autocomplete()) {
      import_metadata.did_import_from_unrecognized_autocomplete_field |=
          field->parsed_autocomplete()->field_type ==
          HtmlFieldType::kUnrecognized;
    }
  }
  return base::MakeFlatMap<FieldType, std::u16string>(
      preceding_values, {}, [](const std::pair<FieldType, ValueForImport>& p) {
        return std::make_pair(p.first, p.second.value_for_import);
      });
}

void AddressFormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile) {
  const FieldTypeSet inaccessible_fields =
      profile.FindInaccessibleProfileValues();
  profile.ClearFields(inaccessible_fields);
  autofill_metrics::LogRemovedSettingInaccessibleFields(
      !inaccessible_fields.empty());
  for (const FieldType inaccessible_field : inaccessible_fields) {
    autofill_metrics::LogRemovedSettingInaccessibleField(inaccessible_field);
  }
}

}  // namespace autofill
