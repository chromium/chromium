// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/address_data_cleaner_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
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
        data_cleaner_(test_pdm_,
                      &sync_service_,
                      *prefs_,
                      /*alternative_state_name_map_updater=*/nullptr) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
  TestPersonalDataManager test_pdm_;
  AddressDataCleaner data_cleaner_;
};

// Tests that for non-syncing users `MaybeCleanupAddressData()` immediately
// performs clean-ups.
TEST_F(AddressDataCleanerTest, MaybeCleanupAddressData_NotSyncing) {
  sync_service_.SetHasSyncConsent(false);
  ASSERT_TRUE(test_api(data_cleaner_).AreCleanupsPending());
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_FALSE(test_api(data_cleaner_).AreCleanupsPending());
}

// Tests that for syncing users `MaybeCleanupAddressData()` doesn't perform
// clean-ups, since it's expecting another call once sync is ready.
TEST_F(AddressDataCleanerTest, MaybeCleanupAddressData_Syncing) {
  sync_service_.SetDownloadStatusFor(
      {syncer::ModelType::AUTOFILL_PROFILE, syncer::ModelType::CONTACT_INFO},
      syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);
  ASSERT_TRUE(test_api(data_cleaner_).AreCleanupsPending());
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_TRUE(test_api(data_cleaner_).AreCleanupsPending());

  sync_service_.SetDownloadStatusFor(
      {syncer::ModelType::AUTOFILL_PROFILE, syncer::ModelType::CONTACT_INFO},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_FALSE(test_api(data_cleaner_).AreCleanupsPending());
}

// Tests that ApplyAddressDedupingRoutine merges the profile values correctly,
// i.e. never lose information and keep the syntax of the profile with the
// higher ranking score.
TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_MergedProfileValues) {
  // Create three profiles with slightly different values and decreasing ranking
  // scores.
  AutofillProfile profile1(AddressCountryCode("US"));
  profile1.SetRawInfo(NAME_MIDDLE, u"J");
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, u"742. Evergreen Terrace");
  profile1.set_use_count(10);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));
  test_pdm_.AddProfile(profile1);

  AutofillProfile profile2(AddressCountryCode(""));
  profile2.SetRawInfo(NAME_MIDDLE, u"Jay");
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));
  test_pdm_.AddProfile(profile2);

  AutofillProfile profile3(AddressCountryCode(""));
  profile3.SetRawInfo(NAME_MIDDLE, u"J");
  profile3.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile3.SetRawInfo(COMPANY_NAME, u"Fox");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));
  test_pdm_.AddProfile(profile3);

  base::HistogramTester histogram_tester;
  test_api(data_cleaner_).ApplyDeduplicationRoutine();

  // `profile1` should have been merged into `profile2` which should then have
  // been merged into `profile3`. Therefore there should only be 1 saved
  // profile.
  ASSERT_EQ(1U, test_pdm_.GetProfiles().size());
  AutofillProfile deduped_profile = *test_pdm_.GetProfiles()[0];

  // Since profiles with higher ranking scores are merged into profiles with
  // lower ranking scores, the result of the merge should be contained in
  // profile3 since it had a lower ranking score compared to profile1.
  EXPECT_EQ(profile3.guid(), deduped_profile.guid());
  // The address syntax that results from the merge should be the one from the
  // imported profile (highest ranking).
  EXPECT_EQ(u"742. Evergreen Terrace",
            deduped_profile.GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be full, even if the profile with the higher
  // ranking only had an initial (no loss of information).
  EXPECT_EQ(u"Jay", deduped_profile.GetRawInfo(NAME_MIDDLE));
  // The specified phone number from profile1 should be kept (no loss of
  // information).
  EXPECT_EQ(u"12345678910",
            deduped_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The specified company name from profile2 should be kept (no loss of
  // information).
  EXPECT_EQ(u"Fox", deduped_profile.GetRawInfo(COMPANY_NAME));
  // The specified country from the imported profile should be kept (no loss of
  // information).
  EXPECT_EQ(u"US", deduped_profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of all the
  // profiles use counts.
  EXPECT_EQ(10U, deduped_profile.use_count());
  // The use date that results from the merge should be the one from the
  // profile1 since it was the most recently used profile.
  EXPECT_LT(profile1.use_date() - base::Seconds(10),
            deduped_profile.use_date());
}

// Tests that ApplyDeduplicationRoutine doesn't affect profiles that shouldn't
// get deduplicated.
TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_UnrelatedProfile) {
  // Expect that the `UpdateableStandardProfile()` is deduplicated into the
  // `StandardProfile()`, but the `UpdateableStandardProfile()` remains
  // unaffected.
  AutofillProfile standard_profile = test::StandardProfile();
  test_pdm_.AddProfile(standard_profile);
  test_pdm_.AddProfile(test::UpdateableStandardProfile());
  AutofillProfile different_profile = test::DifferentFromStandardProfile();
  test_pdm_.AddProfile(different_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_pdm_.GetProfiles(),
              UnorderedElementsAre(Pointee(standard_profile),
                                   Pointee(different_profile)));
}

TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_Metrics) {
  test_pdm_.AddProfile(test::StandardProfile());
  test_pdm_.AddProfile(test::UpdateableStandardProfile());

  base::HistogramTester histogram_tester;
  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 1, 1);
}

// Tests that deduplication is not run a second time on the same major version.
TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_OncePerVersion) {
  test_pdm_.AddProfile(test::StandardProfile());
  test_pdm_.AddProfile(test::UpdateableStandardProfile());
  // Pretend that deduplication was already run this milestone.
  prefs_->SetInteger(prefs::kAutofillLastVersionDeduped, CHROME_VERSION_MAJOR);
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_EQ(2U, test_pdm_.GetProfiles().size());
}

// Tests that `kAccount` profiles are not deduplicated against each other.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountPairs) {
  AutofillProfile account_profile1 = test::StandardProfile();
  account_profile1.set_source_for_testing(AutofillProfile::Source::kAccount);
  test_pdm_.AddProfile(account_profile1);
  AutofillProfile account_profile2 = test::StandardProfile();
  account_profile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  test_pdm_.AddProfile(account_profile2);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_pdm_.GetProfiles(),
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
  test_pdm_.AddProfile(account_profile);
  test_pdm_.AddProfile(test::SubsetOfStandardProfile());

  // Expect that only the account profile remains and that it became a Chrome-
  // originating profile.
  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  std::vector<AutofillProfile*> deduped_profiles = test_pdm_.GetProfiles();
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
  test_pdm_.AddProfile(account_profile);
  AutofillProfile local_profile = test::StandardProfile();
  test_pdm_.AddProfile(local_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(
      test_pdm_.GetProfiles(),
      UnorderedElementsAre(Pointee(account_profile), Pointee(local_profile)));
}

TEST_F(AddressDataCleanerTest, DeleteDisusedAddresses) {
  // Create a disused address (deletable).
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.set_use_date(AutofillClock::Now() - base::Days(400));
  test_pdm_.AddProfile(profile1);

  // Create a recently-used address (not deletable).
  AutofillProfile profile2 = test::GetFullCanadianProfile();
  profile1.set_use_date(AutofillClock::Now() - base::Days(4));
  test_pdm_.AddProfile(profile2);

  test_api(data_cleaner_).DeleteDisusedAddresses();
  EXPECT_THAT(test_pdm_.GetProfiles(), UnorderedElementsAre(Pointee(profile2)));
}

}  // namespace autofill
