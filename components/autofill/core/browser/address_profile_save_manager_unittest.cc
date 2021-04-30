// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using UserDecision = AutofillClient::SaveAddressProfileOfferUserDecision;

using structured_address::VerificationStatus;

// Names of histrogram used for metric collection.
constexpr char kProfileImportTypeHistogram[] =
    "Autofill.ProfileImport.ProfileImportType";
constexpr char kNewProfileEditsHistogram[] =
    "Autofill.ProfileImport.NewProfileEditedType";
constexpr char kProfileUpdateEditsHistogram[] =
    "Autofill.ProfileImport.UpdateProfileEditedType";
constexpr char kNewProfileDecisionHistogram[] =
    "Autofill.ProfileImport.NewProfileDecision";
constexpr char kProfileUpdateDecisionHistogram[] =
    "Autofill.ProfileImport.UpdateProfileDecision";

class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  ~MockPersonalDataManager() override = default;

  MOCK_METHOD(std::string,
              SaveImportedProfile,
              (const AutofillProfile&),
              (override));
};

// This derived version of the AddressProfileSaveManager stores the last import
// for testing purposes and mocks the UI request.
class TestAddressProfileSaveManager : public AddressProfileSaveManager {
 public:
  // The parameters should outlive the AddressProfileSaveManager.
  TestAddressProfileSaveManager(AutofillClient* client,
                                PersonalDataManager* personal_data_manager);

  // Mocks the function that initiates the UI prompt for testing purposes.
  MOCK_METHOD(void, OfferSavePrompt, (), (override));

  // Returns a copy of the last finished import process or 'base::nullopt' if no
  // import process was finished.
  ProfileImportProcess* last_import();

  void OnUserDecisionForTesting(UserDecision decision,
                                AutofillProfile edited_profile) {
    pending_import()->set_prompt_was_shown();
    OnUserDecision(decision, edited_profile);
  }

 protected:
  void ClearPendingImport() override;
  // Profile that is passed from the emulated UI respones in case the user
  // edited the import candidate.
  base::Optional<ProfileImportProcess> last_import_;
};

TestAddressProfileSaveManager::TestAddressProfileSaveManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager)
    : AddressProfileSaveManager(client, personal_data_manager) {}

void TestAddressProfileSaveManager::ClearPendingImport() {
  if (pending_import()) {
    last_import_ = base::OptionalFromPtr(pending_import());
  }
  AddressProfileSaveManager::ClearPendingImport();
}

ProfileImportProcess* TestAddressProfileSaveManager::last_import() {
  return base::OptionalOrNullptr(last_import_);
}

// Definition of a test scenario.
struct ImportScenarioTestCase {
  std::vector<AutofillProfile> existing_profiles;
  AutofillProfile observed_profile;
  bool is_prompt_expected;
  UserDecision user_decision;
  AutofillProfile edited_profile;
  AutofillProfileImportType expected_import_type;
  bool is_profile_change_expected;
  base::Optional<AutofillProfile> merge_candidate;
  base::Optional<AutofillProfile> import_candidate;
  std::vector<AutofillProfile> expected_final_profiles;
  std::vector<AutofillMetrics::EditedFieldTypeForMetrics>
      expected_edited_types_for_metrics;
};

class AddressProfileSaveManagerTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable both explicit save prompts and structured names.
    // The latter is needed to test the concept of silent updates.
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillAddressProfileSavePrompt,
         features::kAutofillEnableSupportForMoreStructureInNames},
        {});
  }

  // Tests the |test_scenario|.
  void TestImportScenario(ImportScenarioTestCase& test_scenario);

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  MockPersonalDataManager mock_personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void AddressProfileSaveManagerTest::TestImportScenario(
    ImportScenarioTestCase& test_scenario) {
  // Assert that there is not a single profile stored in the personal data
  // manager.
  ASSERT_TRUE(mock_personal_data_manager_.GetProfiles().empty());

  TestAddressProfileSaveManager save_manager(&autofill_client_,
                                             &mock_personal_data_manager_);
  base::HistogramTester histogram_tester;

  // Set up the expectation and response for if a prompt should be shown.
  if (test_scenario.is_prompt_expected) {
    EXPECT_CALL(save_manager, OfferSavePrompt())
        .Times(1)
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          save_manager.OnUserDecisionForTesting(test_scenario.user_decision,
                                                test_scenario.edited_profile);
        }));
  } else {
    EXPECT_CALL(save_manager, OfferSavePrompt()).Times(0);
  }

  // Set the existing profiles to the personal data manager.
  mock_personal_data_manager_.SetProfiles(&test_scenario.existing_profiles);

  // Initiate the profile import.
  save_manager.ImportProfileFromForm(test_scenario.observed_profile, "en-US");

  // Assert that there is a finished import process on record.
  ASSERT_NE(save_manager.last_import(), nullptr);
  ProfileImportProcess* last_import = save_manager.last_import();

  EXPECT_EQ(test_scenario.expected_import_type, last_import->import_type());

  // Make a copy of the final profiles in the personal data manager for
  // comparison.
  std::vector<AutofillProfile> final_profiles;
  final_profiles.reserve(test_scenario.expected_final_profiles.size());
  for (const auto* profile : mock_personal_data_manager_.GetProfiles())
    final_profiles.push_back(*profile);

  EXPECT_EQ(test_scenario.expected_final_profiles, final_profiles);

  // Test that the merge and import candidates are correct.
  EXPECT_EQ(test_scenario.merge_candidate, last_import->merge_candidate());
  EXPECT_EQ(test_scenario.import_candidate, last_import->import_candidate());

  // Test the collection of metrics.
  histogram_tester.ExpectUniqueSample(kProfileImportTypeHistogram,
                                      test_scenario.expected_import_type, 1);

  const bool is_new_profile = test_scenario.expected_import_type ==
                              AutofillProfileImportType::kNewProfile;
  const bool is_confirmable_merge =
      test_scenario.expected_import_type ==
      AutofillProfileImportType::kConfirmableMerge;

  // If the import was neither a new profile or a confirmable merge, test that
  // the corresponing updates are unchanged.
  if (!is_new_profile && !is_confirmable_merge) {
    histogram_tester.ExpectTotalCount(kNewProfileEditsHistogram, 0);
    histogram_tester.ExpectTotalCount(kNewProfileDecisionHistogram, 0);
    histogram_tester.ExpectTotalCount(kProfileUpdateEditsHistogram, 0);
    histogram_tester.ExpectTotalCount(kProfileUpdateDecisionHistogram, 0);
  } else {
    DCHECK(!is_new_profile || !is_confirmable_merge);

    const std::string changed_decision_histo =
        is_new_profile ? kNewProfileDecisionHistogram
                       : kProfileUpdateDecisionHistogram;
    const std::string unchanged_decision_histo =
        !is_new_profile ? kNewProfileDecisionHistogram
                        : kProfileUpdateDecisionHistogram;

    const std::string changed_edits_histo = is_new_profile
                                                ? kNewProfileEditsHistogram
                                                : kProfileUpdateEditsHistogram;
    const std::string unchanged_edits_histo =
        !is_new_profile ? kNewProfileEditsHistogram
                        : kProfileUpdateEditsHistogram;

    histogram_tester.ExpectTotalCount(unchanged_decision_histo, 0);
    histogram_tester.ExpectTotalCount(unchanged_edits_histo, 0);

    histogram_tester.ExpectUniqueSample(changed_decision_histo,
                                        test_scenario.user_decision, 1);
    histogram_tester.ExpectTotalCount(
        changed_edits_histo,
        test_scenario.expected_edited_types_for_metrics.size());

    for (auto edited_type : test_scenario.expected_edited_types_for_metrics) {
      histogram_tester.ExpectBucketCount(changed_edits_histo, edited_type, 1);
    }
  }
}

// Test that a profile is correctly imported when no other profile is stored
// yet.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = base::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {observed_profile}};

  TestImportScenario(test_scenario);
}

// Test that a profile is correctly imported when no other profile is stored
// yet. Here, `kUserNotAsked` is supplied which is done as a fallback in case
// the UI is unavailable for technical reasons.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_UserNotAskedFallback) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = base::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {observed_profile}};

  TestImportScenario(test_scenario);
}

// Test that a profile is correctly imported when no other profile is stored
// yet. Here, the profile is edited by the user.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();
  // The edited profile must have the same GUID then the observerd one.
  test::CopyGUID(observed_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEdited,
      .edited_profile = edited_profile,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = base::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::EditedFieldTypeForMetrics::kName,
          AutofillMetrics::EditedFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::EditedFieldTypeForMetrics::kCity,
          AutofillMetrics::EditedFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

// Test that a decline to import a new profile is handled correctly.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = false,
      .merge_candidate = base::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {}};

  TestImportScenario(test_scenario);
}

// Test that the observation of a duplicate profile has no effect.
TEST_F(AddressProfileSaveManagerTest, ImportDuplicateProfile) {
  // Note that the profile is created twice to enforce different GUIDs.
  AutofillProfile existing_profile = test::StandardProfile();
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kDuplicateImport,
      .is_profile_change_expected = false,
      .merge_candidate = base::nullopt,
      .import_candidate = base::nullopt,
      .expected_final_profiles = {existing_profile}};

  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type = AutofillProfileImportType::kSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = base::nullopt,
      .import_candidate = base::nullopt,
      .expected_final_profiles = {final_profile}};
  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update even when the profile
// has the legacy property of being verified.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateVerifiedProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  updateable_profile.set_origin(kSettingsOrigin);
  ASSERT_TRUE(updateable_profile.IsVerified());

  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type = AutofillProfileImportType::kSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = base::nullopt,
      .import_candidate = base::nullopt,
      .expected_final_profiles = {final_profile}};
  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change. Here, `kUserNotAsked` is returned as the fallback
// mechanism when the UI is not available for technical reasons.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMerge_UserNotAskedFallback) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change. The existing profile has the legacy property of
// being verified.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_VerifiedProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  mergeable_profile.set_origin(kSettingsOrigin);
  ASSERT_TRUE(mergeable_profile.IsVerified());

  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();

  AutofillProfile import_candidate = observed_profile;
  test::CopyGUID(mergeable_profile, &import_candidate);
  test::CopyGUID(mergeable_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEdited,
      .edited_profile = edited_profile,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = import_candidate,
      .expected_final_profiles = {edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::EditedFieldTypeForMetrics::kName,
          AutofillMetrics::EditedFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::EditedFieldTypeForMetrics::kCity,
          AutofillMetrics::EditedFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change but the import is declined by the user.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = false,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {mergeable_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMergeAndDuplicate) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  AutofillProfile merged_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, merged_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  merged_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated. Here, the merge is declined.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  mergeable_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated. Here, the merge is accepted with edits.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);
  test::CopyGUID(mergeable_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEdited,
      .edited_profile = edited_profile,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::EditedFieldTypeForMetrics::kName,
          AutofillMetrics::EditedFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::EditedFieldTypeForMetrics::kCity,
          AutofillMetrics::EditedFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

TEST_F(AddressProfileSaveManagerTest, SaveProfileWhenNoSavePrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillAddressProfileSavePrompt);

  AddressProfileSaveManager save_manager(&autofill_client_,
                                         &mock_personal_data_manager_);
  AutofillProfile test_profile = test::GetFullProfile();
  EXPECT_CALL(mock_personal_data_manager_, SaveImportedProfile(test_profile));
  save_manager.ImportProfileFromForm(test_profile, "en_US");
}

}  // namespace

}  // namespace autofill
