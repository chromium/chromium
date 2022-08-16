// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    AutofillMetrics::AddressProfileImportRequirementMetric;

}  // anonymous namespace

bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& predicted_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer,
                      bool collect_metrics) {
  AutofillCountry country(predicted_country_code, app_locale);

  // Include the details of the country to the log.
  LOG_AF(import_log_buffer) << country;

  // Check the |ADDRESS_HOME_LINE1| requirement.
  bool is_line1_missing = false;
  if (country.requires_line1() && !profile.HasRawInfo(ADDRESS_HOME_LINE1) &&
      !profile.HasRawInfo(ADDRESS_HOME_STREET_NAME)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Missing required ADDRESS_HOME_LINE1." << CTag{};
    is_line1_missing = true;
  }

  // Check the |ADDRESS_HOME_CITY| requirement.
  bool is_city_missing = false;
  if (country.requires_city() && !profile.HasRawInfo(ADDRESS_HOME_CITY)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Missing required ADDRESS_HOME_CITY." << CTag{};
    is_city_missing = true;
  }

  // Check the |ADDRESS_HOME_STATE| requirement.
  bool is_state_missing = false;
  if (country.requires_state() && !profile.HasRawInfo(ADDRESS_HOME_STATE)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Missing required ADDRESS_HOME_STATE." << CTag{};
    is_state_missing = true;
  }

  // Check the |ADDRESS_HOME_ZIP| requirement.
  bool is_zip_missing = false;
  if (country.requires_zip() && !profile.HasRawInfo(ADDRESS_HOME_ZIP)) {
    LOG_AF(import_log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                              << "Missing required ADDRESS_HOME_ZIP." << CTag{};
    is_zip_missing = true;
  }

  bool is_zip_or_state_requirement_violated = false;
  if (country.requires_zip_or_state() &&
      !profile.HasRawInfo(ADDRESS_HOME_ZIP) &&
      !profile.HasRawInfo(ADDRESS_HOME_STATE)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Missing required ADDRESS_HOME_ZIP or ADDRESS_HOME_STATE." << CTag{};
    is_zip_or_state_requirement_violated = true;
  }

  bool is_line1_or_house_number_violated = false;
  if (country.requires_line1_or_house_number() &&
      !profile.HasRawInfo(ADDRESS_HOME_LINE1) &&
      !profile.HasRawInfo(ADDRESS_HOME_HOUSE_NUMBER)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Missing required ADDRESS_HOME_LINE1 or ADDRESS_HOME_HOUSE_NUMBER."
        << CTag{};
    is_line1_or_house_number_violated = true;
  }

  // Collect metrics regarding the requirements.
  if (collect_metrics) {
    AutofillMetrics::LogAddressFormImportRequirementMetric(
        is_line1_missing
            ? AddressImportRequirement::LINE1_REQUIREMENT_VIOLATED
            : AddressImportRequirement::LINE1_REQUIREMENT_FULFILLED);

    AutofillMetrics::LogAddressFormImportRequirementMetric(
        is_city_missing ? AddressImportRequirement::CITY_REQUIREMENT_VIOLATED
                        : AddressImportRequirement::CITY_REQUIREMENT_FULFILLED);

    AutofillMetrics::LogAddressFormImportRequirementMetric(
        is_state_missing
            ? AddressImportRequirement::STATE_REQUIREMENT_VIOLATED
            : AddressImportRequirement::STATE_REQUIREMENT_FULFILLED);

    AutofillMetrics::LogAddressFormImportRequirementMetric(
        is_zip_missing ? AddressImportRequirement::ZIP_REQUIREMENT_VIOLATED
                       : AddressImportRequirement::ZIP_REQUIREMENT_FULFILLED);

    AutofillMetrics::LogAddressFormImportRequirementMetric(
        is_zip_or_state_requirement_violated
            ? AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_VIOLATED
            : AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_FULFILLED);

    AutofillMetrics::LogAddressFormImportCountrySpecificFieldRequirementsMetric(
        is_zip_missing, is_state_missing, is_city_missing, is_line1_missing);
  }

  // Return true if all requirements are fulfilled.
  return !(is_line1_missing || is_city_missing || is_state_missing ||
           is_zip_missing || is_zip_or_state_requirement_violated ||
           is_line1_or_house_number_violated);
}

std::string GetPredictedCountryCode(const AutofillProfile& profile,
                                    const std::string& variation_country_code,
                                    const std::string& app_locale,
                                    LogBuffer* import_log_buffer) {
  // Try to acquire the country code form the filled form.
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));

  if (import_log_buffer && !country_code.empty()) {
    *import_log_buffer << LogMessage::kImportAddressProfileFromFormCountrySource
                       << "Country entry in form." << CTag{};
  }

  // As a fallback, use the variation service state to get a country code.
  if (country_code.empty() && !variation_country_code.empty()) {
    country_code = variation_country_code;
    if (import_log_buffer) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormCountrySource
          << "Variations service." << CTag{};
    }
  }

  // As the last resort, derive the country code from the app_locale.
  if (country_code.empty()) {
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);
    if (import_log_buffer && !country_code.empty()) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormCountrySource
          << "App locale." << CTag{};
    }
  }

  return country_code;
}

MultiStepImportMerger::MultiStepImportMerger(
    const std::string& app_locale,
    const std::string& variation_country_code)
    : app_locale_(app_locale),
      variation_country_code_(variation_country_code) {}
MultiStepImportMerger::~MultiStepImportMerger() {}

void MultiStepImportMerger::ProcessMultiStepImport(
    AutofillProfile& profile,
    ProfileImportMetadata& import_metadata,
    const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableMultiStepImports)) {
    return;
  }

  multistep_candidates_.RemoveOutdatedItems(
      features::kAutofillMultiStepImportCandidateTTL.Get(), origin);
  bool has_min_address_requirements =
      MergeProfileWithMultiStepCandidates(profile, import_metadata, origin);

  if (!has_min_address_requirements ||
      features::kAutofillEnableMultiStepImportComplements.Get()) {
    // Add `profile| as a `multistep_candidate`. This happens for incomplete
    // profiles, which can then be complemented in later steps. When
    // `kAutofillEnableMultiStepImportComplements` is enabled, complete profiles
    // are stored too, which enables updating them in later steps.
    // In the latter case, Autofill tries to import the `profile`. This logs
    // metrics depending on `import_metadata`. To prevent double counting,
    // an we store an empty `ProfileImportMetadata` object in this case.
    multistep_candidates_.Push({.profile = profile,
                                .import_metadata = has_min_address_requirements
                                                       ? ProfileImportMetadata()
                                                       : import_metadata},
                               origin);
  }
}

bool MultiStepImportMerger::MergeProfileWithMultiStepCandidates(
    AutofillProfile& profile,
    ProfileImportMetadata& import_metadata,
    const url::Origin& origin) {
  // Greedily merge with a prefix of |multistep_candidates|.
  AutofillProfileComparator comparator(app_locale_);
  auto candidate = multistep_candidates_.begin();
  AutofillProfile completed_profile = profile;
  ProfileImportMetadata completed_metadata = import_metadata;
  // Country completion has not happened yet, so this field can be ignored.
  DCHECK(!completed_metadata.did_complement_country);
  while (candidate != multistep_candidates_.end()) {
    if (!comparator.AreMergeable(completed_profile, candidate->profile) ||
        completed_profile.MergeDataFrom(candidate->profile, app_locale_)) {
      break;
    }
    // ProfileImportMetadata is only relevant for metrics. If the phone number
    // was removed from a partial profile, we still want that removal to appear
    // in the metrics, because it would have hindered that partial profile from
    // import and merging.
    completed_metadata.did_remove_invalid_phone_number |=
        candidate->import_metadata.did_remove_invalid_phone_number;
    candidate++;
  }

  // The minimum address requirements depend on the country, which has possibly
  // changed as a result of the merge.
  if (IsMinimumAddress(
          completed_profile,
          GetPredictedCountryCode(completed_profile, variation_country_code_,
                                  app_locale_, /*import_log_buffer=*/nullptr),
          app_locale_,
          /*import_log_buffer=*/nullptr, /*collect_metrics=*/false)) {
    profile = std::move(completed_profile);
    import_metadata = std::move(completed_metadata);
    multistep_candidates_.Clear();
    return true;
  } else {
    // Remove all profiles that couldn't be merged.
    multistep_candidates_.erase(candidate, multistep_candidates_.end());
    return false;
  }
}

}  // namespace autofill
