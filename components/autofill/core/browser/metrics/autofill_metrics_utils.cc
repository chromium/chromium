// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

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

    default:
      return SettingsVisibleFieldTypeForMetrics::kUndefined;
  }
}

}  // namespace autofill::autofill_metrics
