// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer_utils.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    AutofillMetrics::AddressProfileImportRequirementMetric;

bool IsOriginPartOfDeletionInfo(const absl::optional<url::Origin>& origin,
                                const history::DeletionInfo& deletion_info) {
  if (!origin)
    return false;
  return deletion_info.IsAllHistory() ||
         base::Contains(deletion_info.deleted_rows(), *origin,
                        [](const history::URLRow& url_row) {
                          return url::Origin::Create(url_row.url());
                        });
}

}  // anonymous namespace

bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& predicted_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer,
                      bool collect_metrics) {
  // Validates the `profile` by testing that it has information for at least one
  // of the `types`. If `required` is false, it is considered trivially valid.
  // Logs the profile's validity to UMA and autofill-internals.
  auto ValidateAndLog = [&](bool required,
                            const std::vector<ServerFieldType>& types,
                            AddressImportRequirement valid,
                            AddressImportRequirement invalid) {
    if (!required || base::ranges::any_of(types, [&](ServerFieldType type) {
          return profile.HasRawInfo(type);
        })) {
      AutofillMetrics::LogAddressFormImportRequirementMetric(valid);
      return true;
    }
    AutofillMetrics::LogAddressFormImportRequirementMetric(invalid);
    LOG_AF(import_log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                              << "Missing required " <<
        [&] {
          std::vector<base::StringPiece> type_names;
          for (auto& type : types)
            type_names.push_back(FieldTypeToStringPiece(type));
          return base::JoinString(type_names, " or ");
        }() << "." << CTag{};
    return false;
  };

  AutofillCountry country(predicted_country_code, app_locale);
  // Include the details of the country to the log.
  LOG_AF(import_log_buffer) << country;

  bool is_line1_missing = !ValidateAndLog(
      country.requires_line1(), {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_NAME},
      AddressImportRequirement::LINE1_REQUIREMENT_FULFILLED,
      AddressImportRequirement::LINE1_REQUIREMENT_VIOLATED);

  bool is_city_missing =
      !ValidateAndLog(country.requires_city(), {ADDRESS_HOME_CITY},
                      AddressImportRequirement::CITY_REQUIREMENT_FULFILLED,
                      AddressImportRequirement::CITY_REQUIREMENT_VIOLATED);

  bool is_state_missing =
      !ValidateAndLog(country.requires_state(), {ADDRESS_HOME_STATE},
                      AddressImportRequirement::STATE_REQUIREMENT_FULFILLED,
                      AddressImportRequirement::STATE_REQUIREMENT_VIOLATED);

  bool is_zip_missing =
      !ValidateAndLog(country.requires_zip(), {ADDRESS_HOME_ZIP},
                      AddressImportRequirement::ZIP_REQUIREMENT_FULFILLED,
                      AddressImportRequirement::ZIP_REQUIREMENT_VIOLATED);

  bool is_zip_or_state_requirement_violated = !ValidateAndLog(
      country.requires_zip_or_state(), {ADDRESS_HOME_ZIP, ADDRESS_HOME_STATE},
      AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_FULFILLED,
      AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_VIOLATED);

  bool is_line1_or_house_number_violated = !ValidateAndLog(
      country.requires_line1_or_house_number(),
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_HOUSE_NUMBER},
      AddressImportRequirement::LINE1_OR_HOUSE_NUMBER_REQUIREMENT_FULFILLED,
      AddressImportRequirement::LINE1_OR_HOUSE_NUMBER_REQUIREMENT_VIOLATED);

  if (collect_metrics) {
    AutofillMetrics::LogAddressFormImportCountrySpecificFieldRequirementsMetric(
        is_zip_missing, is_state_missing, is_city_missing, is_line1_missing);
  }

  // Return true if all requirements are fulfilled.
  return !(is_line1_missing || is_city_missing || is_state_missing ||
           is_zip_missing || is_zip_or_state_requirement_violated ||
           is_line1_or_house_number_violated);
}

bool IsValidLearnableProfile(const AutofillProfile& profile,
                             LogBuffer* import_log_buffer) {
  // Returns false if `profile` has invalid information for `type`.
  auto ValidateAndLog = [&](ServerFieldType type,
                            AddressImportRequirement valid,
                            AddressImportRequirement invalid) {
    if (profile.IsPresentButInvalid(type)) {
      AutofillMetrics::LogAddressFormImportRequirementMetric(invalid);
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormFailed << "Invalid "
          << FieldTypeToStringPiece(type) << "." << CTag{};
      return false;
    } else {
      AutofillMetrics::LogAddressFormImportRequirementMetric(valid);
      return true;
    }
  };

  // Reject profiles with invalid `EMAIL_ADDRESS`, `ADDRESS_HOME_STATE` or
  // `ADDRESS_HOME_ZIP` entries and collect metrics on their validity.
  bool all_requirements_satisfied = ValidateAndLog(
      EMAIL_ADDRESS,
      AddressImportRequirement::EMAIL_VALID_REQUIREMENT_FULFILLED,
      AddressImportRequirement::EMAIL_VALID_REQUIREMENT_VIOLATED);

  all_requirements_satisfied &= ValidateAndLog(
      ADDRESS_HOME_STATE,
      AddressImportRequirement::STATE_VALID_REQUIREMENT_FULFILLED,
      AddressImportRequirement::STATE_VALID_REQUIREMENT_VIOLATED);

  all_requirements_satisfied &=
      ValidateAndLog(ADDRESS_HOME_ZIP,
                     AddressImportRequirement::ZIP_VALID_REQUIREMENT_FULFILLED,
                     AddressImportRequirement::ZIP_VALID_REQUIREMENT_VIOLATED);

  return all_requirements_satisfied;
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

void MultiStepImportMerger::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  if (IsOriginPartOfDeletionInfo(multistep_candidates_.origin(), deletion_info))
    Clear();
}

FormAssociator::FormAssociator() = default;
FormAssociator::~FormAssociator() = default;

void FormAssociator::TrackFormAssociations(const url::Origin& origin,
                                           FormSignature form_signature,
                                           FormType form_type) {
  const base::TimeDelta ttl = features::kAutofillAssociateFormsTTL.Get();
  // This ensures that `recent_address_forms_` and `recent_credit_card_forms`
  // share the same origin (if they are non-empty).
  recent_address_forms_.RemoveOutdatedItems(ttl, origin);
  recent_credit_card_forms_.RemoveOutdatedItems(ttl, origin);

  auto& container = form_type == FormType::kAddressForm
                        ? recent_address_forms_
                        : recent_credit_card_forms_;
  container.Push(form_signature, origin);
}

absl::optional<FormStructure::FormAssociations>
FormAssociator::GetFormAssociations(FormSignature form_signature) const {
  FormStructure::FormAssociations associations;
  if (!recent_address_forms_.empty())
    associations.last_address_form_submitted = *recent_address_forms_.begin();
  if (!recent_credit_card_forms_.empty()) {
    associations.last_credit_card_form_submitted =
        *recent_credit_card_forms_.begin();
  }
  if (associations.last_address_form_submitted != form_signature &&
      associations.last_credit_card_form_submitted != form_signature) {
    return absl::nullopt;
  }
  if (recent_address_forms_.size() > 1) {
    associations.second_last_address_form_submitted =
        *std::next(recent_address_forms_.begin());
  }
  return associations;
}

const absl::optional<url::Origin>& FormAssociator::origin() const {
  DCHECK(
      !recent_address_forms_.origin() || !recent_credit_card_forms_.origin() ||
      *recent_address_forms_.origin() == *recent_credit_card_forms_.origin());
  return recent_address_forms_.origin() ? recent_address_forms_.origin()
                                        : recent_credit_card_forms_.origin();
}

void FormAssociator::Clear() {
  recent_address_forms_.Clear();
  recent_credit_card_forms_.Clear();
}

void FormAssociator::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  if (IsOriginPartOfDeletionInfo(origin(), deletion_info))
    Clear();
}

}  // namespace autofill
