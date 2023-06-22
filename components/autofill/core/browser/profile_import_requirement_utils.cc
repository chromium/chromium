// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_import_requirement_utils.h"

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

}  // anonymous namespace

bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& predicted_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer,
                      bool collect_metrics) {
  // Validates the `profile` by testing that it has information for at least one
  // of the `types`. If `required` is false, it is considered trivially valid.
  // Logs the profile's validity to UMA and autofill-internals.
  auto ValidateAndLog =
      [&](bool required, const std::vector<ServerFieldType>& types,
          AddressImportRequirement valid, AddressImportRequirement invalid) {
        const bool is_valid =
            !required || base::ranges::any_of(types, [&](ServerFieldType type) {
              return profile.HasRawInfo(type);
            });
        if (!is_valid) {
          LOG_AF(import_log_buffer)
              << LogMessage::kImportAddressProfileFromFormFailed
              << "Missing required " <<
              [&] {
                std::vector<base::StringPiece> type_names;
                for (auto& type : types) {
                  type_names.push_back(FieldTypeToStringPiece(type));
                }
                return base::JoinString(type_names, " or ");
              }()
              << "." << CTag{};
        }
        if (collect_metrics) {
          autofill_metrics::LogAddressFormImportRequirementMetric(
              is_valid ? valid : invalid);
        }
        return is_valid;
      };

  AutofillCountry country(predicted_country_code, app_locale);
  // Include the details of the country to the log.
  LOG_AF(import_log_buffer) << country;

  const bool is_line1_missing = !ValidateAndLog(
      country.requires_line1(), {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_NAME},
      AddressImportRequirement::kLine1RequirementFulfilled,
      AddressImportRequirement::kLine1RequirementViolated);

  const bool is_city_missing =
      !ValidateAndLog(country.requires_city(), {ADDRESS_HOME_CITY},
                      AddressImportRequirement::kCityRequirementFulfilled,
                      AddressImportRequirement::kCityRequirementViolated);

  const bool is_state_missing =
      !ValidateAndLog(country.requires_state(), {ADDRESS_HOME_STATE},
                      AddressImportRequirement::kStateRequirementFulfilled,
                      AddressImportRequirement::kStateRequirementViolated);

  const bool is_zip_missing =
      !ValidateAndLog(country.requires_zip(), {ADDRESS_HOME_ZIP},
                      AddressImportRequirement::kZipRequirementFulfilled,
                      AddressImportRequirement::kZipRequirementViolated);

  const bool is_zip_or_state_requirement_violated = !ValidateAndLog(
      country.requires_zip_or_state(), {ADDRESS_HOME_ZIP, ADDRESS_HOME_STATE},
      AddressImportRequirement::kZipOrStateRequirementFulfilled,
      AddressImportRequirement::kZipOrStateRequirementViolated);

  const bool is_line1_or_house_number_violated = !ValidateAndLog(
      country.requires_line1_or_house_number(),
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_HOUSE_NUMBER},
      AddressImportRequirement::kLine1OrHouseNumberRequirementFulfilled,
      AddressImportRequirement::kLine1OrHouseNumberRequirementViolated);

  bool is_minimum_address =
      !(is_line1_missing || is_city_missing || is_state_missing ||
        is_zip_missing || is_zip_or_state_requirement_violated ||
        is_line1_or_house_number_violated);
  // TODO(crbug.com/1413205): Merge this into is_minimum_address.
  if (is_minimum_address && country.requires_full_name()) {
    is_minimum_address &=
        ValidateAndLog(/*required=*/true, {NAME_FULL},
                       AddressImportRequirement::kNameRequirementFulfilled,
                       AddressImportRequirement::kNameRequirementViolated);
  }
  if (collect_metrics) {
    autofill_metrics::
        LogAddressFormImportCountrySpecificFieldRequirementsMetric(
            is_zip_missing, is_state_missing, is_city_missing,
            is_line1_missing);
  }
  return is_minimum_address;
}

}  // namespace autofill
