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

// kAccount profiles are synced from an external source and have potentially
// originated from outside of Autofill. In order to determine the added value
// for Autofill, the `AutofillProfile::Source` is further resolved in some
// metrics.
enum class AutofillProfileSourceCategory {
  kLocalOrSyncable = 0,
  kAccountChrome = 1,
  kAccountNonChrome = 2,
  kMaxValue = kAccountNonChrome
};

// Maps the `profile` to its category, depending on the profile's `source()`
// and `initial_creator()`.
AutofillProfileSourceCategory GetCategoryOfProfile(
    const AutofillProfile& profile);

// Converts the `category` to the histogram-suffix used for resolving some
// metrics by category.
const char* GetProfileCategorySuffix(AutofillProfileSourceCategory category);

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

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
