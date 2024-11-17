// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/address_data_cleaner_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::Pointee;
using testing::UnorderedElementsAre;

class AddressDataCleanerTest : public testing::Test {
 public:
  AddressDataCleanerTest()
      : prefs_(test::PrefServiceForTesting()),
        data_cleaner_(test_adm_,
                      &sync_service_,
                      *prefs_,
                      /*alternative_state_name_map_updater=*/nullptr) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
  TestAddressDataManager test_adm_;
  AddressDataCleaner data_cleaner_;
};

// Tests that for users not syncing addresses, `MaybeCleanupAddressData()`
// immediately performs clean-ups.
TEST_F(AddressDataCleanerTest, MaybeCleanupAddressData_NotSyncingAddresses) {
  // Disable UserSelectableType::kAutofill.
  sync_service_.GetUserSettings()->SetSelectedTypes(false, {});
  ASSERT_TRUE(test_api(data_cleaner_).AreCleanupsPending());
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_FALSE(test_api(data_cleaner_).AreCleanupsPending());
}

// Tests that for syncing users `MaybeCleanupAddressData()` doesn't perform
// clean-ups, since it's expecting another call once sync is ready.
TEST_F(AddressDataCleanerTest, MaybeCleanupAddressData_SyncingAddresses) {
  sync_service_.SetDownloadStatusFor(
      {syncer::DataType::AUTOFILL_PROFILE, syncer::DataType::CONTACT_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  ASSERT_TRUE(test_api(data_cleaner_).AreCleanupsPending());
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_TRUE(test_api(data_cleaner_).AreCleanupsPending());

  sync_service_.SetDownloadStatusFor(
      {syncer::DataType::AUTOFILL_PROFILE, syncer::DataType::CONTACT_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
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
  test_adm_.AddProfile(profile1);

  AutofillProfile profile2(AddressCountryCode("US"));
  profile2.SetRawInfo(NAME_MIDDLE, u"Jay");
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));
  test_adm_.AddProfile(profile2);

  AutofillProfile profile3(AddressCountryCode("US"));
  profile3.SetRawInfo(NAME_MIDDLE, u"J");
  profile3.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile3.SetRawInfo(COMPANY_NAME, u"Fox");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));
  test_adm_.AddProfile(profile3);

  base::HistogramTester histogram_tester;
  test_api(data_cleaner_).ApplyDeduplicationRoutine();

  // `profile1` should have been merged into `profile2` which should then have
  // been merged into `profile3`. Therefore there should only be 1 saved
  // profile.
  ASSERT_EQ(1U, test_adm_.GetProfiles().size());
  AutofillProfile deduped_profile = *test_adm_.GetProfiles()[0];

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
  test_adm_.AddProfile(standard_profile);
  test_adm_.AddProfile(test::UpdateableStandardProfile());
  AutofillProfile different_profile = test::DifferentFromStandardProfile();
  test_adm_.AddProfile(different_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              UnorderedElementsAre(Pointee(standard_profile),
                                   Pointee(different_profile)));
}

TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_Metrics) {
  test_adm_.AddProfile(test::StandardProfile());
  test_adm_.AddProfile(test::UpdateableStandardProfile());

  base::HistogramTester histogram_tester;
  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 1, 1);
}

// Tests that deduplication is not run a second time on the same major version.
TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_OncePerVersion) {
  test_adm_.AddProfile(test::StandardProfile());
  test_adm_.AddProfile(test::UpdateableStandardProfile());
  // Pretend that deduplication was already run this milestone.
  prefs_->SetInteger(prefs::kAutofillLastVersionDeduped, CHROME_VERSION_MAJOR);
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_EQ(2U, test_adm_.GetProfiles().size());
}

// Tests that `kAccount` profiles are not deduplicated against each other.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountPairs) {
  AutofillProfile account_profile1 = test::StandardProfile();
  test_api(account_profile1)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile1);
  AutofillProfile account_profile2 = test::StandardProfile();
  test_api(account_profile2)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile2);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
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
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);
  test_adm_.AddProfile(test::SubsetOfStandardProfile());

  // Expect that only the account profile remains and that it became a Chrome-
  // originating profile.
  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  std::vector<const AutofillProfile*> deduped_profiles =
      test_adm_.GetProfiles();
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
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);
  AutofillProfile local_profile = test::StandardProfile();
  test_adm_.AddProfile(local_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(
      test_adm_.GetProfiles(),
      UnorderedElementsAre(Pointee(account_profile), Pointee(local_profile)));
}

TEST_F(AddressDataCleanerTest, DeleteDisusedAddresses) {
  // Create a disused address (deletable).
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.set_use_date(AutofillClock::Now() - base::Days(400));
  test_adm_.AddProfile(profile1);

  // Create a recently-used address (not deletable).
  AutofillProfile profile2 = test::GetFullCanadianProfile();
  profile1.set_use_date(AutofillClock::Now() - base::Days(4));
  test_adm_.AddProfile(profile2);

  test_api(data_cleaner_).DeleteDisusedAddresses();
  EXPECT_THAT(test_adm_.GetProfiles(), UnorderedElementsAre(Pointee(profile2)));
}

TEST_F(AddressDataCleanerTest, CalculateMinimalIncompatibleTypeSets) {
  const AutofillProfileComparator comparator("en_US");
  AutofillProfile profile = test::GetFullProfile();
  // FullProfile2 differs from `profile` in numerious ways.
  AutofillProfile other_profile1 = test::GetFullProfile2();
  // Add a profile that only differs from `profile` in its email address.
  AutofillProfile other_profile2 = test::GetFullProfile();
  other_profile2.SetRawInfo(EMAIL_ADDRESS, u"other-email@gmail.com");
  std::vector<const AutofillProfile*> other_profiles = {&other_profile1,
                                                        &other_profile2};
  // Expect that the only minimal set is the email address.
  EXPECT_THAT(
      AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
          profile, other_profiles, comparator),
      testing::UnorderedElementsAre(
          autofill_metrics::DifferingProfileWithTypeSet{&other_profile2,
                                                        {EMAIL_ADDRESS}}));
  // Add one more profile that only differs from `profile` in its phone number.
  AutofillProfile other_profile3 = test::GetFullProfile();
  other_profile3.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+49 1578 7912345");
  other_profiles.push_back(&other_profile3);
  // Expect that both minimal sets are returned.
  EXPECT_THAT(
      AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
          profile, other_profiles, comparator),
      testing::UnorderedElementsAre(
          autofill_metrics::DifferingProfileWithTypeSet{&other_profile2,
                                                        {EMAIL_ADDRESS}},
          autofill_metrics::DifferingProfileWithTypeSet{
              &other_profile3, {PHONE_HOME_WHOLE_NUMBER}}));
}

TEST_F(AddressDataCleanerTest, IsTokenLowQualityForDeduplicationPurposes) {
  using ObservationType = ProfileTokenQuality::ObservationType;

  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FULL, ObservationType::kEditedFallback);
  // Not enough observation.
  EXPECT_FALSE(AddressDataCleaner::IsTokenLowQualityForDeduplicationPurposes(
      profile, NAME_FULL));

  test_api(profile.token_quality())
      .AddObservation(NAME_FULL, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_FULL, ObservationType::kEditedFallback);
  test_api(profile.token_quality())
      .AddObservation(NAME_FULL, ObservationType::kEditedFallback);
  // Enough observations, and enough of them are "bad".
  EXPECT_TRUE(AddressDataCleaner::IsTokenLowQualityForDeduplicationPurposes(
      profile, NAME_FULL));

  test_api(profile.token_quality())
      .AddObservation(NAME_FULL, ObservationType::kAccepted);
  // Too many "good" observations for the token to be considered low quality.
  EXPECT_FALSE(AddressDataCleaner::IsTokenLowQualityForDeduplicationPurposes(
      profile, NAME_FULL));
}

}  // namespace
}  // namespace autofill
