// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using autofill_metrics::SettingsVisibleFieldTypeForMetrics;
using ::testing::UnorderedPointwise;

// Test that two AutofillProfiles have the same `record_type() and `Compare()`
// equal. This is useful for testing profile migration, which changes a
// profile's record_type and GUID (preventing the use of operator==).
MATCHER(CompareWithRecordType, "") {
  const AutofillProfile& a = std::get<0>(arg);
  const AutofillProfile& b = std::get<1>(arg);
  return a.record_type() == b.record_type() && a.Compare(b) == 0;
}

class AutofillProfileImportProcessTest : public testing::Test {
 protected:
  void BlockProfileForUpdates(const AutofillProfile& profile) {
    while (!address_data_manager().IsProfileUpdateBlocked(profile.guid())) {
      address_data_manager().AddStrikeToBlockProfileUpdate(profile.guid());
    }
  }

  void BlockDomainForNewProfiles(GURL url) {
    while (!address_data_manager().IsNewProfileImportBlockedForDomain(url)) {
      address_data_manager().AddStrikeToBlockNewProfileImportForDomain(url);
    }
  }

  // Returns all profiles stored in the `address_data_manager_` after
  // finalizing the `import_process`'s import.
  std::vector<AutofillProfile> ApplyImportAndGetProfiles(
      ProfileImportProcess& import_process) {
    import_process.ApplyImport();
    // For convenience, return plain objects rather than pointers.
    std::vector<AutofillProfile> profiles;
    for (const AutofillProfile* adm_profile :
         address_data_manager().GetProfiles()) {
      profiles.push_back(*adm_profile);
    }
    return profiles;
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  TestAddressDataManager& address_data_manager() {
    return address_data_manager_;
  }

  GURL url_{"https://www.import.me/now.html"};

  ukm::SourceId ukm_source_id() const { return 123; }

  ProfileImportProcess CreateProfileImportProcess(
      const AutofillProfile& profile,
      bool allow_only_silent_updates,
      ProfileImportMetadata metadata = {}) {
    return ProfileImportProcess(profile, "en_US", url_, ukm_source_id(),
                                &address_data_manager(),
                                allow_only_silent_updates, metadata);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillSupportPhoneticNameForJP};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAddressDataManager address_data_manager_;
};

// Tests the import process for the scenario, that the user accepts the import
// of their first profile.
TEST_F(AutofillProfileImportProcessTest, ImportFirstProfile_UserAccepts) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Advance the test clock to make sure that the modification date of the new
  // profile gets updated.
  AdvanceClock(base::Days(1));

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Simulate the acceptance of the save prompt.
  import_data.AcceptWithoutEdits();

  // This operation should result in a profile change, and the type of the
  // import corresponds to the creation of a new profile.
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 1U);
  EXPECT_THAT(resulting_profiles,
              testing::UnorderedElementsAre(observed_profile));
  EXPECT_EQ(resulting_profiles.at(0).usage_history().modification_date(),
            base::Time::Now());
}

// Tests the import process for the scenario, that the import of a new profile
// is blocked.
TEST_F(AutofillProfileImportProcessTest, ImportFirstProfile_ImportIsBlocked) {
  AutofillProfile observed_profile = test::StandardProfile();

  BlockDomainForNewProfiles(url_);

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // The user is not asked.
  import_data.AcceptWithoutPrompt();

  // This operation should not result in a profile change.
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSuppressedNewProfile);

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre());
}

// Tests the import process for the scenario, that the user accepts the import
// of their first profile but with additional edits..
TEST_F(AutofillProfileImportProcessTest,
       ImportFirstProfile_UserAcceptsWithEdits) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Simulate that the user accepts the save prompt but only after editing the
  // profile. Note, that the `guid` of the edited profile must match the `guid`
  // of the initial import candidate.
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();
  test::CopyGUID(observed_profile, &edited_profile);
  import_data.AcceptWithEdits(edited_profile);

  // This operation should result in a profile change, and the type of the
  // import corresponds to the creation of a new profile.
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(edited_profile));
}

// Tests the import process for the scenario, that the user declines the import
// of their first profile.
TEST_F(AutofillProfileImportProcessTest, ImportFirstProfile_UserRejects) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Simulate the decline of the user.
  import_data.Declined();

  // Since the user declined, there should be no change to the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());
  // The type of import was nevertheless corresponds to the creation of a new
  // profile.
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre());
}

// Tests the import of a profile that is an exact duplicate of the only already
// existing profile.
TEST_F(AutofillProfileImportProcessTest, ImportDuplicateProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  address_data_manager().AddProfile(observed_profile);

  // Create the import process for the scenario that the observed profile is an
  // exact copy of an already existing one.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the import of a duplicate is determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);

  // In this scenario, the user should not be queried and the process is
  // silently accepted.
  import_data.AcceptWithoutPrompt();

  // There should be no change to the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(observed_profile));
}

// Tests that an incorrectly complemented country doesn't lead to an almost-
// duplicate profile import.
// Regression test for crbug.com/1376937.
TEST_F(AutofillProfileImportProcessTest, IncorrectlyComplementedCountry) {
  AutofillProfile profile = test::StandardProfile(AddressCountryCode("ES"));
  EXPECT_EQ(u"ES", profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  address_data_manager().AddProfile(profile);

  // Suppose the country was incorrectly complemented to "ZA". Note that the
  // selected country must share the same address model with the existing
  // profile to be considered mergeable.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"ZA");

  // Test that the import is correctly classified as a duplicate.
  ProfileImportMetadata metadata;
  metadata.did_complement_country = true;
  ProfileImportProcess import_data(
      profile, "en_US", url_, ukm_source_id(), &address_data_manager(),
      /*allow_only_silent_updates=*/false, metadata);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
}

// Tests the import of a profile that is an exact duplicate of an already
// existing profile along with other profiles that are not mergeable or
// updateable with the observed profile.
TEST_F(AutofillProfileImportProcessTest,
       ImportDuplicateProfile_OutOfMultipleProfiles) {
  AutofillProfile observed_profile = test::StandardProfile();
  // This already existing profile is an exact duplicate of the observed one.
  AutofillProfile duplicate_existing_profile = observed_profile;
  // This already existing profile is neither mergeable nor updateable with the
  // observed one.
  AutofillProfile distinct_existing_profile =
      test::DifferentFromStandardProfile();

  address_data_manager().AddProfile(duplicate_existing_profile);
  address_data_manager().AddProfile(distinct_existing_profile);

  // Create the import process for the two already existing profiles.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);

  // In this scenario, the user should not be queried and the process is
  // silently accepted.
  import_data.AcceptWithoutPrompt();

  // Verify that this operation does not result in a change of the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(duplicate_existing_profile,
                                            distinct_existing_profile));
}

// Tests that importing a profile that is an exact duplicate of a kAccount
// profile is rejected as a duplicate.
TEST_F(AutofillProfileImportProcessTest, ImportDuplicateProfile_kAccount) {
  AutofillProfile account_profile = test::StandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                   ukm_source_id(), &address_data_manager(),
                                   /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
  import_data.AcceptWithoutPrompt();
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(account_profile));
}

// Tests that importing a profile that is a subset of a kAccount profile is
// rejected as a duplicate.
TEST_F(AutofillProfileImportProcessTest, ImportSubsetProfile_kAccount) {
  AutofillProfile account_profile = test::StandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  ProfileImportProcess import_data(
      test::SubsetOfStandardProfile(), "en_US", url_, ukm_source_id(),
      &address_data_manager(), /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
  import_data.AcceptWithoutPrompt();
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(account_profile));
}

// Tests that importing an `ExtendedHiraganaProfile` with address fields is a
// superset of a `HiraganaProfile` kAccount profile results in an update and
// not an upload. The record type of the resulting profile remains kAccount.
TEST_F(AutofillProfileImportProcessTest,
       ImportJapaneseSupersetProfile_kAccount_PostStorage) {
  AutofillProfile account_profile = test::HiraganaProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  ProfileImportProcess import_data(test::ExtendedHiraganaProfile(), "ja_JP",
                                   url_, ukm_source_id(),
                                   &address_data_manager(),
                                   /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  import_data.AcceptWithoutPrompt();
  EXPECT_TRUE(import_data.ProfilesChanged());
  AutofillProfile expected_profile = test::ExtendedHiraganaProfile();
  expected_profile.set_guid(account_profile.guid());
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(expected_profile));
}

// Tests that importing a profile that is semantically equal to a kAccount
// profile is rejected as a duplicate.
TEST_F(AutofillProfileImportProcessTest,
       ImportDifferentCharacterProfile_kAccount) {
  AutofillProfile account_profile = test::HiraganaProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  // Import a profile with equivalent data but saved in a different alphabet.
  ProfileImportProcess import_data(test::KatakanaProfile1(), "ja_JP", url_,
                                   ukm_source_id(), &address_data_manager(),
                                   /*allow_only_silent_updates=*/false);
  // The import is rejected as a duplicate.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
  import_data.AcceptWithoutPrompt();
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(account_profile));
}

// Tests that importing a Katakana profile with a different alternative name
// than the existing Hiragana profile results in a new profile creation and user
// is offered to save it.
TEST_F(AutofillProfileImportProcessTest, ImportNewKatakanaProfile_UserAccepts) {
  AutofillProfile existing_hiragana_profile = test::HiraganaProfile();
  test_api(existing_hiragana_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(existing_hiragana_profile);

  AutofillProfile new_katakana_profile = test::KatakanaProfile2();

  // Advance the test clock to make sure that the modification date of the new
  // profile gets updated.
  AdvanceClock(base::Days(1));

  ProfileImportProcess import_data =
      CreateProfileImportProcess(new_katakana_profile,
                                 /*allow_only_silent_updates=*/false);
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 2U);
  EXPECT_THAT(resulting_profiles,
              testing::UnorderedElementsAre(existing_hiragana_profile,
                                            new_katakana_profile));
  EXPECT_EQ(resulting_profiles.at(1).usage_history().modification_date(),
            base::Time::Now());
}

// Tests that importing a profile that is a superset of a kAccount profile
// results in an update. The record type of resulting profile remains kAccount.
TEST_F(AutofillProfileImportProcessTest,
       ImportSupersetProfile_kAccount_PostStorage) {
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                   ukm_source_id(), &address_data_manager(),
                                   /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  import_data.AcceptWithoutPrompt();
  EXPECT_TRUE(import_data.ProfilesChanged());
  AutofillProfile expected_profile = test::StandardProfile();
  expected_profile.set_guid(account_profile.guid());
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(expected_profile));
}

// Tests that an import can cause a silent update of a `kAccount` profile.
TEST_F(AutofillProfileImportProcessTest, ImportSilentUpdate_kAccount) {
  AutofillProfile account_profile = test::UpdateableStandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(account_profile);

  // The `observed_profile` is of type `kLocalOrSyncable`. This should not
  // prevent silent-updating a `kAccount` profile.
  ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                   ukm_source_id(), &address_data_manager(),
                                   /*allow_only_silent_updates=*/true);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdate);
  import_data.AcceptWithoutPrompt();
  EXPECT_TRUE(import_data.ProfilesChanged());
  // Expect that the existing profiles was updated to the standard profile,
  // while maintaining it's `kAccount` status.
  AutofillProfile expected_profile = test::StandardProfile();
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  expected_profile.set_guid(account_profile.guid());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(expected_profile));
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile.
TEST_F(AutofillProfileImportProcessTest, MergeWithExistingProfile_Accepted) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  mergeable_profile.usage_history().set_modification_date(base::Time::Now());
  AdvanceClock(base::Days(1));

  address_data_manager().AddProfile(mergeable_profile);

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);

  // There should be a merge candidate that is the existing profile.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();

  // And verify that this correctly translates to a change of the stored
  // profiles.
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Explicitly check the content of the stored profiles. The final profile
  // should have the same content as the observed profile, but the `guid` of the
  // `mergeable_profile`.
  AutofillProfile final_profile = test::StandardProfile();
  test::CopyGUID(mergeable_profile, &final_profile);

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 1U);
  EXPECT_THAT(resulting_profiles, testing::UnorderedElementsAre(final_profile));
  EXPECT_EQ(resulting_profiles.at(0).usage_history().modification_date(),
            base::Time::Now());
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile for the scenario that the user introduced additional edits.
TEST_F(AutofillProfileImportProcessTest,
       MergeWithExistingProfile_AcceptWithEdits) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  mergeable_profile.usage_history().set_modification_date(base::Time::Now());
  AdvanceClock(base::Days(1));

  address_data_manager().AddProfile(mergeable_profile);

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be merge candidate that is the existing profile.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);

  // Simulate that the user accepts this import with additional edits.
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();
  // Note that it is necessary to maintain the `guid` of the initial import
  // candidate.
  test::CopyGUID(mergeable_profile, &edited_profile);
  import_data.AcceptWithEdits(edited_profile);

  // This should result in a change of stored profiles.
  EXPECT_TRUE(import_data.ProfilesChanged());

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 1U);
  EXPECT_THAT(resulting_profiles,
              testing::UnorderedElementsAre(edited_profile));
  EXPECT_EQ(resulting_profiles.at(0).usage_history().modification_date(),
            base::Time::Now());
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile for the scenario that there are multiple profiles stored.
TEST_F(AutofillProfileImportProcessTest,
       MergeWithExistingProfile_MultipleStoredProfiles_Accepted) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  // This is just another completely different profile.
  AutofillProfile distinct_profile = test::DifferentFromStandardProfile();

  address_data_manager().AddProfile(mergeable_profile);
  address_data_manager().AddProfile(distinct_profile);

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be merge candidate that is the existing profile.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);

  // Simulate that the user accepts the operation without further edits.
  import_data.AcceptWithoutEdits();

  // This should result in the change of at least on profile.
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Test that the user decision translates correctly to the expected end
  // result.
  AutofillProfile merged_profile = test::StandardProfile();
  test::CopyGUID(mergeable_profile, &merged_profile);

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(merged_profile, distinct_profile));
}

// Tests the rejection of the merge of the observed profile with an already
// existing one.
TEST_F(AutofillProfileImportProcessTest, MergeWithExistingProfile_Rejected) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  // Since the merge is not accepted, the `modification_date` should not be
  // changed.
  mergeable_profile.usage_history().set_modification_date(base::Time::Now());
  const base::Time earlier_time = base::Time::Now();
  AdvanceClock(base::Days(1));

  address_data_manager().AddProfile(mergeable_profile);

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be merge candidate that is the existing profile.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // But there should be no further updates profiles.
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 0u);

  // Simulate the decline by the user.
  import_data.Declined();

  // Since there are no additional updates, this should result in no overall
  // changes.
  EXPECT_FALSE(import_data.ProfilesChanged());

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 1U);
  EXPECT_THAT(resulting_profiles,
              testing::UnorderedElementsAre(mergeable_profile));
  EXPECT_EQ(resulting_profiles.at(0).usage_history().modification_date(),
            earlier_time);
}

// Tests the scenario in which the observed profile results in a silent update
// of the only already existing profile.
TEST_F(AutofillProfileImportProcessTest, SilentlyUpdateProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();

  address_data_manager().AddProfile(updateable_profile);

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdate);
  // There should be no merge candidate since this is only a silent update.
  EXPECT_FALSE(import_data.merge_candidate().has_value());
  // But there should be one updated profiles.
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 1u);

  // In this scenario, the user should not be prompted.
  import_data.AcceptWithoutPrompt();

  // The operation should result in a change of the profiles
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Test that the existing profile was correctly updated.
  AutofillProfile updated_profile = test::StandardProfile();
  updated_profile.set_guid(updateable_profile.guid());

  std::vector<AutofillProfile> resulting_profiles =
      ApplyImportAndGetProfiles(import_data);
  ASSERT_EQ(resulting_profiles.size(), 1U);
  EXPECT_THAT(resulting_profiles,
              testing::UnorderedElementsAre(updated_profile));
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile for which updates are blocked.
TEST_F(AutofillProfileImportProcessTest, BlockedMerge) {
  AutofillProfile observed_profile = test::StandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  BlockProfileForUpdates(mergeable_profile);
  address_data_manager().AddProfile(mergeable_profile);

  // Create the import process with a mergeable profile.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSuppressedConfirmableMerge);

  // There should be no merge candidate because the only potential candidate is
  // blocked and also no silent update.
  EXPECT_FALSE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 0u);

  // The user should not be asked.
  import_data.AcceptWithoutPrompt();

  EXPECT_FALSE(import_data.ProfilesChanged());

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(mergeable_profile));
}

// Tests the scenario in which the observed profile is not imported since the
// import process only silent updates.
TEST_F(AutofillProfileImportProcessTest, SilentlyUpdateProfile_WithNewProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/true);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSuppressedNewProfile);
  // There should be no merge candidate since this is only a silent update.
  EXPECT_FALSE(import_data.merge_candidate().has_value());
  // But there should be one updated profiles.
  EXPECT_TRUE(import_data.silently_updated_profiles().empty());

  // In this scenario, the user should not be prompted.
  import_data.AcceptWithoutPrompt();

  // The operation should result in a change of the profiles
  EXPECT_FALSE(import_data.ProfilesChanged());
}

// Tests the scenario in which an observed profile cannot be merged with an
// existing profile while another already existing profile can be silently
// updated since the import process allows for silent update only
TEST_F(AutofillProfileImportProcessTest,
       SilentlyUpdateProfile_NoMergeOnlySilentUpdate) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  address_data_manager().AddProfile(updateable_profile);
  address_data_manager().AddProfile(mergeable_profile);

  // Create the import process with a mergeable and an updateable profile..
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/true);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(
      import_data.import_type(),
      AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate);
  // There should be no merge candidate because the only potential candidate is
  // blocked but there should be a silent update.
  EXPECT_FALSE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 1u);

  // The user should not be asked.
  import_data.AcceptWithoutPrompt();

  // The silent update should be performed unconditionally. Therefore, there
  // should be a change to the stored profiles nevertheless.
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile updated_profile = observed_profile;
  test::CopyGUID(updateable_profile, &updated_profile);

  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedElementsAre(mergeable_profile, updated_profile));
}

// Tests that for eligible users, new profiles are of record type kAccount.
TEST_F(AutofillProfileImportProcessTest, NewProfileRecordType) {
  // Ineligible user.
  {
    address_data_manager().SetIsEligibleForAddressAccountStorage(false);
    ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                     ukm_source_id(), &address_data_manager(),
                                     /*allow_only_silent_updates=*/false);
    EXPECT_EQ(import_data.import_candidate()->record_type(),
              AutofillProfile::RecordType::kLocalOrSyncable);
  }

  // Eligible user.
  {
    address_data_manager().SetIsEligibleForAddressAccountStorage(true);
    ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                     ukm_source_id(), &address_data_manager(),
                                     /*allow_only_silent_updates=*/false);
    EXPECT_EQ(import_data.import_candidate()->record_type(),
              AutofillProfile::RecordType::kAccount);
  }
}

// Two `kLocalOrSyncable` profiles are stored. One of them is observed during
// submission. Expected that this profile is offered for migration.
// After accepting, expect that the profile's record type has changed and that
// the second profile is left unaltered.
TEST_F(AutofillProfileImportProcessTest, MigrateProfileToAccount) {
  const AutofillProfile profile_to_migrate = test::StandardProfile();
  const AutofillProfile other_profile = test::DifferentFromStandardProfile();
  address_data_manager().AddProfile(profile_to_migrate);
  address_data_manager().AddProfile(other_profile);
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  auto import_data = CreateProfileImportProcess(
      profile_to_migrate, /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kProfileMigration);
  EXPECT_EQ(import_data.import_candidate(), profile_to_migrate);

  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(
          CompareWithRecordType(),
          {profile_to_migrate.ConvertToAccountProfile(), other_profile}));
}

// Test that the profile to migrate can be silently updated. Expect that after
// accepting the migration, the stored profile has record type `kAccount` and
// was silently updated.
TEST_F(AutofillProfileImportProcessTest, MigrateProfileToAccount_SilentUpdate) {
  const AutofillProfile profile_to_migrate = test::UpdateableStandardProfile();
  const AutofillProfile observed_profile = test::StandardProfile();
  address_data_manager().AddProfile(profile_to_migrate);
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kProfileMigrationAndSilentUpdate);
  // The import candidate should be the existing profile (`profile_to_migrate`),
  // silently updated with the `observed_profile`. This is effectively the
  // `observed_profile` with a different GUID.
  AutofillProfile expected_import_candidate = observed_profile;
  expected_import_candidate.set_guid(profile_to_migrate.guid());
  EXPECT_EQ(import_data.import_candidate(), expected_import_candidate);

  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedPointwise(
                  CompareWithRecordType(),
                  {observed_profile.ConvertToAccountProfile()}));
}

// Even if a profile migration is rejected, silent updates are applied.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_SilentUpdate_Decline) {
  const AutofillProfile migration_candidate = test::UpdateableStandardProfile();
  const AutofillProfile observed_profile = test::StandardProfile();
  address_data_manager().AddProfile(migration_candidate);
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kProfileMigrationAndSilentUpdate);
  import_data.Declined();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(CompareWithRecordType(), {observed_profile}));
}

// Expect that no migration is offered for ineligible users.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_IneligibleUser) {
  const AutofillProfile profile = test::StandardProfile();
  address_data_manager().AddProfile(profile);
  address_data_manager().SetIsEligibleForAddressAccountStorage(false);

  auto import_data =
      CreateProfileImportProcess(profile, /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
}

// Expect that no migration is offered for ineligible profiles.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_IneligibleProfile) {
  AutofillProfile profile = test::StandardProfile();
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"KP");
  address_data_manager().AddProfile(profile);

  auto import_data =
      CreateProfileImportProcess(profile, /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
}

// Tests that importing a superset of Home & Work profile results in an update
// prompt which in fact adds a new profile with `kAccount` type.
TEST_F(AutofillProfileImportProcessTest, ImportingHomeAndWorkProfileSuperset) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile home_profile = test::SubsetOfStandardProfile();
  test_api(home_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  address_data_manager().AddProfile(home_profile);

  // Create the import process for the scenario the `observed_profile` is a
  // superset of the existing `home_profile`.
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kHomeAndWorkSuperset);

  // There should be a merge candidate that is the `home_profile`.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), home_profile);

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Confirm that only the superset profile exists.
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedPointwise(
                  CompareWithRecordType(),
                  {observed_profile.ConvertToAccountProfile()}));
}

// Tests that when importing a superset of Home & Work profile, metrics are
// correctly emitted.
TEST_F(AutofillProfileImportProcessTest,
       ImportingHomeAndWorkProfileSuperset_Metrics) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile home_profile = test::SubsetOfStandardProfile();
  test_api(home_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  address_data_manager().AddProfile(home_profile);
  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Simulate that the user accepts this import with edits.
  AutofillProfile edited_profile = *import_data.import_candidate();
  edited_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"updated city", VerificationStatus::kUserVerified);
  import_data.AcceptWithEdits(edited_profile);

  base::HistogramTester histogram_tester;
  import_data.CollectMetrics(/*ukm_recorder=*/nullptr,
                             address_data_manager().GetProfiles());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.ProfileImportType",
      AutofillProfileImportType::kHomeAndWorkSuperset, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.HomeAndWorkSupersetProfileDecision",
      AutofillClient::AddressPromptUserDecision::kEditAccepted, 1);
  // `observed_profile` has city and zip, both of which `home_profile` is
  // lacking. This caused the home/work superset prompt.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ProfileImport.HomeAndWorkSupersetAffectedType"),
              base::BucketsAre(
                  base::Bucket(SettingsVisibleFieldTypeForMetrics::kCity, 1),
                  base::Bucket(SettingsVisibleFieldTypeForMetrics::kZip, 1)));
  // The user manually edited the value of the city field.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.HomeAndWorkSupersetEditedType",
      SettingsVisibleFieldTypeForMetrics::kCity, 1);
}

// Tests that an accepted import of a `kAccountNameEmail` superset profile by an
// address account storage eligible user, results in an addition of a new
// `kAccount` profile.
TEST_F(AutofillProfileImportProcessTest,
       ImportingAccountNameEmailProfileSuperset_AddressAccountStorageEligible) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile observed_profile =
      test::AccountNameEmailProfileSuperset();

  address_data_manager().AddProfile(account_name_email_profile);

  // Create the import process for the scenario where the `observed_profile` is
  // a superset of the existing `account_name_email_profile`.
  ProfileImportProcess import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kNameEmailSuperset);

  // The merge candidate should be set to std::nullopt
  ASSERT_FALSE(import_data.merge_candidate().has_value());

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile expected_profile = test::AccountNameEmailProfileSuperset();
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);

  // Confirm that only the superset profile exists.
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(CompareWithRecordType(), {expected_profile}));
}

// Tests that an accepted import of a `kAccountNameEmail` superset profile by an
// address account storage ineligible user, results in an addition of a new
// `kLocalOrSyncable` profile.
TEST_F(
    AutofillProfileImportProcessTest,
    ImportingAccountNameEmailProfileSuperset_AddressAccountStorageIneligible) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(false);

  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile observed_profile =
      test::AccountNameEmailProfileSuperset();

  address_data_manager().AddProfile(account_name_email_profile);

  // Create the import process for the scenario the where `observed_profile` is
  // a superset of the existing `account_name_email_profile`.
  ProfileImportProcess import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kNameEmailSuperset);

  // The merge candidate should be set to std::nullopt
  ASSERT_FALSE(import_data.merge_candidate().has_value());

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile expected_profile = test::AccountNameEmailProfileSuperset();
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);

  // Confirm that only the superset profile exists.
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(CompareWithRecordType(), {expected_profile}));
}

// Tests that when importing a superset of the `kAccountNameEmail` profile,
// metrics are correctly emitted.
TEST_F(AutofillProfileImportProcessTest,
       ImportingAccountNameEmailSupersetProfile_Metrics) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);
  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile observed_profile =
      test::AccountNameEmailProfileSuperset();

  address_data_manager().AddProfile(account_name_email_profile);

  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false);

  // Simulate that the user accepts this import with edits.
  AutofillProfile edited_profile = *import_data.import_candidate();
  edited_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"Updated Name Full", VerificationStatus::kUserVerified);
  import_data.AcceptWithEdits(edited_profile);

  base::HistogramTester histogram_tester;
  import_data.CollectMetrics(/*ukm_recorder=*/nullptr,
                             address_data_manager().GetProfiles());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.NameEmailSupersetEditedType",
      SettingsVisibleFieldTypeForMetrics::kName, 1);
}

// Tests that an accepted import of a profile that is a superset of both the
// `kAccountNameEmail` profile and the `kAccountWork` profile results in a
// creation of a new superset profile.
TEST_F(AutofillProfileImportProcessTest, NameEmail_Work_SupersetImport) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile work_profile =
      test::OnlyAddressProfile(AutofillProfile::RecordType::kAccountWork);
  const AutofillProfile observed_profile =
      test::SupersetProfileOf({account_name_email_profile, work_profile},
                              address_data_manager().app_locale(),
                              AutofillProfile::RecordType::kLocalOrSyncable);

  address_data_manager().AddProfile(account_name_email_profile);
  address_data_manager().AddProfile(work_profile);

  // Insert guids into `unedited_autofilled_profile_guids` to trigger
  // `kAccountNameEmail` H/W merge flow.
  ProfileImportMetadata metadata;
  metadata.unedited_autofilled_profile_guids.insert(
      account_name_email_profile.guid());
  metadata.unedited_autofilled_profile_guids.insert(work_profile.guid());

  ProfileImportProcess import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false, metadata);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kHomeWorkNameEmailMerge);

  // The merge candidate should be set to std::nullopt
  ASSERT_EQ(import_data.merge_candidate(), std::nullopt);

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Create an additional profile that is expected to exists after the import is
  // applied.
  AutofillProfile expected_profile = observed_profile;
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);

  // Confirm that only the superset profile exists.
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              UnorderedPointwise(CompareWithRecordType(), {expected_profile}));
}

// Tests that an accepted import of a profile that is a superset of both the
// `kAccountNameEmail` profile and the `kAccountHome` profile results in a
// creation of a new superset profile.
TEST_F(AutofillProfileImportProcessTest, NameEmail_Home_SupersetImport) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile home_profile =
      test::OnlyAddressProfile(AutofillProfile::RecordType::kAccountHome);
  const AutofillProfile observed_profile =
      test::SupersetProfileOf({account_name_email_profile, home_profile},
                              address_data_manager().app_locale(),
                              AutofillProfile::RecordType::kLocalOrSyncable);

  address_data_manager().AddProfile(home_profile);
  address_data_manager().AddProfile(account_name_email_profile);

  // Insert guids into `unedited_autofilled_profile_guids` to trigger
  // `kAccountNameEmail` H/W merge flow.
  ProfileImportMetadata metadata;
  metadata.unedited_autofilled_profile_guids.insert(
      account_name_email_profile.guid());
  metadata.unedited_autofilled_profile_guids.insert(home_profile.guid());

  ProfileImportProcess import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false, metadata);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kHomeWorkNameEmailMerge);

  // The merge candidate should be set to std::nullopt
  ASSERT_EQ(import_data.merge_candidate(), std::nullopt);

  // Simulate that the user accepts this import without edits.
  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Create an additional profile that is expected to exists after the import is
  // applied.
  AutofillProfile expected_profile = observed_profile;
  test_api(expected_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);

  // Confirm that only the superset profile exists.
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              UnorderedPointwise(CompareWithRecordType(), {expected_profile}));
}

// Tests that an accepted import of a profile that is a superset of both
// `kAccountNameEmail` and `kAccountHome` doesn't result in
// `kHomeWorkNameEmailMerge` import if unedited_autofilled_profile_guids` of
// `ProfileImportMetadata` contains more than 2 profiles or a profile of the
// wrong type.
TEST_F(AutofillProfileImportProcessTest,
       NameEmail_HW_SupersetImport_IncorrectGuidMetadata) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);

  const AutofillProfile standard_profile = test::StandardProfile();
  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile home_profile =
      test::OnlyAddressProfile(AutofillProfile::RecordType::kAccountHome);
  const AutofillProfile observed_profile =
      test::SupersetProfileOf({account_name_email_profile, home_profile},
                              address_data_manager().app_locale(),
                              AutofillProfile::RecordType::kLocalOrSyncable);

  address_data_manager().AddProfile(home_profile);
  address_data_manager().AddProfile(account_name_email_profile);
  address_data_manager().AddProfile(standard_profile);
  ProfileImportMetadata metadata;

  // Wrong number of profiles
  metadata.unedited_autofilled_profile_guids.insert(
      account_name_email_profile.guid());
  ProfileImportProcess import_data_1 = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false, metadata);
  EXPECT_NE(import_data_1.import_type(),
            AutofillProfileImportType::kHomeWorkNameEmailMerge);

  // Contains profile of an incorrect type - `kAccount`
  metadata.unedited_autofilled_profile_guids.insert(standard_profile.guid());
  ProfileImportProcess import_data_2 = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false, metadata);
  EXPECT_NE(import_data_2.import_type(),
            AutofillProfileImportType::kHomeWorkNameEmailMerge);
}

// Tests that when importing a superset of the `kAccountNameEmail` profile and
// one of the H/W profiles, metrics are correctly emitted.
TEST_F(AutofillProfileImportProcessTest,
       ImportingHomeWorkNameEmailSupersetProfile_Metrics) {
  address_data_manager().SetIsEligibleForAddressAccountStorage(true);
  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  const AutofillProfile home_profile =
      test::OnlyAddressProfile(AutofillProfile::RecordType::kAccountHome);
  const AutofillProfile observed_profile =
      test::SupersetProfileOf({account_name_email_profile, home_profile},
                              address_data_manager().app_locale(),
                              AutofillProfile::RecordType::kLocalOrSyncable);

  address_data_manager().AddProfile(home_profile);
  address_data_manager().AddProfile(account_name_email_profile);

  // Insert guids into `unedited_autofilled_profile_guids` to trigger
  // `kAccountNameEmail` H/W merge flow.
  ProfileImportMetadata metadata;
  metadata.unedited_autofilled_profile_guids.insert(
      account_name_email_profile.guid());
  metadata.unedited_autofilled_profile_guids.insert(home_profile.guid());

  auto import_data = CreateProfileImportProcess(
      observed_profile, /*allow_only_silent_updates=*/false, metadata);

  // Simulate that the user accepts this import with edits.
  AutofillProfile edited_profile = *import_data.import_candidate();
  edited_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"Updated City", VerificationStatus::kUserVerified);
  import_data.AcceptWithEdits(edited_profile);
  base::HistogramTester histogram_tester;
  import_data.CollectMetrics(/*ukm_recorder=*/nullptr,
                             address_data_manager().GetProfiles());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.HomeWorkNameEmailMergeEditedType",
      SettingsVisibleFieldTypeForMetrics::kCity, 1);
}

}  // namespace

}  // namespace autofill
