// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace test {

void SetProfileTestValues(AutofillProfile* profile,
                          const std::vector<ProfileTestData>& profile_test_data,
                          bool finalize) {
  DCHECK(profile);

  base::ranges::for_each(
      profile_test_data, [&](const ProfileTestData& test_data) {
        profile->SetRawInfoWithVerificationStatus(
            test_data.field_type, base::UTF8ToUTF16(test_data.value),
            test_data.verification_status);
      });

  if (finalize) {
    profile->FinalizeAfterImport();
  }
}

void CopyGUID(const AutofillProfile& from, AutofillProfile* to) {
  to->set_guid(from.guid());
}

void SetProfileObservedTestValues(AutofillProfile* profile,
                                  const std::vector<ProfileTestData>& test_data,
                                  bool finalize) {
  // Make a copy of the test data with all verification statuses replaced with
  // 'kObserved'.
  std::vector<ProfileTestData> observed_test_data;
  base::ranges::for_each(test_data, [&](const ProfileTestData& entry) {
    observed_test_data.emplace_back(ProfileTestData{
        entry.field_type, entry.value, VerificationStatus::kObserved});
  });

  // Set this data to the profile.
  SetProfileTestValues(profile, observed_test_data, finalize);
}

AutofillProfile StandardProfile(AddressCountryCode country_code) {
  AutofillProfile profile(country_code);
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "Pablo Diego de la Ruiz y Picasso",
       VerificationStatus::kUserVerified},
      {ADDRESS_HOME_STREET_ADDRESS, "123 Mainstreet",
       VerificationStatus::kObserved},
      {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
      {ADDRESS_HOME_ZIP, "98765", VerificationStatus::kObserved},
      {ADDRESS_HOME_CITY, "Mountainview", VerificationStatus::kObserved}};
  SetProfileTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile UpdateableStandardProfile() {
  AutofillProfile profile(AddressCountryCode("US"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "Pablo Diego de la Ruiz y Picasso",
       VerificationStatus::kObserved},
      {ADDRESS_HOME_STREET_ADDRESS, "123 Mainstreet",
       VerificationStatus::kObserved},
      {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
      {ADDRESS_HOME_ZIP, "98765", VerificationStatus::kObserved},
      {ADDRESS_HOME_CITY, "Mountainview", VerificationStatus::kObserved}};
  SetProfileTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile SubsetOfStandardProfile() {
  AutofillProfile profile(AddressCountryCode("US"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "Pablo Diego de la Ruiz y Picasso"},
      {ADDRESS_HOME_STREET_ADDRESS, "123 Mainstreet"},
      {ADDRESS_HOME_STATE, "CA"},
      {ADDRESS_HOME_ZIP, ""},
      {ADDRESS_HOME_CITY, ""}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile DifferentFromStandardProfile() {
  AutofillProfile profile(AddressCountryCode("US"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "Neo Anderson"},
      {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue"},
      {ADDRESS_HOME_STATE, "CA"},
      {ADDRESS_HOME_ZIP, "99666"},
      {ADDRESS_HOME_CITY, "Los Angeles"}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

}  // namespace test

}  // namespace autofill
