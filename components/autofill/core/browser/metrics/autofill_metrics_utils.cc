// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

namespace internal {

constexpr DenseSet<FormType> kAddressFormTypes = {FormType::kAddressForm};
constexpr DenseSet<FormType> kCreditCardFormTypes = {
    FormType::kCreditCardForm, FormType::kStandaloneCvcForm};
constexpr DenseSet<FieldType> kFieldTypesOfATypicalStoreLocatorForm = {
    ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP};

bool IsCvcOnlyForm(const FormStructure& form) {
  if (form.fields().size() != 1) {
    return false;
  }
  // Theoretically we don't need to check
  // CREDIT_CARD_STANDALONE_VERIFICATION_CODE type here since if it's
  // CREDIT_CARD_STANDALONE_VERIFICATION_CODE, the form type would be treated as
  // kStandaloneCvcForm already. Add CREDIT_CARD_STANDALONE_VERIFICATION_CODE
  // here just for completion.
  static constexpr FieldTypeSet kCvcTypes = {
      CREDIT_CARD_VERIFICATION_CODE, CREDIT_CARD_STANDALONE_VERIFICATION_CODE};
  return kCvcTypes.contains(form.fields()[0]->Type().GetStorableType());
}

bool IsEmailOnlyForm(const FormStructure& form) {
  bool has_email_field = false;
  for (const auto& field : form.fields()) {
    FieldType field_type = field->Type().GetStorableType();
    if (field_type == EMAIL_ADDRESS) {
      has_email_field = true;
    }
    if (field_type != EMAIL_ADDRESS && field_type != UNKNOWN_TYPE &&
        FieldTypeGroupToFormType(field->Type().group()) !=
            FormType::kPasswordForm) {
      return false;
    }
  }
  return has_email_field;
}

bool IsPostalAddressForm(const FormStructure& form) {
  DenseSet<FieldType> postal_address_field_types;
  for (const auto& field : form.fields()) {
    if (field->Type().group() == FieldTypeGroup::kAddress &&
        field->Type().GetStorableType() != ADDRESS_HOME_COUNTRY) {
      postal_address_field_types.insert(field->Type().GetStorableType());
    }
  }
  return postal_address_field_types.size() >= 3 &&
         postal_address_field_types != kFieldTypesOfATypicalStoreLocatorForm;
}

// Returns `form`'s types for logging. If `filter_by` is not empty, only those
// `FormTypeNameForLogging` entries are returned that correspond to the form
// types in `filter_by`.
DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
    const FormStructure& form,
    std::optional<DenseSet<FormType>> filter_by = std::nullopt) {
  DenseSet<FormTypeNameForLogging> form_types;
  for (FormType form_type : form.GetFormTypes()) {
    if (filter_by && !(*filter_by).contains(form_type)) {
      continue;
    }
    switch (form_type) {
      case FormType::kAddressForm:
        form_types.insert(FormTypeNameForLogging::kAddressForm);
        if (IsEmailOnlyForm(form)) {
          form_types.insert(FormTypeNameForLogging::kEmailOnlyForm);
        } else if (IsPostalAddressForm(form)) {
          form_types.insert(FormTypeNameForLogging::kPostalAddressForm);
        }
        break;
      case FormType::kCreditCardForm:
        form_types.insert(IsCvcOnlyForm(form)
                              ? FormTypeNameForLogging::kStandaloneCvcForm
                              : FormTypeNameForLogging::kCreditCardForm);
        break;
      case FormType::kStandaloneCvcForm:
        form_types.insert(FormTypeNameForLogging::kStandaloneCvcForm);
        break;
      case FormType::kPasswordForm:
      case FormType::kUnknownFormType:
        break;
    }
  }
  return form_types;
}

}  // namespace internal

AutofillProfileRecordTypeCategory GetCategoryOfProfile(
    const AutofillProfile& profile) {
  switch (profile.record_type()) {
    case AutofillProfile::RecordType::kLocalOrSyncable:
      return AutofillProfileRecordTypeCategory::kLocalOrSyncable;
    case AutofillProfile::RecordType::kAccount:
    case AutofillProfile::RecordType::kAccountHome:
    case AutofillProfile::RecordType::kAccountWork:
      return profile.initial_creator_id() ==
                     AutofillProfile::kInitialCreatorOrModifierChrome
                 ? AutofillProfileRecordTypeCategory::kAccountChrome
                 : AutofillProfileRecordTypeCategory::kAccountNonChrome;
  }
}

const char* GetProfileCategorySuffix(
    AutofillProfileRecordTypeCategory category) {
  switch (category) {
    case AutofillProfileRecordTypeCategory::kLocalOrSyncable:
      return "Legacy";
    case AutofillProfileRecordTypeCategory::kAccountChrome:
      return "AccountChrome";
    case AutofillProfileRecordTypeCategory::kAccountNonChrome:
      return "AccountNonChrome";
  }
}

SettingsVisibleFieldTypeForMetrics ConvertSettingsVisibleFieldTypeForMetrics(
    FieldType field_type) {
  switch (field_type) {
    case NAME_FULL:
      return SettingsVisibleFieldTypeForMetrics::kName;

    case EMAIL_ADDRESS:
      return SettingsVisibleFieldTypeForMetrics::kEmailAddress;

    case PHONE_HOME_WHOLE_NUMBER:
      return SettingsVisibleFieldTypeForMetrics::kPhoneNumber;

    case ADDRESS_HOME_CITY:
      return SettingsVisibleFieldTypeForMetrics::kCity;

    case ADDRESS_HOME_COUNTRY:
      return SettingsVisibleFieldTypeForMetrics::kCountry;

    case ADDRESS_HOME_ZIP:
      return SettingsVisibleFieldTypeForMetrics::kZip;

    case ADDRESS_HOME_STATE:
      return SettingsVisibleFieldTypeForMetrics::kState;

    case ADDRESS_HOME_STREET_ADDRESS:
      return SettingsVisibleFieldTypeForMetrics::kStreetAddress;

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return SettingsVisibleFieldTypeForMetrics::kDependentLocality;

    case COMPANY_NAME:
      return SettingsVisibleFieldTypeForMetrics::kCompany;

    case ADDRESS_HOME_ADMIN_LEVEL2:
      return SettingsVisibleFieldTypeForMetrics::kAdminLevel2;

    case ALTERNATIVE_FULL_NAME:
      return SettingsVisibleFieldTypeForMetrics::kAlternativeName;

    default:
      return SettingsVisibleFieldTypeForMetrics::kUndefined;
  }
}

DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
    const FormStructure& form) {
  return internal::GetFormTypesForLogging(form);
}

DenseSet<FormTypeNameForLogging> GetAddressFormTypesForLogging(
    const FormStructure& form) {
  return internal::GetFormTypesForLogging(form, internal::kAddressFormTypes);
}

DenseSet<FormTypeNameForLogging> GetCreditCardFormTypesForLogging(
    const FormStructure& form) {
  return internal::GetFormTypesForLogging(form, internal::kCreditCardFormTypes);
}

bool ShouldLogAutofillSuggestionShown(
    AutofillSuggestionTriggerSource trigger_source) {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
      return true;
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kAutofillAi:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
      return false;
  }
}

int GetBucketForAcceptanceMetricsGroupedByFieldType(FieldType field_type,
                                                    bool suggestion_accepted) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
                "Autofill::FieldType value needs more than 12 bits.");

  return (field_type << 2) | suggestion_accepted;
}

int GetDuplicationRank(
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets) {
  // All elements of `min_incompatible_sets` have the same size.
  return min_incompatible_sets.empty()
             ? std::numeric_limits<int>::max()
             : min_incompatible_sets.back().field_type_set.size();
}

uint64_t FormGlobalIdToHash64Bit(const FormGlobalId& form_global_id) {
  return StrToHash64Bit(
      base::NumberToString(form_global_id.renderer_id.value()) +
      form_global_id.frame_token.ToString());
}

uint64_t FieldGlobalIdToHash64Bit(const FieldGlobalId& field_global_id) {
  return StrToHash64Bit(
      base::NumberToString(field_global_id.renderer_id.value()) +
      field_global_id.frame_token.ToString());
}

}  // namespace autofill::autofill_metrics
