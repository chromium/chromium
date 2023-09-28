// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_import_process.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Test that two AutofillProfiles have the same `source() and `Compare()` equal.
// This is useful for testing profile migration, which changes a profile's
// source and GUID (preventing the use of operator==).
MATCHER(CompareWithSource, "") {
  const AutofillProfile& a = std::get<0>(arg);
  const AutofillProfile& b = std::get<1>(arg);
  return a.source() == b.source() && a.Compare(b) == 0;
}

class AutofillProfileImportProcessTest : public testing::Test {
 protected:
  void BlockProfileForUpdates(const AutofillProfile& profile) {
    while (!personal_data_manager_.IsProfileUpdateBlocked(profile.guid())) {
      personal_data_manager_.AddStrikeToBlockProfileUpdate(profile.guid());
    }
  }

  void BlockDomainForNewProfiles(GURL url) {
    while (!personal_data_manager_.IsNewProfileImportBlockedForDomain(url)) {
      personal_data_manager_.AddStrikeToBlockNewProfileImportForDomain(url);
    }
  }

  // Returns all profiles stored in the `personal_data_manager_` after
  // finalizing the `import_process`'s import.
  std::vector<AutofillProfile> ApplyImportAndGetProfiles(
      ProfileImportProcess& import_process) const {
    import_process.ApplyImport();
    // For convenience, return plain objects rather than pointers.
    std::vector<AutofillProfile> profiles;
    for (const AutofillProfile* pdm_profile :
         personal_data_manager_.GetProfiles()) {
      profiles.push_back(*pdm_profile);
    }
    return profiles;
  }

  TestPersonalDataManager personal_data_manager_;
  GURL url_{"https://www.import.me/now.html"};
};

// Test that two subsequently created `ProfileImportProcess`s have distinct ids.
TEST_F(AutofillProfileImportProcessTest, DistinctIds) {
  AutofillProfile empty_profile;
  ProfileImportProcess import_data1(empty_profile, "en_US", url_,
                                    &personal_data_manager_,
                                    /*allow_only_silent_updates=*/false);
  ProfileImportProcess import_data2(empty_profile, "en_US", url_,
                                    &personal_data_manager_,
                                    /*allow_only_silent_updates=*/false);

  // The import ids should be distinct.
  EXPECT_NE(import_data1.import_id(), import_data2.import_id());

  // In fact, the import id is incremented for every initiated
  // `ProfileImportData`.
  EXPECT_EQ(import_data1.import_id().value() + 1,
            import_data2.import_id().value());
}

// Tests the import process for the scenario, that the user accepts the import
// of their first profile.
TEST_F(AutofillProfileImportProcessTest, ImportFirstProfile_UserAccepts) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();

  // Advance the test clock to make sure that the modification date of the new
  // profile gets updated.
  test_clock.Advance(base::Days(1));
  base::Time current_time = AutofillClock::Now();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), current_time);
}

// Tests the import process for the scenario, that the import of a new profile
// is blocked.
TEST_F(AutofillProfileImportProcessTest, ImportFirstProfile_ImportIsBlocked) {
  AutofillProfile observed_profile = test::StandardProfile();

  BlockDomainForNewProfiles(url_);

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  personal_data_manager_.AddProfile(observed_profile);

  // Create the import process for the scenario that the observed profile is an
  // exact copy of an already existing one.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  AutofillProfile profile = test::StandardProfile();
  EXPECT_EQ(u"US", profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  personal_data_manager_.AddProfile(profile);

  // Suppose the country was incorrectly complemented to "DE".
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");

  // Test that the import is correctly classified as a duplicate.
  ProfileImportProcess import_data(
      profile, "en_US", url_, &personal_data_manager_,
      /*allow_only_silent_updates=*/false,
      ProfileImportMetadata{.did_complement_country = true});
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

  personal_data_manager_.AddProfile(duplicate_existing_profile);
  personal_data_manager_.AddProfile(distinct_existing_profile);

  // Create the import process for the two already existing profiles.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data_manager_.AddProfile(account_profile);

  ProfileImportProcess import_data(
      /*observed_profile=*/test::StandardProfile(), "en_US", url_,
      &personal_data_manager_,
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
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data_manager_.AddProfile(account_profile);

  ProfileImportProcess import_data(
      /*observed_profile=*/test::SubsetOfStandardProfile(), "en_US", url_,
      &personal_data_manager_,
      /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
  import_data.AcceptWithoutPrompt();
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(account_profile));
}

// Tests that importing a profile that is a superset of a kAccount profile is
// rejected as a duplicate when `kAutofillAccountProfileStorage` is disabled.
TEST_F(AutofillProfileImportProcessTest,
       ImportSupersetProfile_kAccount_PreStorage) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(features::kAutofillAccountProfileStorage);

  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data_manager_.AddProfile(account_profile);

  ProfileImportProcess import_data(
      /*observed_profile=*/test::StandardProfile(), "en_US", url_,
      &personal_data_manager_,
      /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
  import_data.AcceptWithoutPrompt();
  EXPECT_FALSE(import_data.ProfilesChanged());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(account_profile));
}

// Tests that importing a profile that is a superset of a kAccount profile
// results in an update when `kAutofillAccountProfileStorage` is enabled.
// The source of resulting profile remains kAccount.
TEST_F(AutofillProfileImportProcessTest,
       ImportSupersetProfile_kAccount_PostStorage) {
  base::test::ScopedFeatureList feature(
      features::kAutofillAccountProfileStorage);

  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data_manager_.AddProfile(account_profile);

  ProfileImportProcess import_data(
      /*observed_profile=*/test::StandardProfile(), "en_US", url_,
      &personal_data_manager_,
      /*allow_only_silent_updates=*/false);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  import_data.AcceptWithoutPrompt();
  EXPECT_TRUE(import_data.ProfilesChanged());
  AutofillProfile expected_profile = test::StandardProfile();
  expected_profile.set_guid(account_profile.guid());
  expected_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(expected_profile));
}

// Tests that an import can cause a silent update of a `kAccount` profile.
TEST_F(AutofillProfileImportProcessTest, ImportSilentUpdate_kAccount) {
  AutofillProfile account_profile = test::UpdateableStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data_manager_.AddProfile(account_profile);

  // The `observed_profile` is of type `kLocalOrSyncable`. This should not
  // prevent silent-updating a `kAccount` profile.
  ProfileImportProcess import_data(
      /*observed_profile=*/test::StandardProfile(), "en_US", url_,
      &personal_data_manager_,
      /*allow_only_silent_updates=*/true);

  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdateForIncompleteProfile);
  import_data.AcceptWithoutPrompt();
  EXPECT_TRUE(import_data.ProfilesChanged());
  // Expect that the existing profiles was updated to the standard profile,
  // while maintaining it's `kAccount` status.
  AutofillProfile expected_profile = test::StandardProfile();
  expected_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  expected_profile.set_guid(account_profile.guid());
  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(expected_profile));
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile.
TEST_F(AutofillProfileImportProcessTest, MergeWithExistingProfile_Accepted) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  mergeable_profile.set_modification_date(AutofillClock::Now());
  test_clock.Advance(base::Days(1));
  base::Time current_time = AutofillClock::Now();

  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), current_time);
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile for the scenario that the user introduced additional edits.
TEST_F(AutofillProfileImportProcessTest,
       MergeWithExistingProfile_AcceptWithEdits) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  mergeable_profile.set_modification_date(AutofillClock::Now());
  test_clock.Advance(base::Days(1));
  base::Time current_time = AutofillClock::Now();

  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), current_time);
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

  personal_data_manager_.AddProfile(mergeable_profile);
  personal_data_manager_.AddProfile(distinct_profile);

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  // Since the merge is not accepted, the `modification_date` should not be
  // changed.
  mergeable_profile.set_modification_date(AutofillClock::Now());
  base::Time earlier_time = AutofillClock::Now();
  test_clock.Advance(base::Days(1));

  personal_data_manager_.AddProfile(mergeable_profile);

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), earlier_time);
}

// Tests the scenario in which the observed profile results in a silent update
// of the only already existing profile.
TEST_F(AutofillProfileImportProcessTest, SilentlyUpdateProfile) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  updateable_profile.set_modification_date(AutofillClock::Now());
  test_clock.Advance(base::Days(1));
  base::Time current_time = AutofillClock::Now();

  personal_data_manager_.AddProfile(updateable_profile);

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), current_time);
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile while another already existing profile can be silently
// updated. In this test, the users accepts the merge.
TEST_F(AutofillProfileImportProcessTest, BothMergeAndSilentUpdate_Accepted) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  personal_data_manager_.AddProfile(updateable_profile);
  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process with a mergeable and a updateable profile..
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMergeAndSilentUpdate);
  // There should be a merge candidate.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // And also an updated profile.
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 1u);

  // Simulate that the user accepts the prompt without edits.
  import_data.AcceptWithoutEdits();

  // This should result in a change of the stored profiles.
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile updated_profile = observed_profile;
  test::CopyGUID(updateable_profile, &updated_profile);
  AutofillProfile merged_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &merged_profile);

  EXPECT_THAT(ApplyImportAndGetProfiles(import_data),
              testing::UnorderedElementsAre(merged_profile, updated_profile));
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile while another already existing profile can be silently
// updated. In this test, the users declines the merge.
TEST_F(AutofillProfileImportProcessTest, BothMergeAndSilentUpdate_Rejected) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  personal_data_manager_.AddProfile(updateable_profile);
  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process with a mergeable and a updateable profile..
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMergeAndSilentUpdate);
  // There should be a merge candidate.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // And also an updated profile.
  EXPECT_EQ(import_data.silently_updated_profiles().size(), 1u);

  // Simulate that the user declines the merge.
  import_data.Declined();

  // The silent update should be performed unconditionally. Therefore, there
  // should be a change to the stored profiles nevertheless.
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile updated_profile = observed_profile;
  test::CopyGUID(updateable_profile, &updated_profile);

  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedElementsAre(mergeable_profile, updated_profile));
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile for which updates are blocked while another already existing
// profile can be silently updated.
TEST_F(AutofillProfileImportProcessTest, BlockedMergeAndSilentUpdate) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  BlockProfileForUpdates(mergeable_profile);

  personal_data_manager_.AddProfile(updateable_profile);
  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process with a mergeable and an updateable profile..
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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

// Tests the scenario in which an observed profile can be merged with an
// existing profile for which updates are blocked.
TEST_F(AutofillProfileImportProcessTest, BlockedMerge) {
  AutofillProfile observed_profile = test::StandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  BlockProfileForUpdates(mergeable_profile);
  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process with a mergeable profile.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);

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

// Tests the scenario in which the observed profile results in a silent update
// of the only already existing profile. The import process only supports
// silent updates.
TEST_F(AutofillProfileImportProcessTest,
       SilentlyUpdateProfile_WithIncompleteProfile) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();

  // Set a modification date and subsequently advance the test clock.
  updateable_profile.set_modification_date(AutofillClock::Now());
  test_clock.Advance(base::Days(1));
  base::Time current_time = AutofillClock::Now();

  personal_data_manager_.AddProfile(updateable_profile);

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/true);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdateForIncompleteProfile);
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
  EXPECT_EQ(resulting_profiles.at(0).modification_date(), current_time);
}

// Tests the scenario in which the observed profile is not imported since the
// import process only silent updates.
TEST_F(AutofillProfileImportProcessTest, SilentlyUpdateProfile_WithNewProfile) {
  TestAutofillClock test_clock;

  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/true);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kUnusableIncompleteProfile);
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

  personal_data_manager_.AddProfile(updateable_profile);
  personal_data_manager_.AddProfile(mergeable_profile);

  // Create the import process with a mergeable and an updateable profile..
  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/true);

  // Test that the type of import was determined correctly.
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdateForIncompleteProfile);
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

// Tests that for eligible users, new profiles are of source kAccount.
TEST_F(AutofillProfileImportProcessTest, NewProfileSource) {
  // Ineligible user.
  {
    personal_data_manager_.SetIsEligibleForAddressAccountStorage(false);
    ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                     &personal_data_manager_,
                                     /*allow_only_silent_updates=*/false);
    EXPECT_EQ(import_data.import_candidate()->source(),
              AutofillProfile::Source::kLocalOrSyncable);
  }

  // Eligible user.
  {
    personal_data_manager_.SetIsEligibleForAddressAccountStorage(true);
    ProfileImportProcess import_data(test::StandardProfile(), "en_US", url_,
                                     &personal_data_manager_,
                                     /*allow_only_silent_updates=*/false);
    EXPECT_EQ(import_data.import_candidate()->source(),
              AutofillProfile::Source::kAccount);

    // Profiles with an ineligible country are not stored in the account.
    AutofillProfile ineligible_profile = test::StandardProfile();
    ineligible_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"SD");
    import_data = ProfileImportProcess(ineligible_profile, "en_US", url_,
                                       &personal_data_manager_,
                                       /*allow_only_silent_updates=*/false);
    EXPECT_EQ(import_data.import_candidate()->source(),
              AutofillProfile::Source::kLocalOrSyncable);
  }
}

// Two `kLocalOrSyncable` profiles are stored. One of them is observed during
// submission. Expected that this profile is offered for migration.
// After accepting, expect that the profile's source has changed and that the
// second profile is left unaltered.
TEST_F(AutofillProfileImportProcessTest, MigrateProfileToAccount) {
  const AutofillProfile profile_to_migrate = test::StandardProfile();
  const AutofillProfile other_profile = test::DifferentFromStandardProfile();
  personal_data_manager_.AddProfile(profile_to_migrate);
  personal_data_manager_.AddProfile(other_profile);
  personal_data_manager_.SetIsEligibleForAddressAccountStorage(true);

  ProfileImportProcess import_data(
      /*observed_profile=*/profile_to_migrate, "en_US", url_,
      &personal_data_manager_,
      /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kProfileMigration);
  EXPECT_EQ(import_data.import_candidate(), profile_to_migrate);

  import_data.AcceptWithoutEdits();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(
          CompareWithSource(),
          {profile_to_migrate.ConvertToAccountProfile(), other_profile}));
}

// Test that the profile to migrate can be silently updated. Expect that after
// accepting the migration, the stored profile has source `kAccount` and was
// silently updated.
TEST_F(AutofillProfileImportProcessTest, MigrateProfileToAccount_SilentUpdate) {
  const AutofillProfile profile_to_migrate = test::UpdateableStandardProfile();
  const AutofillProfile observed_profile = test::StandardProfile();
  personal_data_manager_.AddProfile(profile_to_migrate);
  personal_data_manager_.SetIsEligibleForAddressAccountStorage(true);

  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);
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
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(
          CompareWithSource(), {observed_profile.ConvertToAccountProfile()}));
}

// Even if a profile migration is rejected, silent updates are applied.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_SilentUpdate_Decline) {
  const AutofillProfile migration_candidate = test::UpdateableStandardProfile();
  const AutofillProfile observed_profile = test::StandardProfile();
  personal_data_manager_.AddProfile(migration_candidate);
  personal_data_manager_.SetIsEligibleForAddressAccountStorage(true);

  ProfileImportProcess import_data(observed_profile, "en_US", url_,
                                   &personal_data_manager_,
                                   /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kProfileMigrationAndSilentUpdate);
  import_data.Declined();
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_THAT(
      ApplyImportAndGetProfiles(import_data),
      testing::UnorderedPointwise(CompareWithSource(), {observed_profile}));
}

// Expect that no migration is offered for ineligible users.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_IneligibleUser) {
  const AutofillProfile profile = test::StandardProfile();
  personal_data_manager_.AddProfile(profile);
  personal_data_manager_.SetIsEligibleForAddressAccountStorage(false);

  ProfileImportProcess import_data(
      /*observed_profile=*/profile, "en_US", url_, &personal_data_manager_,
      /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
}

// Expect that no migration is offered for ineligible profiles.
TEST_F(AutofillProfileImportProcessTest,
       MigrateProfileToAccount_IneligibleProfile) {
  AutofillProfile profile = test::StandardProfile();
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"KP");
  personal_data_manager_.AddProfile(profile);

  ProfileImportProcess import_data(
      /*observed_profile=*/profile, "en_US", url_, &personal_data_manager_,
      /*allow_only_silent_updates=*/false);
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);
}

}  // namespace

}  // namespace autofill
