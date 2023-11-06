// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_requirement_utils.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

}  // anonymous namespace

base::flat_set<autofill_metrics::AddressProfileImportRequirementMetric>
GetAutofillProfileRequirementResult(const AutofillProfile& profile,
                                    LogBuffer* import_log_buffer) {
  CHECK(profile.HasInfo(ADDRESS_HOME_COUNTRY));

  std::vector<AddressImportRequirement> address_import_requirements;
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
        if (is_valid) {
          address_import_requirements.push_back(valid);
        } else {
          address_import_requirements.push_back(invalid);
          LOG_AF(import_log_buffer)
              << LogMessage::kImportAddressProfileFromFormFailed
              << "Missing required " <<
              [&] {
                std::vector<std::string_view> type_names;
                for (ServerFieldType type : types) {
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

  bool is_valid = ValidateAndLog(
      country.requires_line1(), {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_NAME},
      AddressImportRequirement::kLine1RequirementFulfilled,
      AddressImportRequirement::kLine1RequirementViolated);

  is_valid &=
      ValidateAndLog(country.requires_city(), {ADDRESS_HOME_CITY},
                     AddressImportRequirement::kCityRequirementFulfilled,
                     AddressImportRequirement::kCityRequirementViolated);

  is_valid &=
      ValidateAndLog(country.requires_state(), {ADDRESS_HOME_STATE},
                     AddressImportRequirement::kStateRequirementFulfilled,
                     AddressImportRequirement::kStateRequirementViolated);

  is_valid &= ValidateAndLog(country.requires_zip(), {ADDRESS_HOME_ZIP},
                             AddressImportRequirement::kZipRequirementFulfilled,
                             AddressImportRequirement::kZipRequirementViolated);

  is_valid &= ValidateAndLog(
      country.requires_zip_or_state(), {ADDRESS_HOME_ZIP, ADDRESS_HOME_STATE},
      AddressImportRequirement::kZipOrStateRequirementFulfilled,
      AddressImportRequirement::kZipOrStateRequirementViolated);

  is_valid &= ValidateAndLog(
      country.requires_line1_or_house_number(),
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_HOUSE_NUMBER},
      AddressImportRequirement::kLine1OrHouseNumberRequirementFulfilled,
      AddressImportRequirement::kLine1OrHouseNumberRequirementViolated);

  // TODO(crbug.com/1413205): Merge this into is_minimum_address.
  if (is_valid && country.requires_full_name()) {
    ValidateAndLog(/*required=*/true, {NAME_FULL},
                   AddressImportRequirement::kNameRequirementFulfilled,
                   AddressImportRequirement::kNameRequirementViolated);
  }

  return base::flat_set<AddressImportRequirement>(
      std::move(address_import_requirements));
}

bool IsMinimumAddress(const AutofillProfile& profile) {
  const std::vector<std::string>& country_codes =
      autofill::CountryDataMap::GetInstance()->country_codes();
  if (!base::Contains(country_codes, base::UTF16ToUTF8(profile.GetRawInfo(
                                         ADDRESS_HOME_COUNTRY)))) {
    return false;
  }
  base::flat_set<AddressImportRequirement> address_import_requirements =
      GetAutofillProfileRequirementResult(profile,
                                          /*import_log_buffer=*/nullptr);
  return !base::ranges::any_of(
      kMinimumAddressRequirementViolations,
      [&](AddressImportRequirement address_requirement_violation) {
        return address_import_requirements.contains(
            address_requirement_violation);
      });
}

bool IsEligibleForMigrationToAccount(
    const PersonalDataManager& personal_data_manager,
    const AutofillProfile& profile) {
  return personal_data_manager.IsEligibleForAddressAccountStorage() &&
         !personal_data_manager.IsProfileMigrationBlocked(profile.guid()) &&
         personal_data_manager.IsCountryEligibleForAccountStorage(
             base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)));
}

}  // namespace autofill
