// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

using FieldFillingStatus = AutofillMetrics::FieldFillingStatus;

void FormGroupFillingStats::AddFieldFillingStatus(FieldFillingStatus status) {
  switch (status) {
    case FieldFillingStatus::kAccepted:
      num_accepted++;
      return;
    case FieldFillingStatus::kCorrectedToSameType:
      num_corrected_to_same_type++;
      return;
    case FieldFillingStatus::kCorrectedToDifferentType:
      num_corrected_to_different_type++;
      return;
    case FieldFillingStatus::kCorrectedToUnknownType:
      num_corrected_to_unknown_type++;
      return;
    case FieldFillingStatus::kCorrectedToEmpty:
      num_corrected_to_empty++;
      return;
    case FieldFillingStatus::kManuallyFilledToSameType:
      num_manually_filled_to_same_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToDifferentType:
      num_manually_filled_to_differt_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToUnknownType:
      num_manually_filled_to_unknown_type++;
      return;
    case FieldFillingStatus::kLeftEmpty:
      num_left_empty++;
      return;
  }
  NOTREACHED();
}

FieldFillingStatus GetFieldFillingStatus(const AutofillField& field) {
  const bool is_empty = field.IsEmpty();
  const bool possible_types_empty =
      !FieldHasMeaningfulPossibleFieldTypes(field);
  const bool possible_types_contain_type = TypeOfFieldIsPossibleType(field);

  if (field.is_autofilled)
    return FieldFillingStatus::kAccepted;

  if (field.previously_autofilled()) {
    if (is_empty)
      return FieldFillingStatus::kCorrectedToEmpty;

    if (possible_types_contain_type)
      return FieldFillingStatus::kCorrectedToSameType;

    if (possible_types_empty)
      return FieldFillingStatus::kCorrectedToUnknownType;

    return FieldFillingStatus::kCorrectedToDifferentType;
  }

  if (is_empty)
    return FieldFillingStatus::kLeftEmpty;

  if (possible_types_contain_type)
    return FieldFillingStatus::kManuallyFilledToSameType;

  if (possible_types_empty)
    return FieldFillingStatus::kManuallyFilledToUnknownType;

  return FieldFillingStatus::kManuallyFilledToDifferentType;
}

AutofillProfileSourceCategory GetCategoryOfProfile(
    const AutofillProfile& profile) {
  switch (profile.source()) {
    case AutofillProfile::Source::kLocalOrSyncable:
      return AutofillProfileSourceCategory::kLocalOrSyncable;
    case AutofillProfile::Source::kAccount:
      return profile.initial_creator_id() ==
                     AutofillProfile::kInitialCreatorOrModifierChrome
                 ? AutofillProfileSourceCategory::kAccountChrome
                 : AutofillProfileSourceCategory::kAccountNonChrome;
  }
}

const char* GetProfileCategorySuffix(AutofillProfileSourceCategory category) {
  switch (category) {
    case AutofillProfileSourceCategory::kLocalOrSyncable:
      return "Legacy";
    case AutofillProfileSourceCategory::kAccountChrome:
      return "AccountChrome";
    case AutofillProfileSourceCategory::kAccountNonChrome:
      return "AccountNonChrome";
  }
}

SettingsVisibleFieldTypeForMetrics ConvertSettingsVisibleFieldTypeForMetrics(
    ServerFieldType field_type) {
  switch (field_type) {
    case ServerFieldType::NAME_FULL:
      return SettingsVisibleFieldTypeForMetrics::kName;

    case ServerFieldType::EMAIL_ADDRESS:
      return SettingsVisibleFieldTypeForMetrics::kEmailAddress;

    case ServerFieldType::PHONE_HOME_WHOLE_NUMBER:
      return SettingsVisibleFieldTypeForMetrics::kPhoneNumber;

    case ServerFieldType::ADDRESS_HOME_CITY:
      return SettingsVisibleFieldTypeForMetrics::kCity;

    case ServerFieldType::ADDRESS_HOME_COUNTRY:
      return SettingsVisibleFieldTypeForMetrics::kCountry;

    case ServerFieldType::ADDRESS_HOME_ZIP:
      return SettingsVisibleFieldTypeForMetrics::kZip;

    case ServerFieldType::ADDRESS_HOME_STATE:
      return SettingsVisibleFieldTypeForMetrics::kState;

    case ServerFieldType::ADDRESS_HOME_STREET_ADDRESS:
      return SettingsVisibleFieldTypeForMetrics::kStreetAddress;

    case ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY:
      return SettingsVisibleFieldTypeForMetrics::kDependentLocality;

    case ServerFieldType::NAME_HONORIFIC_PREFIX:
      return SettingsVisibleFieldTypeForMetrics::kHonorificPrefix;

    case ServerFieldType::COMPANY_NAME:
      return SettingsVisibleFieldTypeForMetrics::kCompany;

    default:
      return SettingsVisibleFieldTypeForMetrics::kUndefined;
  }
}

void MergeFormGroupFillingStats(const FormGroupFillingStats& first,
                                FormGroupFillingStats& second) {
  second.num_accepted = first.num_accepted + second.num_accepted;
  second.num_corrected_to_same_type =
      first.num_corrected_to_same_type + second.num_corrected_to_same_type;
  second.num_corrected_to_different_type =
      first.num_corrected_to_different_type +
      second.num_corrected_to_different_type;
  second.num_corrected_to_unknown_type = first.num_corrected_to_unknown_type +
                                         second.num_corrected_to_unknown_type;
  second.num_corrected_to_empty =
      first.num_corrected_to_empty + second.num_corrected_to_empty;
  second.num_manually_filled_to_same_type =
      first.num_manually_filled_to_same_type +
      second.num_manually_filled_to_same_type;
  second.num_manually_filled_to_differt_type =
      first.num_manually_filled_to_differt_type +
      second.num_manually_filled_to_differt_type;
  second.num_manually_filled_to_unknown_type =
      first.num_manually_filled_to_unknown_type +
      second.num_manually_filled_to_unknown_type;
  second.num_left_empty = first.num_left_empty + second.num_left_empty;
}

}  // namespace autofill::autofill_metrics
