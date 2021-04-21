// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_import_process.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using structured_address::VerificationStatus;

namespace {

// Test that two subsequently created `ProfileImportProcess`s have distinct ids.
TEST(AutofillProfileImportProcess, DistinctIds) {
  AutofillProfile empty_profile;
  ProfileImportProcess import_data1(empty_profile, {}, "en_US");
  ProfileImportProcess import_data2(empty_profile, {}, "en_US");

  // The import ids should be distinct.
  EXPECT_NE(import_data1.import_id(), import_data2.import_id());

  // In fact, the import id is incremented for every initiated
  // `ProfileImportData`.
  EXPECT_EQ(import_data1.import_id().value() + 1,
            import_data2.import_id().value());
}

// Tests the import process for the scenario, that the user accepts the import
// of their first profile.
TEST(AutofillProfileImportProcess, ImportFirstProfile_UserAccepts) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  ProfileImportProcess import_data(observed_profile, {}, "en_US");

  // Simulate the acceptance of the save prompt.
  import_data.AcceptWithoutEdits();

  // This operation should result in a profile change, and the type of the
  // import corresponds to the creation of a new profile.
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_TRUE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  // Test that the user decision translates correctly to the expected end
  // result.
  std::vector<AutofillProfile> expected_resulting_profiles = {observed_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the import process for the scenario, that the user accepts the import
// of their first profile but with additional edits..
TEST(AutofillProfileImportProcess, ImportFirstProfile_UserAcceptsWithEdits) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  ProfileImportProcess import_data(observed_profile, {}, "en_US");

  // Simulate that the user accepts the save prompt but only after editing the
  // profile. Note, that the `guid` of the edited profile must match the `guid`
  // of the initial import candidate.
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();
  test::CopyGUID(observed_profile, &edited_profile);
  import_data.AcceptWithEdits(edited_profile);

  // This operation should result in a profile change, and the type of the
  // import corresponds to the creation of a new profile.
  EXPECT_TRUE(import_data.ProfilesChanged());
  EXPECT_TRUE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  // Test that the user decision translates correctly to the expected end
  // result.
  std::vector<AutofillProfile> expected_resulting_profiles = {edited_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the import process for the scenario, that the user declines the import
// of their first profile.
TEST(AutofillProfileImportProcess, ImportFirstProfile_UserRejects) {
  AutofillProfile observed_profile = test::StandardProfile();

  // Create the import process for the scenario that there aren't any other
  // stored profiles yet.
  ProfileImportProcess import_data(observed_profile, {}, "en_US");

  // Simulate the decline of the user.
  import_data.Declined();

  // Since the user declined, there should be no change to the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());
  // The type of import was nevertheless corresponds to the creation of a new
  // profile.
  EXPECT_TRUE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(), AutofillProfileImportType::kNewProfile);

  // Test that the final state of the profiles is the initial state.
  std::vector<AutofillProfile> expected_resulting_profiles = {};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the import of a profile that is an exact duplicate of the only already
// existing profile.
TEST(AutofillProfileImportProcess, ImportDuplicateProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_profile = observed_profile;

  // Create the import process for the scenario that the observed profile is an
  // exact copy of an already existing one.
  ProfileImportProcess import_data(observed_profile, {&existing_profile},
                                   "en_US");

  // Test that the import of a duplicate is determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);

  // In this scenario, the user should not be queried and the process is
  // silently accepted.
  import_data.AcceptWithoutPrompt();

  // There should be no change to the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());

  std::vector<AutofillProfile> expected_resulting_profiles = {existing_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the import of a profile that is an exact duplicate of an already
// existing profile along with other profiles that are not mergeable or
// updateable with the observed profile.
TEST(AutofillProfileImportProcess,
     ImportDuplicateProfile_OutOfMultipleProfiles) {
  AutofillProfile observed_profile = test::StandardProfile();
  // This already existing profile is an exact duplicate of the observed one.
  AutofillProfile duplicate_existing_profile = observed_profile;
  // This already existing profile is neither mergeable nor updateable with the
  // observed one.
  AutofillProfile distinct_existing_profile =
      test::DifferentFromStandardProfile();

  // Create the import process for the two already existing profiles.
  ProfileImportProcess import_data(
      observed_profile,
      {&duplicate_existing_profile, &distinct_existing_profile}, "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kDuplicateImport);

  // In this scenario, the user should not be queried and the process is
  // silently accepted.
  import_data.AcceptWithoutPrompt();

  // Verify that this operation does not result in a change of the profiles.
  EXPECT_FALSE(import_data.ProfilesChanged());

  std::vector<AutofillProfile> expected_resulting_profiles = {
      duplicate_existing_profile, distinct_existing_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile.
TEST(AutofillProfileImportProcess, MergeWithExistingProfile_Accepted) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  ProfileImportProcess import_data(observed_profile, {&mergeable_profile},
                                   "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
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

  std::vector<AutofillProfile> expected_resulting_profiles = {final_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile for the scenario that the user introduced additional edits.
TEST(AutofillProfileImportProcess, MergeWithExistingProfile_AcceptWithEdits) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Create the import process for the scenario that a profile that is mergeable
  // with the observed profile already exists.
  ProfileImportProcess import_data(observed_profile, {&mergeable_profile},
                                   "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
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

  // Test that the user decision translates correctly to the expected end
  // result.
  std::vector<AutofillProfile> expected_resulting_profiles = {edited_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the accepted import of a profile that is mergeable with an already
// existing profile for the scenario that there are multiple profiles stored.
TEST(AutofillProfileImportProcess,
     MergeWithExistingProfile_MultipleStoredProfiles_Accepted) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  // This is just another completely different profile.
  AutofillProfile distinct_profile = test::DifferentFromStandardProfile();

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  ProfileImportProcess import_data(
      observed_profile, {&mergeable_profile, &distinct_profile}, "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
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
  std::vector<AutofillProfile> expected_resulting_profiles = {distinct_profile,
                                                              merged_profile};

  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the rejection of the merge of the observed profile with an already
// existing one.
TEST(AutofillProfileImportProcess, MergeWithExistingProfile_Rejected) {
  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Create an import data instance for the observed profile and determine the
  // import type for the case that there are no already existing profiles.
  ProfileImportProcess import_data(observed_profile, {&mergeable_profile},
                                   "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be merge candidate that is the existing profile.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // But there should be no further updates profiles.
  EXPECT_EQ(import_data.updated_profiles().size(), 0u);

  // Simulate the decline by the user.
  import_data.Declined();

  // Since there are no additional updates, this should result in no overall
  // changes.
  EXPECT_FALSE(import_data.ProfilesChanged());

  // Test that the user decision translates correctly to the expected end
  // result.
  std::vector<AutofillProfile> expected_resulting_profiles = {
      mergeable_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the scenario in which the observed profile results in a silent update
// of the only already existing profile.
TEST(AutofillProfileImportProcess, SilentlyUpdateProfile) {
  // Silent updates need structured names to be enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();

  // Create the import process for the scenario that there is an existing
  // profile that is updateable with the observed profile.
  ProfileImportProcess import_data(observed_profile, {&updateable_profile},
                                   "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_FALSE(import_data.ImportIsMerge());
  EXPECT_TRUE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kSilentUpdate);
  // There should be no merge candidate since this is only a silent update.
  EXPECT_FALSE(import_data.merge_candidate().has_value());
  // But there should be one updated profiles.
  EXPECT_EQ(import_data.updated_profiles().size(), 1u);

  // In this scenario, the user should not be prompted.
  import_data.AcceptWithoutPrompt();

  // The operation should result in a change of the profiles
  EXPECT_TRUE(import_data.ProfilesChanged());

  // Test that the existing profile was correctly updated.
  AutofillProfile updated_profile = test::StandardProfile();
  updated_profile.set_guid(updateable_profile.guid());
  std::vector<AutofillProfile> expected_resulting_profiles = {updated_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile while another already existing profile can be silently
// updated. In this test, the users accepts the merge.
TEST(AutofillProfileImportProcess, BothMergeAndSilentUpdate_Accepted) {
  // Silent updates need structured names to be enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Create the import process with a mergeable and a updateable profile..
  ProfileImportProcess import_data(
      observed_profile, {&updateable_profile, &mergeable_profile}, "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be a merge candidate.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // And also an updated profile.
  EXPECT_EQ(import_data.updated_profiles().size(), 1u);

  // Simulate that the user accepts the prompt without edits.
  import_data.AcceptWithoutEdits();

  // This should result in a change of the stored profiles.
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile updated_profile = observed_profile;
  test::CopyGUID(updateable_profile, &updated_profile);
  AutofillProfile merged_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &merged_profile);
  std::vector<AutofillProfile> expected_resulting_profiles = {updated_profile,
                                                              merged_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

// Tests the scenario in which an observed profile can be merged with an
// existing profile while another already existing profile can be silently
// updated. In this test, the users declines the merge.
TEST(AutofillProfileImportProcess, BothMergeAndSilentUpdate_Rejected) {
  // Silent updates need structured names to be enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  AutofillProfile observed_profile = test::StandardProfile();
  // The profile should be updateable with the observed profile.
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  // This profile should be mergeable with the observed profile.
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Create the import process with a mergeable and a updateable profile..
  ProfileImportProcess import_data(
      observed_profile, {&updateable_profile, &mergeable_profile}, "en_US");

  // Test that the type of import was determined correctly.
  EXPECT_FALSE(import_data.ImportIsNewProfile());
  EXPECT_TRUE(import_data.ImportIsMerge());
  EXPECT_FALSE(import_data.ImportIsSilentUpdate());
  EXPECT_EQ(import_data.import_type(),
            AutofillProfileImportType::kConfirmableMerge);
  // There should be a merge candidate.
  ASSERT_TRUE(import_data.merge_candidate().has_value());
  EXPECT_EQ(import_data.merge_candidate(), mergeable_profile);
  // And also an updated profile.
  EXPECT_EQ(import_data.updated_profiles().size(), 1u);

  // Simulate that the user declines the merge.
  import_data.Declined();

  // The silent update should be performed unconditionally. Therefore, there
  // should be a change to the stored profiles nevertheless.
  EXPECT_TRUE(import_data.ProfilesChanged());

  AutofillProfile updated_profile = observed_profile;
  test::CopyGUID(updateable_profile, &updated_profile);

  std::vector<AutofillProfile> expected_resulting_profiles = {
      updated_profile, mergeable_profile};
  EXPECT_EQ(import_data.GetResultingProfiles(), expected_resulting_profiles);
}

}  // namespace

}  // namespace autofill
