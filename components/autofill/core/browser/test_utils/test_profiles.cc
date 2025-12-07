// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/test_profiles.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace test {

void SetProfileTestValues(AutofillProfile* profile,
                          const std::vector<ProfileTestData>& profile_test_data,
                          bool finalize) {
  DCHECK(profile);

  std::ranges::for_each(
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
  std::ranges::for_each(test_data, [&](const ProfileTestData& entry) {
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

AutofillProfile HiraganaProfile() {
  AutofillProfile profile(AddressCountryCode("JP"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "山田 太郎", VerificationStatus::kObserved},
      {ALTERNATIVE_FULL_NAME, "やまだ たろう", VerificationStatus::kObserved}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile ExtendedHiraganaProfile() {
  AutofillProfile profile = HiraganaProfile();
  // Add the address-related fields.
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {ADDRESS_HOME_STREET_ADDRESS, "123 Mainstreet",
       VerificationStatus::kObserved},
      {ADDRESS_HOME_ZIP, "98765", VerificationStatus::kObserved}};
  SetProfileTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile KatakanaProfile1() {
  AutofillProfile profile(AddressCountryCode("JP"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "山田 太郎", VerificationStatus::kObserved},
      {ALTERNATIVE_FULL_NAME, "ヤマダ タロウ", VerificationStatus::kObserved}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile KatakanaProfile2() {
  AutofillProfile profile(AddressCountryCode("JP"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "ネオ アンダーソン"},
      {ALTERNATIVE_FULL_NAME, "ネオ アンダーソン"}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile AccountNameEmailProfile() {
  AccountInfo info;
  info.full_name = "George Washington";
  info.email = "george.washington@gmail.com";
  return AutofillProfile{info};
}

AutofillProfile AccountNameEmailProfileSuperset() {
  AutofillProfile profile(AutofillProfile::RecordType::kAccount,
                          AddressCountryCode("US"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {NAME_FULL, "George Washington"},
      {EMAIL_ADDRESS, "george.washington@gmail.com"},
      {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue"},
      {ADDRESS_HOME_STATE, "CA"},
      {ADDRESS_HOME_ZIP, "99666"},
      {ADDRESS_HOME_CITY, "Los Angeles"}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile OnlyAddressProfile(AutofillProfile::RecordType record_type) {
  AutofillProfile profile(record_type, AddressCountryCode("US"));
  const std::vector<ProfileTestData> observed_profile_test_data = {
      {ADDRESS_HOME_STREET_ADDRESS, "123 Mainstreet"},
      {ADDRESS_HOME_STATE, "CA"},
      {ADDRESS_HOME_ZIP, "12345"},
      {ADDRESS_HOME_CITY, "San Francisco"}};
  SetProfileObservedTestValues(&profile, observed_profile_test_data);
  return profile;
}

AutofillProfile SupersetProfileOf(base::span<const AutofillProfile> profiles,
                                  std::string_view app_locale,
                                  AutofillProfile::RecordType type,
                                  AddressCountryCode country_code) {
  AutofillProfileComparator comparator{app_locale};
  AutofillProfile new_profile{type, country_code};
  for (const AutofillProfile& profile : profiles) {
    CHECK(comparator.AreMergeable(new_profile, profile));
    new_profile.MergeDataFrom(profile, app_locale.data());
  }
  return new_profile;
}

}  // namespace test

}  // namespace autofill
