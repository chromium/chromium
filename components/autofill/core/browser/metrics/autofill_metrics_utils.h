// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
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

// Used to store `CalculateMinimalIncompatibleProfileWithTypeSets() ` result. It
// contains the `profile` that was being compared and a set of FieldTypes that
// had different values.
struct DifferingProfileWithTypeSet {
  const raw_ptr<const AutofillProfile> profile;
  const FieldTypeSet field_type_set;

  bool operator==(const DifferingProfileWithTypeSet& other) const = default;
};

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
  kAlternativeName = 13,
  kMaxValue = kAlternativeName
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

// Returns whether the caller should log autofill suggestions shown metrics.
// Some suggestions can be "displayed" without a direct user action (i.e. typing
// into a field or unfocusing a text area with a previous
// `FillingProduct::kCompose` suggestion). We do not want to log suggestion
// shown logs for them since they defeat the purpose of the metric.
bool ShouldLogAutofillSuggestionShown(
    AutofillSuggestionTriggerSource trigger_source);

// This function encodes the integer value of a `FieldType` and the
// boolean value of `suggestion_accepted` into a 14 bit integer.
// The lower 2 bits are used to encode the filling acceptance and the higher 12
// bits are used to encode the field type. This integer is used to determine
// which bucket of metrics such as
// "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType"
// should be emitted.
// Even though `suggestion_accepted` could be encoded in only 1 bit, 2 bits are
// used to leave room for possible other future values.
int GetBucketForAcceptanceMetricsGroupedByFieldType(FieldType field_type,
                                                    bool suggestion_accepted);

// Given the result of `CalculateMinimalIncompatibleProfileWithTypeSets()`,
// returns the minimum number of fields whose removal makes `import_candidate` a
// duplicate of any entry in `existing_profiles`. Returns
// `std::numeric_limits<int>::max()` in case `min_incompatible_sets` is empty.
int GetDuplicationRank(
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets);

// Returns 64-bit hash of the string of form global id, which consists of
// |frame_token| and |renderer_id|.
uint64_t FormGlobalIdToHash64Bit(const FormGlobalId& form_global_id);
// Returns 64-bit hash of the string of field global id, which consists of
// |frame_token| and |renderer_id|.
uint64_t FieldGlobalIdToHash64Bit(const FieldGlobalId& field_global_id);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
