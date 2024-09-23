// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_requirement_utils.h"

#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

// Stores the collection of AddressImportRequirement that are violated. These
// violation prevents the import of a profile.
constexpr AddressImportRequirement kMinimumAddressRequirementViolations[] = {
    AddressImportRequirement::kLine1RequirementViolated,
    AddressImportRequirement::kCityRequirementViolated,
    AddressImportRequirement::kStateRequirementViolated,
    AddressImportRequirement::kZipRequirementViolated,
    AddressImportRequirement::kZipOrStateRequirementViolated,
    AddressImportRequirement::kLine1OrHouseNumberRequirementViolated};

}  // anonymous namespace

std::vector<autofill_metrics::AddressProfileImportRequirementMetric>
ValidateProfileImportRequirements(const AutofillProfile& profile,
                                  LogBuffer* import_log_buffer) {
  CHECK(profile.HasInfo(ADDRESS_HOME_COUNTRY));

  std::vector<AddressImportRequirement> address_import_requirements;
  // Validates the `profile` by testing that it has information for at least one
  // of the `types`. If `required` is false, it is considered trivially valid.
  // Logs the profile's validity to UMA and autofill-internals.
  auto ValidateAndLog =
      [&](bool required, const std::vector<FieldType>& types,
          AddressImportRequirement valid, AddressImportRequirement invalid) {
        const bool is_valid =
            !required || std::ranges::any_of(types, [&](FieldType type) {
              return profile.HasRawInfo(type);
            });
        if (is_valid) {
          address_import_requirements.push_back(valid);
        } else {
          address_import_requirements.push_back(invalid);
          LOG_AF(import_log_buffer)
              << LogMessage::kImportAddressProfileFromFormFailed
              << "Missing required " <<
              [&] {
                std::vector<std::string_view> type_names;
                for (FieldType type : types) {
                  type_names.push_back(FieldTypeToStringView(type));
                }
                return base::JoinString(type_names, " or ");
              }()
              << "." << CTag{};
        }

        return is_valid;
      };

  AutofillCountry country(
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)));
  // Include the details of the country to the log.
  LOG_AF(import_log_buffer) << country;

  ValidateAndLog(country.requires_line1(),
                 {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_NAME},
                 AddressImportRequirement::kLine1RequirementFulfilled,
                 AddressImportRequirement::kLine1RequirementViolated);
  std::vector<FieldType> city_types = {ADDRESS_HOME_CITY};
  if (country.country_code() == "MX") {
    city_types.push_back(ADDRESS_HOME_ADMIN_LEVEL2);
  }
  ValidateAndLog(country.requires_city(), city_types,
                 AddressImportRequirement::kCityRequirementFulfilled,
                 AddressImportRequirement::kCityRequirementViolated);
  ValidateAndLog(country.requires_state(), {ADDRESS_HOME_STATE},
                 AddressImportRequirement::kStateRequirementFulfilled,
                 AddressImportRequirement::kStateRequirementViolated);
  ValidateAndLog(country.requires_zip(), {ADDRESS_HOME_ZIP},
                 AddressImportRequirement::kZipRequirementFulfilled,
                 AddressImportRequirement::kZipRequirementViolated);
  ValidateAndLog(country.requires_zip_or_state(),
                 {ADDRESS_HOME_ZIP, ADDRESS_HOME_STATE},
                 AddressImportRequirement::kZipOrStateRequirementFulfilled,
                 AddressImportRequirement::kZipOrStateRequirementViolated);
  ValidateAndLog(
      country.requires_line1_or_house_number(),
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_HOUSE_NUMBER},
      AddressImportRequirement::kLine1OrHouseNumberRequirementFulfilled,
      AddressImportRequirement::kLine1OrHouseNumberRequirementViolated);

  return address_import_requirements;
}

bool ValidateNonEmptyValues(const AutofillProfile& profile,
                            LogBuffer* log_buffer) {
  // Returns false if `profile` has invalid information for `type`.
  auto ValidateAndLog = [&](FieldType type, AddressImportRequirement valid,
                            AddressImportRequirement invalid) {
    if (profile.IsPresentButInvalid(type)) {
      autofill_metrics::LogAddressFormImportRequirementMetric(invalid);
      LOG_AF(log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                         << "Invalid " << FieldTypeToStringView(type) << "."
                         << CTag{};
      return false;
    } else {
      autofill_metrics::LogAddressFormImportRequirementMetric(valid);
      return true;
    }
  };

  // Reject profiles with invalid `EMAIL_ADDRESS`, `ADDRESS_HOME_STATE` or
  // `ADDRESS_HOME_ZIP` entries and collect metrics on their validity.
  bool all_requirements_satisfied = ValidateAndLog(
      EMAIL_ADDRESS, AddressImportRequirement::kEmailValidRequirementFulfilled,
      AddressImportRequirement::kEmailValidRequirementViolated);

  all_requirements_satisfied &=
      ValidateAndLog(ADDRESS_HOME_STATE,
                     AddressImportRequirement::kStateValidRequirementFulfilled,
                     AddressImportRequirement::kStateValidRequirementViolated);

  all_requirements_satisfied &= ValidateAndLog(
      ADDRESS_HOME_ZIP, AddressImportRequirement::kZipValidRequirementFulfilled,
      AddressImportRequirement::kZipValidRequirementViolated);

  return all_requirements_satisfied;
}

bool IsMinimumAddress(const AutofillProfile& profile, LogBuffer* log_buffer) {
  const std::vector<std::string>& country_codes =
      autofill::CountryDataMap::GetInstance()->country_codes();
  if (!base::Contains(country_codes, base::UTF16ToUTF8(profile.GetRawInfo(
                                         ADDRESS_HOME_COUNTRY)))) {
    return false;
  }
  std::vector<AddressImportRequirement> address_requirements =
      ValidateProfileImportRequirements(profile, log_buffer);
  return !std::ranges::any_of(
      kMinimumAddressRequirementViolations,
      [&](AddressImportRequirement address_requirement_violation) {
        return base::Contains(address_requirements,
                              address_requirement_violation);
      });
}

bool IsEligibleForMigrationToAccount(
    const AddressDataManager& address_data_manager,
    const AutofillProfile& profile) {
  return address_data_manager.IsEligibleForAddressAccountStorage() &&
         IsProfileEligibleForMigrationToAccount(address_data_manager, profile);
}

bool IsProfileEligibleForMigrationToAccount(
    const AddressDataManager& address_data_manager,
    const AutofillProfile& profile) {
  return !address_data_manager.IsProfileMigrationBlocked(profile.guid()) &&
         address_data_manager.IsCountryEligibleForAccountStorage(
             base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)));
}

}  // namespace autofill
