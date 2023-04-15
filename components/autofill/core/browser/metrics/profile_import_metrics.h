// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_IMPORT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_IMPORT_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill::autofill_metrics {

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. These values enumerates the
// status of the different requirements to successfully import an address
// profile from a form submission.
enum class AddressProfileImportRequirementMetric {
  // The form must contain either no or only a single unique email address.
  kEmailAddressUniqueRequirementFulfilled = 0,
  kEmailAddressUniqueRequirementViolated = 1,
  // The form is not allowed to contain invalid field types.
  kNoInvalidFieldTypesRequirementFulfilled = 2,
  kNoInvalidFieldTypesRequirementViolated = 3,
  // If required by |CountryData|, the form must contain a city entry.
  kCityRequirementFulfilled = 4,
  kCityRequirementViolated = 5,
  // If required by |CountryData|, the form must contain a state entry.
  kStateRequirementFulfilled = 6,
  kStateRequirementViolated = 7,
  // If required by |CountryData|, the form must contain a ZIP entry.
  kZipRequirementFulfilled = 8,
  kZipRequirementViolated = 9,
  // If present, the email address must be valid.
  kEmailValidRequirementFulfilled = 10,
  kEmailValidRequirementViolated = 11,
  // The country is no longer required. Instead, an invalid observed country is
  // replaced by the complement country logic.
  kCountryValidRequirementFulfilled = 12,
  kCountryValidRequirementViolated = 13,
  // If present, the state must be valid (if verifiable).
  kStateValidRequirementFulfilled = 14,
  kStateValidRequirementViolated = 15,
  // If present, the ZIP must be valid (if verifiable).
  kZipValidRequirementFulfilled = 16,
  kZipValidRequirementViolated = 17,
  // 18 and 19 are deprecated, as phone numbers are not a requirement anymore.
  // Indicates the overall status of the import requirements check.
  kOverallRequirementFulfilled = 20,
  kOverallRequirementViolated = 21,
  // If required by |CountryData|, the form must contain a line1 entry.
  kLine1RequirementFulfilled = 22,
  kLine1RequirementViolated = 23,
  // If required by |CountryData|, the form must contain a either a zip or a
  // state entry.
  kZipOrStateRequirementFulfilled = 24,
  kZipOrStateRequirementViolated = 25,
  // If required by |CountryData|, the form must contain a either an address
  // line 1 or a house number.
  kLine1OrHouseNumberRequirementFulfilled = 26,
  kLine1OrHouseNumberRequirementViolated = 27,
  // If required by `kAutofillRequireNameForProfileImportsFromForms` feature,
  // the form must contain a non-empty name.
  kNameRequirementFulfilled = 28,
  kNameRequirementViolated = 29,
  // Must be set to the last entry.
  kMaxValue = kNameRequirementViolated,
};

// Represents the status of the field type requirements that are specific to
// countries.
enum class AddressProfileImportCountrySpecificFieldRequirementsMetric {
  ALL_GOOD = 0,
  ZIP_REQUIREMENT_VIOLATED = 1,
  STATE_REQUIREMENT_VIOLATED = 2,
  ZIP_STATE_REQUIREMENT_VIOLATED = 3,
  CITY_REQUIREMENT_VIOLATED = 4,
  ZIP_CITY_REQUIREMENT_VIOLATED = 5,
  STATE_CITY_REQUIREMENT_VIOLATED = 6,
  ZIP_STATE_CITY_REQUIREMENT_VIOLATED = 7,
  LINE1_REQUIREMENT_VIOLATED = 8,
  LINE1_ZIP_REQUIREMENT_VIOLATED = 9,
  LINE1_STATE_REQUIREMENT_VIOLATED = 10,
  LINE1_ZIP_STATE_REQUIREMENT_VIOLATED = 11,
  LINE1_CITY_REQUIREMENT_VIOLATED = 12,
  LINE1_ZIP_CITY_REQUIREMENT_VIOLATED = 13,
  LINE1_STATE_CITY_REQUIREMENT_VIOLATED = 14,
  LINE1_ZIP_STATE_CITY_REQUIREMENT_VIOLATED = 15,
  kMaxValue = LINE1_ZIP_STATE_CITY_REQUIREMENT_VIOLATED,
};

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. These values represent the overall
// status of a profile import.
enum class AddressProfileImportStatusMetric {
  kNoImport = 0,
  kRegularImport = 1,
  // This value was deprecated in M113.
  kSectionUnionImport = 2,
  kMaxValue = kSectionUnionImport,
};

// Logs the address profile import UKM after the form submission.
// `user_decision` is the user's decision based on the storage prompt, if
// presented. `num_edited_fields` is the number of fields that were edited by
// the user before acceptance of the storage prompt. `profile_import_metadata`
// stores metadata related to the import of the address profiles.
void LogAddressProfileImportUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    AutofillProfileImportType import_type,
    AutofillClient::SaveAddressProfileOfferUserDecision user_decision,
    const ProfileImportMetadata& profile_import_metadata,
    size_t num_edited_fields);

// Logs the status of an address import requirement defined by type.
void LogAddressFormImportRequirementMetric(
    AddressProfileImportRequirementMetric metric);

// Logs the overall status of the country specific field requirements for
// importing an address profile from a submitted form.
void LogAddressFormImportCountrySpecificFieldRequirementsMetric(
    bool is_zip_missing,
    bool is_state_missing,
    bool is_city_missing,
    bool is_line1_missing);

// Logs the overall status of an address import upon form submission.
void LogAddressFormImportStatusMetric(AddressProfileImportStatusMetric metric);

// Logs the type of a profile import.
void LogProfileImportType(AutofillProfileImportType import_type);

// Logs the type of a profile import that are used for the silent updates.
void LogSilentUpdatesProfileImportType(AutofillProfileImportType import_type);

// Logs the user decision for importing a new profile.
void LogNewProfileImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision);

// Logs the user decision for updating an exiting profile.
void LogProfileUpdateImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision);

// Logs if at least one setting-inaccessible field was removed on import.
void LogRemovedSettingInaccessibleFields(bool did_remove);

// Logs that `field` was removed from a profile on import, because it is
// setting-inaccessible in the profile's country.
void LogRemovedSettingInaccessibleField(ServerFieldType field);

// Logs whether a phone number was parsed successfully on profile import.
// Contrary to the profile import requirement metrics, the parsing result is
// only emitted when a number is present.
void LogPhoneNumberImportParsingResult(bool parsed_successfully);

// Logs the number of fields with an unrecognized autocomplete attributed that
// were considered for the import due to AutofillFillAndImportFromMoreFields.
void LogNewProfileNumberOfAutocompleteUnrecognizedFields(int count);

// Logs the number of fields with an unrecognized autocomplete attributed that
// were considered for the update due to AutofillFillAndImportFromMoreFields.
void LogProfileUpdateNumberOfAutocompleteUnrecognizedFields(int count);

// Logs that a specific type was edited in a save prompt.
void LogNewProfileEditedType(ServerFieldType edited_type);

// Logs the number of edited fields for an accepted profile save.
void LogNewProfileNumberOfEditedFields(int number_of_edited_fields);

// Logs that a specific type changed in a profile update that received the
// user |decision|. Note that additional manual edits in the update prompt are
// not accounted for in this metric.
void LogProfileUpdateAffectedType(
    ServerFieldType affected_type,
    AutofillClient::SaveAddressProfileOfferUserDecision decision);

// Logs that a specific type was edited in an update prompt.
void LogProfileUpdateEditedType(ServerFieldType edited_type);

// Logs the number of edited fields for an accepted profile update.
void LogUpdateProfileNumberOfEditedFields(int number_of_edited_fields);

// Logs the number of changed fields for a profile update that received the
// user |decision|. Note that additional manual edits in the update prompt are
// not accounted for in this metric.
void LogUpdateProfileNumberOfAffectedFields(
    int number_of_affected_fields,
    AutofillClient::SaveAddressProfileOfferUserDecision decision);

// Logs the user's decision for migrating an existing `kLocalOrSyncable` profile
// to `kAccount`.
void LogProfileMigrationImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision);

// Logs that a specific type was edited in a migration prompt.
void LogProfileMigrationEditedType(ServerFieldType edited_type);

// Logs the number of edited fields for an accepted profile migration.
void LogProfileMigrationNumberOfEditedFields(int number_of_edited_fields);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_IMPORT_METRICS_H_
