// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

namespace internal {

// Returns `true` if `form` has at least one email field and otherwise nothing
// but unknown, password or additional email fields.
bool IsEmailOnlyForm(const FormStructure& form);

// Returns `true` if `form` has at least 3 distinct field types of
// `FieldTypeGroup::kAddress` that are not country, and those field types are
// not equal to `kFieldTypesOfATypicalStoreLocatorForm`. Returns `false`
// otherwise.
bool IsPostalAddressForm(const FormStructure& form);

}  // namespace internal

// kAccount profiles are synced from an external source and have potentially
// originated from outside of Autofill. In order to determine the added value
// for Autofill, the `AutofillProfile::RecordType` is further resolved in some
// metrics.
enum class AutofillProfileRecordTypeCategory {
  kLocalOrSyncable = 0,
  kAccountChrome = 1,
  kAccountNonChrome = 2,
  kMaxValue = kAccountNonChrome
};

// Maps the `profile` to its category, depending on the profile's
// `record_type()` and `initial_creator()`.
AutofillProfileRecordTypeCategory GetCategoryOfProfile(
    const AutofillProfile& profile);

// Converts the `category` to the histogram-suffix used for resolving some
// metrics by category.
const char* GetProfileCategorySuffix(
    AutofillProfileRecordTypeCategory category);

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. This is the subset of field
// types that can be changed in a profile change/store dialog or are affected
// in a profile merge operation.
enum class SettingsVisibleFieldTypeForMetrics {
  kUndefined = 0,
  kName = 1,
  kEmailAddress = 2,
  kPhoneNumber = 3,
  kCity = 4,
  kCountry = 5,
  kZip = 6,
  kState = 7,
  kStreetAddress = 8,
  kDependentLocality = 9,
  // kHonorificPrefix = 10,  // Deprecated in M123.
  kCompany = 11,
  kAdminLevel2 = 12,
  kMaxValue = kAdminLevel2
};

// Converts a server field type that can be edited in the settings to an enum
// used for metrics.
SettingsVisibleFieldTypeForMetrics ConvertSettingsVisibleFieldTypeForMetrics(
    FieldType field_type);

// Returns the set of all fillable form types for `form.`
DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
    const FormStructure& form);

// Returns GetFormTypesForLogging() where entries need to correspond to
// `FormType::kAddressForm`.
DenseSet<FormTypeNameForLogging> GetAddressFormTypesForLogging(
    const FormStructure& form);

// Returns GetFormTypesForLogging() where entries need to correspond to
// `FormType::kCreditCardForm` or `FormType::kStandaloneCvcForm`.
DenseSet<FormTypeNameForLogging> GetCreditCardFormTypesForLogging(
    const FormStructure& form);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
