// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
using testing::Pointee;
using testing::UnorderedElementsAre;
}  // namespace

class AddressDataCleanerTest : public testing::Test {
 public:
  AddressDataCleanerTest()
      : prefs_(test::PrefServiceForTesting()),
        address_data_cleaner_(&test_pdm_,
                              /*alternative_state_name_map_updater=*/nullptr,
                              prefs_.get()) {}

 protected:
  bool ApplyAddressDedupingRoutine() {
    return address_data_cleaner_.ApplyAddressDedupingRoutine();
  }

  bool DeleteDisusedAddresses() {
    return address_data_cleaner_.DeleteDisusedAddresses();
  }

  TestPersonalDataManager& personal_data() { return test_pdm_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  TestPersonalDataManager test_pdm_;
  AddressDataCleaner address_data_cleaner_;
};

// Tests that ApplyAddressDedupingRoutine merges the profile values correctly,
// i.e. never lose information and keep the syntax of the profile with the
// higher ranking score.
TEST_F(AddressDataCleanerTest,
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

  personal_data().AddProfile(profile1);
  personal_data().AddProfile(profile2);
  personal_data().AddProfile(profile3);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ApplyAddressDedupingRoutine());

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
TEST_F(AddressDataCleanerTest,
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

  personal_data().AddProfile(Homer1);
  personal_data().AddProfile(Homer2);
  personal_data().AddProfile(Homer3);
  personal_data().AddProfile(Homer4);
  personal_data().AddProfile(Barney);

  base::HistogramTester histogram_tester;
  // |Homer1| should get merged into |Homer2| which should then be merged into
  // |Homer3|. |Homer4| and |Barney| should not be deduped at all.
  EXPECT_TRUE(ApplyAddressDedupingRoutine());

  // Get the profiles, sorted by ranking score to have a deterministic order.
  std::vector<AutofillProfile*> profiles = personal_data().GetProfiles();

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

TEST_F(AddressDataCleanerTest,
       ApplyAddressDedupingRoutine_NopIfZeroProfiles) {
  ASSERT_TRUE(personal_data().GetProfiles().empty());
  EXPECT_FALSE(ApplyAddressDedupingRoutine());
}

TEST_F(AddressDataCleanerTest,
       ApplyAddressDedupingRoutine_NopIfOneProfile) {
  personal_data().AddProfile(test::GetFullProfile());
  EXPECT_FALSE(ApplyAddressDedupingRoutine());
}

// Tests that ApplyAddressDedupingRoutine is not run a second time on the same
// major version.
TEST_F(AddressDataCleanerTest,
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

  personal_data().AddProfile(profile1);
  personal_data().AddProfile(profile2);

  // The deduping routine should be run a first time.
  EXPECT_TRUE(ApplyAddressDedupingRoutine());

  std::vector<AutofillProfile*> profiles = personal_data().GetProfiles();

  // The profiles should have been deduped
  EXPECT_EQ(1U, profiles.size());

  // Add another duplicate profile.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  personal_data().AddProfile(profile3);

  // The deduping routine should not be run.
  EXPECT_FALSE(ApplyAddressDedupingRoutine());

  // The two duplicate profiles should still be present.
  EXPECT_EQ(2U, personal_data().GetProfiles().size());
}

// Tests that `kAccount` profiles are not deduplicated against each other.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountPairs) {
  AutofillProfile account_profile1 = test::StandardProfile();
  account_profile1.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data().AddProfile(account_profile1);
  AutofillProfile account_profile2 = test::StandardProfile();
  account_profile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data().AddProfile(account_profile2);

  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  EXPECT_THAT(personal_data().GetProfiles(),
              UnorderedElementsAre(Pointee(account_profile1),
                                   Pointee(account_profile2)));
}

// Tests that `kLocalOrSyncable` profiles which are a subset of a `kAccount`
// profile are deduplicated. The result is a Chrome account profile.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountSuperset) {
  // Create a non-Chrome account profile and a local profile.
  AutofillProfile account_profile = test::StandardProfile();
  const int non_chrome_service =
      AutofillProfile::kInitialCreatorOrModifierChrome + 1;
  account_profile.set_initial_creator_id(non_chrome_service);
  account_profile.set_last_modifier_id(non_chrome_service);
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data().AddProfile(account_profile);
  personal_data().AddProfile(test::SubsetOfStandardProfile());

  // Expect that only the account profile remains and that it became a Chrome-
  // originating profile.
  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  std::vector<AutofillProfile*> deduped_profiles =
      personal_data().GetProfiles();
  ASSERT_THAT(deduped_profiles, UnorderedElementsAre(Pointee(account_profile)));
  EXPECT_EQ(deduped_profiles[0]->initial_creator_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_EQ(deduped_profiles[0]->last_modifier_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
}

// Tests that `kAccount` profiles which are a subset of a `kLocalOrSyncable`
// profile are not deduplicated.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountSubset) {
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data().AddProfile(account_profile);
  AutofillProfile local_profile = test::StandardProfile();
  personal_data().AddProfile(local_profile);

  EXPECT_TRUE(ApplyAddressDedupingRoutine());
  EXPECT_THAT(
      personal_data().GetProfiles(),
      UnorderedElementsAre(Pointee(account_profile), Pointee(local_profile)));
}

// Tests that DeleteDisusedAddresses only deletes the addresses that are
// supposed to be deleted.
TEST_F(AddressDataCleanerTest,
       DeleteDisusedAddresses_DeleteDesiredAddressesOnly) {
  auto now = AutofillClock::Now();

  // Create a disused address (deletable).
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.set_use_date(now - base::Days(400));
  personal_data().AddProfile(profile0);

  // Create a recently-used address (not deletable).
  AutofillProfile profile1 = test::GetFullCanadianProfile();
  profile1.set_use_date(now - base::Days(4));
  personal_data().AddProfile(profile1);

  EXPECT_TRUE(DeleteDisusedAddresses());

  EXPECT_THAT(personal_data().GetProfiles(),
              UnorderedElementsAre(Pointee(profile1)));
}

}  // namespace autofill
