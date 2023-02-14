// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_import_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

namespace {

const char* GetSaveAndUpdatePromptDecisionMetricsSuffix(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  switch (decision) {
    case AutofillClient::SaveAddressProfileOfferUserDecision::kUndefined:
      return ".Undefined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kUserNotAsked:
      return ".UserNotAsked";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted:
      return ".Accepted";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined:
      return ".Declined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted:
      return ".EditAccepted";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined:
      return ".EditDeclined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kNever:
      return ".Never";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored:
      return ".Ignored";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kMessageTimeout:
      return ".MessageTimeout";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kMessageDeclined:
      return ".MessageDeclined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined:
      return ".AutoDeclined";
  }
  NOTREACHED();
  return "";
}

}  // namespace

void LogAddressProfileImportUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    AutofillProfileImportType import_type,
    AutofillClient::SaveAddressProfileOfferUserDecision user_decision,
    const ProfileImportMetadata& profile_import_metadata,
    size_t num_edited_fields) {
  ukm::builders::Autofill_AddressProfileImport(source_id)
      .SetAutocompleteUnrecognizedImport(
          profile_import_metadata
              .did_import_from_unrecognized_autocomplete_field)
      .SetImportType(static_cast<int64_t>(import_type))
      .SetNumberOfEditedFields(num_edited_fields)
      .SetPhoneNumberStatus(
          static_cast<int64_t>(profile_import_metadata.phone_import_status))
      .SetUserDecision(static_cast<int64_t>(user_decision))
      .Record(ukm_recorder);
}

void LogAddressFormImportRequirementMetric(
    AddressProfileImportRequirementMetric metric) {
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportRequirements",
                                metric);
}

void LogAddressFormImportCountrySpecificFieldRequirementsMetric(
    bool is_zip_missing,
    bool is_state_missing,
    bool is_city_missing,
    bool is_line1_missing) {
  const auto metric =
      static_cast<AddressProfileImportCountrySpecificFieldRequirementsMetric>(
          (is_zip_missing ? 0b1 : 0) | (is_state_missing ? 0b10 : 0) |
          (is_city_missing ? 0b100 : 0) | (is_line1_missing ? 0b1000 : 0));
  base::UmaHistogramEnumeration(
      "Autofill.AddressProfileImportCountrySpecificFieldRequirements", metric);
}

void LogAddressFormImportStatusMetric(AddressProfileImportStatusMetric metric) {
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportStatus", metric);
}

void LogProfileImportType(AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.ProfileImportType",
                                import_type);
}

void LogSilentUpdatesProfileImportType(AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.SilentUpdatesProfileImportType", import_type);
}

void LogNewProfileImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.NewProfileDecision",
                                decision);
}

void LogNewProfileWithIgnoredCountryImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.NewProfileWithIgnoredCountryDecision", decision);
}

void LogNewProfileNumberOfAutocompleteUnrecognizedFields(int count) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.NewProfileNumberOfAutocompleteUnrecognizedFields",
      count, /*exclusive_max=*/20);
}

void LogProfileUpdateImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.UpdateProfileDecision",
                                decision);
}

void LogProfileUpdateWithIgnoredCountryImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileWithIgnoredCountryDecision",
      decision);
}

void LogProfileUpdateNumberOfAutocompleteUnrecognizedFields(int count) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport."
      "UpdateProfileNumberOfAutocompleteUnrecognizedFields",
      count, /*exclusive_max=*/20);
}

// static
void LogRemovedSettingInaccessibleFields(bool did_remove) {
  base::UmaHistogramBoolean(
      "Autofill.ProfileImport.InaccessibleFieldsRemoved.Total", did_remove);
}

// static
void LogRemovedSettingInaccessibleField(ServerFieldType field) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.InaccessibleFieldsRemoved.ByFieldType",
      ConvertSettingsVisibleFieldTypeForMetrics(field));
}

// static
void LogPhoneNumberImportParsingResult(bool parsed_successfully) {
  base::UmaHistogramBoolean("Autofill.ProfileImport.PhoneNumberParsed",
                            parsed_successfully);
}

void LogNewProfileEditedType(ServerFieldType edited_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.NewProfileEditedType",
      ConvertSettingsVisibleFieldTypeForMetrics(edited_type));
}

void LogNewProfileNumberOfEditedFields(int number_of_edited_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.NewProfileNumberOfEditedFields",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void LogProfileUpdateAffectedType(
    ServerFieldType affected_type,
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  // Record the decision-specific metric.
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.ProfileImport.UpdateProfileAffectedType",
                    GetSaveAndUpdatePromptDecisionMetricsSuffix(decision)}),
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));

  // But also collect an histogram for any decision.
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileAffectedType.Any",
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));
}

void LogProfileUpdateEditedType(ServerFieldType edited_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileEditedType",
      ConvertSettingsVisibleFieldTypeForMetrics(edited_type));
}

void LogUpdateProfileNumberOfEditedFields(int number_of_edited_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.UpdateProfileNumberOfEditedFields",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void LogUpdateProfileNumberOfAffectedFields(
    int number_of_edited_fields,
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  // Record the decision-specific metric.
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields",
           GetSaveAndUpdatePromptDecisionMetricsSuffix(decision)}),
      number_of_edited_fields, /*exclusive_max=*/15);

  // But also collect an histogram for any decision.
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields.Any",
      number_of_edited_fields, /*exclusive_max=*/15);
}

}  // namespace autofill::autofill_metrics
