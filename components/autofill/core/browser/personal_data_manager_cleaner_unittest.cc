// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalDataManagerCleanerTest : public PersonalDataManagerTestBase,
                                       public testing::Test {
 public:
  PersonalDataManagerCleanerTest() = default;
  ~PersonalDataManagerCleanerTest() override = default;

  void SetUp() override {
    SetUpTest();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    ResetPersonalDataManager(
        /*use_sync_transport_mode=*/false, personal_data_.get());
    personal_data_manager_cleaner_ =
        std::make_unique<PersonalDataManagerCleaner>(personal_data_.get(),
                                                     nullptr, prefs_.get());
  }

  void TearDown() override {
    personal_data_manager_cleaner_.reset();
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
    TearDownTest();
  }

 protected:
  // Runs the deduplication routine on a set of `profiles` and returns the
  // result. For simplicity, this function skips updating the `personal_data_`
  // and just operates on the `profiles`.
  std::vector<AutofillProfile> DeduplicateProfiles(
      const std::vector<AutofillProfile>& profiles) {
    // `DedupeProfiles()` takes a vector of unique_ptrs. This
    // function's interface uses regular AutofillProfiles instead, since this
    // simplifies testing. So convert back and forth.
    std::vector<std::unique_ptr<AutofillProfile>> profile_ptrs;
    for (const AutofillProfile& profile : profiles) {
      profile_ptrs.push_back(std::make_unique<AutofillProfile>(profile));
    }
    std::unordered_set<std::string> profiles_to_delete;
    DedupeProfiles(&profile_ptrs, &profiles_to_delete);
    // Convert back and remove all `profiles_to_delete`, since
    // `DedupeProfiles()` doesn't modify `profile_ptrs`.
    std::vector<AutofillProfile> deduped_profiles;
    for (const std::unique_ptr<AutofillProfile>& profile : profile_ptrs) {
      deduped_profiles.push_back(*profile);
    }
    std::erase_if(deduped_profiles, [&](const AutofillProfile& profile) {
      return profiles_to_delete.contains(profile.guid());
    });
    return deduped_profiles;
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged()).Times(1);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  bool ApplyAddressDedupingRoutine() {
    return personal_data_manager_cleaner_->ApplyAddressDedupingRoutine();
  }

  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<std::string>* profile_guids_to_delete) const {
    personal_data_manager_cleaner_->DedupeProfiles(existing_profiles,
                                                   profile_guids_to_delete);
  }

  bool DeleteDisusedAddresses() {
    return personal_data_manager_cleaner_->DeleteDisusedAddresses();
  }

  PersonalDataManager& personal_data() { return *personal_data_.get(); }

 private:
  std::unique_ptr<PersonalDataManager> personal_data_;
  std::unique_ptr<PersonalDataManagerCleaner> personal_data_manager_cleaner_;
};

// Tests that ApplyAddressDedupingRoutine merges the profile values correctly,
// i.e. never lose information and keep the syntax of the profile with the
// higher ranking score.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyAddressDedupingRoutine_MergedProfileValues) {
  // Create a profile with a higher ranking score.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(10);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a medium ranking score.
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a profile with a lower ranking score.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved;
  EXPECT_EQ(3U, personal_data().GetProfiles().size());

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  PersonalDataProfileTaskWaiter(personal_data()).Wait();

  std::vector<AutofillProfile*> profiles = personal_data().GetProfiles();

  // |profile1| should have been merged into |profile2| which should then have
  // been merged into |profile3|. Therefore there should only be 1 saved
  // profile.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profiles were removed (profiles 1 and 2).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Since profiles with higher ranking scores are merged into profiles with
  // lower ranking scores, the result of the merge should be contained in
  // profile3 since it had a lower ranking score compared to profile1.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  // The address syntax that results from the merge should be the one from the
  // imported profile (highest ranking).
  EXPECT_EQ(u"742. Evergreen Terrace",
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be full, even if the profile with the higher
  // ranking only had an initial (no loss of information).
  EXPECT_EQ(u"Jay", profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The specified phone number from profile1 should be kept (no loss of
  // information).
  EXPECT_EQ(u"12345678910", profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The specified company name from profile2 should be kept (no loss of
  // information).
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  // The specified country from the imported profile should be kept (no loss of
  // information).
  EXPECT_EQ(u"US", profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of all the
  // profiles use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // profile1 since it was the most recently used profile.
  EXPECT_LT(profile1.use_date() - base::Seconds(10), profiles[0]->use_date());
}

// Tests that ApplyAddressDedupingRoutine works as expected in a realistic
// scenario. Tests that it merges the different set of similar profiles
// independently and that the resulting profiles have the right values. It has
// no effect on the other profiles.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyAddressDedupingRoutine_MultipleDedupes) {
  // Create a Homer home profile with a higher ranking score than other Homer
  // profiles.
  AutofillProfile Homer1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&Homer1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  Homer1.set_use_count(10);
  Homer1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a Homer home profile with a medium ranking score compared to other
  // Homer profiles.
  AutofillProfile Homer2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&Homer2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Homer2.set_use_count(5);
  Homer2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a Homer home profile with a lower ranking score than other Homer
  // profiles.
  AutofillProfile Homer3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&Homer3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  Homer3.set_use_count(3);
  Homer3.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Homer work profile (different address).
  AutofillProfile Homer4(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&Homer4, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "12 Nuclear Plant.", "",
                       "Springfield", "IL", "91601", "US", "9876543");
  Homer4.set_use_count(3);
  Homer4.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Barney profile (guest user).
  AutofillProfile Barney(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&Barney, "Barney", "", "Gumble", "barney.gumble@abc.com",
                       "ABC", "123 Other Street", "", "Springfield", "IL",
                       "91601", "", "");
  Barney.set_use_count(1);
  Barney.set_use_date(AutofillClock::Now() - base::Days(180));
  Barney.FinalizeAfterImport();

  AddProfileToPersonalDataManager(Homer1);
  AddProfileToPersonalDataManager(Homer2);
  AddProfileToPersonalDataManager(Homer3);
  AddProfileToPersonalDataManager(Homer4);
  AddProfileToPersonalDataManager(Barney);

  // Make sure the 5 profiles were saved;
  EXPECT_EQ(5U, personal_data().GetProfiles().size());

  base::HistogramTester histogram_tester;

  // |Homer1| should get merged into |Homer2| which should then be merged into
  // |Homer3|. |Homer4| and |Barney| should not be deduped at all.
  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  PersonalDataProfileTaskWaiter(personal_data()).Wait();

  // Get the profiles, sorted by ranking score to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data().GetProfilesToSuggest();

  // The 2 duplicates Homer home profiles with the higher ranking score  should
  // have been deduped.
  ASSERT_EQ(3U, profiles.size());
  // 5 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 5, 1);
  // 2 profile were removed (|Homer1|, |Homer2|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // The remaining profiles should be |Homer3|, |Homer4| and |Barney| in this
  // order of ranking score.
  EXPECT_EQ(Homer3.guid(), profiles[0]->guid());
  EXPECT_EQ(Homer4.guid(), profiles[1]->guid());
  EXPECT_EQ(Barney.guid(), profiles[2]->guid());

  // |Homer3|'s data:
  // The address should be saved with the syntax of |Homer1| since it has the
  // highest ranking score.
  EXPECT_EQ(u"742. Evergreen Terrace",
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be the full version found in |Homer2|,
  EXPECT_EQ(u"Jay", profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The phone number from |Homer2| should be kept (no loss of information).
  EXPECT_EQ(u"12345678910", profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The company name from |Homer3| should be kept (no loss of information).
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  // The country from |Homer1| profile should be kept (no loss of information).
  EXPECT_EQ(u"US", profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of Homer 1, 2
  // and 3's respective use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // |Homer1| since it was the most recently used profile.
  EXPECT_LT(Homer1.use_date() - base::Seconds(5), profiles[0]->use_date());
  EXPECT_GT(Homer1.use_date() + base::Seconds(5), profiles[0]->use_date());

  // The other profiles should not have been modified.
  EXPECT_TRUE(Homer4 == *profiles[1]);
  EXPECT_TRUE(Barney == *profiles[2]);
}

TEST_F(PersonalDataManagerCleanerTest,
       ApplyAddressDedupingRoutine_NopIfZeroProfiles) {
  EXPECT_TRUE(personal_data().GetProfiles().empty());
  EXPECT_FALSE(ApplyAddressDedupingRoutine());
}

TEST_F(PersonalDataManagerCleanerTest,
       ApplyAddressDedupingRoutine_NopIfOneProfile) {
  // Create a profile to dedupe.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  AddProfileToPersonalDataManager(profile);

  EXPECT_EQ(1U, personal_data().GetProfiles().size());
  EXPECT_FALSE(ApplyAddressDedupingRoutine());
}

// Tests that ApplyAddressDedupingRoutine is not run a second time on the same
// major version.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyAddressDedupingRoutine_OncePerVersion) {
  // Create a profile to dedupe.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  // Create a similar profile.
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(2U, personal_data().GetProfiles().size());

  // The deduping routine should be run a first time.
  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  PersonalDataProfileTaskWaiter(personal_data()).Wait();

  std::vector<AutofillProfile*> profiles = personal_data().GetProfiles();

  // The profiles should have been deduped
  EXPECT_EQ(1U, profiles.size());

  // Add another duplicate profile.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile3);

  // Make sure |profile3| was saved.
  EXPECT_EQ(2U, personal_data().GetProfiles().size());

  // The deduping routine should not be run.
  EXPECT_FALSE(ApplyAddressDedupingRoutine());

  // The two duplicate profiles should still be present.
  EXPECT_EQ(2U, personal_data().GetProfiles().size());
}

// Tests that `kAccount` profiles are not deduplicated against each other.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountPairs) {
  AutofillProfile account_profile1 = test::StandardProfile();
  account_profile1.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile account_profile2 = test::StandardProfile();
  account_profile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  EXPECT_THAT(
      DeduplicateProfiles({account_profile1, account_profile2}),
      testing::UnorderedElementsAre(account_profile1, account_profile2));
}

// Tests that `kLocalOrSyncable` profiles which are a subset of a `kAccount`
// profile are deduplicated. The result is a Chrome account profile.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountSuperset) {
  // Create a non-Chrome account profile and a local profile.
  AutofillProfile account_profile = test::StandardProfile();
  const int non_chrome_service =
      AutofillProfile::kInitialCreatorOrModifierChrome + 1;
  account_profile.set_initial_creator_id(non_chrome_service);
  account_profile.set_last_modifier_id(non_chrome_service);
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile local_profile = test::SubsetOfStandardProfile();

  // Expect that only the account profile remains and that it became a Chrome-
  // originating profile.
  std::vector<AutofillProfile> deduped_profiles =
      DeduplicateProfiles({account_profile, local_profile});
  EXPECT_THAT(deduped_profiles, testing::UnorderedElementsAre(account_profile));
  EXPECT_EQ(deduped_profiles[0].initial_creator_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_EQ(deduped_profiles[0].last_modifier_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
}

// Tests that `kAccount` profiles which are a subset of a `kLocalOrSyncable`
// profile are not deduplicated.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountSubset) {
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile local_profile = test::StandardProfile();
  EXPECT_THAT(DeduplicateProfiles({account_profile, local_profile}),
              testing::UnorderedElementsAre(account_profile, local_profile));
}

// Tests that DeleteDisusedAddresses only deletes the addresses that are
// supposed to be deleted.
TEST_F(PersonalDataManagerCleanerTest,
       DeleteDisusedAddresses_DeleteDesiredAddressesOnly) {
  auto now = AutofillClock::Now();

  // Create a disused address (deletable).
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.set_use_date(now - base::Days(400));
  AddProfileToPersonalDataManager(profile0);

  // Create a recently-used address (not deletable).
  AutofillProfile profile1 = test::GetFullCanadianProfile();
  profile1.set_use_date(now - base::Days(4));
  AddProfileToPersonalDataManager(profile1);

  EXPECT_TRUE(DeleteDisusedAddresses());
  PersonalDataProfileTaskWaiter(personal_data()).Wait();

  EXPECT_THAT(personal_data().GetProfiles(),
              testing::UnorderedElementsAre(testing::Pointee(profile1)));
}

}  // namespace autofill
