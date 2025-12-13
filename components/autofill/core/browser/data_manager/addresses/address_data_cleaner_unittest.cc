// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
                      /*alternative_state_name_map_updater=*/nullptr) {
    prefs_->SetBoolean(prefs::kAutofillRanExtraDeduplication, false);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
  TestAddressDataManager test_adm_;
  AddressDataCleaner data_cleaner_;
};

class MockAddressDataCleaner : public AddressDataCleaner {
 public:
  using AddressDataCleaner::AddressDataCleaner;
  MOCK_METHOD(void, ApplyDeduplicationRoutine, (), (override));
};

// Two profiles are considered equal for deduplication purposes if they compare
// equal and have the same record type.
MATCHER(IsEqualForDeduplicationPurposes, "") {
  const AutofillProfile* a = std::get<0>(arg);
  const AutofillProfile& b = std::get<1>(arg);

  return a->record_type() == b.record_type() && a->Compare(b) == 0 &&
         a->usage_history().use_count() == b.usage_history().use_count() &&
         a->usage_history().use_date() == b.usage_history().use_date();
}

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
  profile1.SetRawInfo(ADDRESS_HOME_ZIP, u"1234");
  profile1.usage_history().set_use_count(10);
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(1));
  test_adm_.AddProfile(profile1);

  AutofillProfile profile2(AddressCountryCode("US"));
  profile2.SetRawInfo(NAME_MIDDLE, u"Jay");
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"12345678910");
  profile2.usage_history().set_use_count(5);
  profile2.usage_history().set_use_date(AutofillClock::Now() - base::Days(3));
  test_adm_.AddProfile(profile2);

  AutofillProfile profile3(AddressCountryCode("US"));
  profile3.SetRawInfo(NAME_MIDDLE, u"J");
  profile3.SetRawInfo(ADDRESS_HOME_LINE1, u"742 Evergreen Terrace");
  profile3.SetRawInfo(COMPANY_NAME, u"Fox");
  profile3.usage_history().set_use_count(3);
  profile3.usage_history().set_use_date(AutofillClock::Now() - base::Days(5));
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
  // The specified phone number from profile2 should be kept (no loss of
  // information).
  EXPECT_EQ(u"12345678910",
            deduped_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The specified company name from profile3 should be kept (no loss of
  // information).
  EXPECT_EQ(u"Fox", deduped_profile.GetRawInfo(COMPANY_NAME));
  // The specified country from the imported profile should be kept (no loss of
  // information).
  EXPECT_EQ(u"US", deduped_profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of all the
  // profiles use counts.
  EXPECT_EQ(10U, deduped_profile.usage_history().use_count());
  // The use date that results from the merge should be the one from the
  // profile1 since it was the most recently used profile.
  EXPECT_LT(profile1.usage_history().use_date() - base::Seconds(10),
            deduped_profile.usage_history().use_date());
}

// Tests that ApplyDeduplicationRoutine doesn't affect profiles that shouldn't
// get deduplicated.
TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_UnrelatedProfile) {
  // Expect that the `SubsetOfStandardProfile()` is deduplicated into the
  // `StandardProfile()`, but the `DifferentFromStandardProfile()` remains
  // unaffected.
  AutofillProfile standard_profile = test::StandardProfile();
  test_adm_.AddProfile(standard_profile);
  test_adm_.AddProfile(test::SubsetOfStandardProfile());
  AutofillProfile different_profile = test::DifferentFromStandardProfile();
  test_adm_.AddProfile(different_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              UnorderedElementsAre(Pointee(standard_profile),
                                   Pointee(different_profile)));
}

TEST_F(AddressDataCleanerTest, ApplyDeduplicationRoutine_Metrics) {
  test_adm_.AddProfile(test::StandardProfile());
  test_adm_.AddProfile(test::SubsetOfStandardProfile());

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
  test_adm_.AddProfile(test::SubsetOfStandardProfile());
  // Pretend that deduplication was already run this milestone.
  prefs_->SetInteger(prefs::kAutofillLastVersionDeduped,
                     version_info::GetMajorVersionNumberAsInt());
  data_cleaner_.MaybeCleanupAddressData();
  EXPECT_EQ(2U, test_adm_.GetProfiles().size());
}

// Tests that `kAccount` profiles are not deduplicated against each other.
// TODO(crbug.com/357074792): Remove this test when the feature is cleaned up.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountPairs) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(features::kAutofillDeduplicateAccountAddresses);
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

// Tests that `kAccount` profiles are deduplicated when mergeable with either a
// different `kAccount` profile or a `kLocalOrSyncable` profile.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountExactDuplicates) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};

  AutofillProfile account_profile1 = test::StandardProfile();
  test_api(account_profile1)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile1);
  AutofillProfile account_profile2 = test::StandardProfile();
  test_api(account_profile2)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile2);
  AutofillProfile local_profile1 = test::StandardProfile();
  test_api(local_profile1)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  test_adm_.AddProfile(local_profile1);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(IsEqualForDeduplicationPurposes(),
                                          {account_profile1}));
}

// Tests that `kLocalOrSyncable` profiles which are a subset of a `kAccount`
// profile are deduplicated. The result is a Chrome account profile.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountSuperset) {
  // Create a non-Chrome account profile and a local profile.
  AutofillProfile account_profile = test::StandardProfile();
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
}

// Tests that `kLocalOrSyncable` profiles which are a subset of a `kAccount`
// profile are deduplicated.
TEST_F(AddressDataCleanerTest,
       Deduplicate_kAccountSupersetWithAccountDeduplicationEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};

  AutofillProfile account_profile = test::StandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);
  AutofillProfile local_profile = test::SubsetOfStandardProfile();
  test_api(local_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  test_adm_.AddProfile(local_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(
                  IsEqualForDeduplicationPurposes(),
                  {account_profile}));
}

// Tests that the best usage history is persistent during subset deduplication.
// The test verifies that if there are three profiles, A, B and C, where:
// A ⊆ B and A ⊆ C
// B ⊄ C and C ⊄ B
// Both B and C profiles may benefit from the usage information that A had.
TEST_F(AddressDataCleanerTest, Deduplicate_MergingSubsets) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};
  base::Time now = base::Time::Now();

  // Setup subset with count: 5 and use date: Now - 5 minutes.
  AutofillProfile subset_profile(AddressCountryCode("US"));
  subset_profile.SetInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"test@gmail.com", "en-US",
      VerificationStatus::kUserVerified);
  test_api(subset_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  subset_profile.usage_history().set_use_date(now - base::Minutes(5));
  subset_profile.usage_history().set_use_count(5);
  test_adm_.AddProfile(subset_profile);

  // Setup superset with count: 10 and use date: Now - 10 minutes.
  AutofillProfile superset_profile_1(AddressCountryCode("US"));
  superset_profile_1.SetInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"test@gmail.com", "en-US",
      VerificationStatus::kUserVerified);
  superset_profile_1.SetInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"Warsaw", "en-US", VerificationStatus::kUserVerified);
  test_api(superset_profile_1)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  superset_profile_1.usage_history().set_use_date(now - base::Minutes(10));
  superset_profile_1.usage_history().set_use_count(10);
  test_adm_.AddProfile(superset_profile_1);

  // Setup superset 2 (not mergabe with the first superset) with count: 1 and
  // use date: Now.
  AutofillProfile superset_profile_2(AddressCountryCode("US"));
  superset_profile_2.SetInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"test@gmail.com", "en-US",
      VerificationStatus::kUserVerified);
  superset_profile_2.SetInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"Munich", "en-US", VerificationStatus::kUserVerified);
  test_api(superset_profile_2)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  superset_profile_2.usage_history().set_use_date(now);
  superset_profile_2.usage_history().set_use_count(1);
  test_adm_.AddProfile(superset_profile_2);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();

  AutofillProfile expected_1 = superset_profile_1;
  expected_1.usage_history().set_use_count(10);
  expected_1.usage_history().set_use_date(now - base::Minutes(5));
  AutofillProfile expected_2 = superset_profile_2;
  expected_2.usage_history().set_use_count(5);
  expected_2.usage_history().set_use_date(now);

  // Expect that the subset was merged into both supersets and that their best
  // combined usage history attributes are kept.
  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(IsEqualForDeduplicationPurposes(),
                                          {expected_1, expected_2}));
}

// Tests that `kAccount` profiles which are a subset of a `kLocalOrSyncable`
// profile are not deduplicated.
// TODO(crbug.com/357074792): Remove this test when the feature is cleaned up.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountSubset) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(features::kAutofillDeduplicateAccountAddresses);
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

// Tests that `kAccount` profiles which are a subset of a `kLocalOrSyncable`
// profile are deduplicated.
TEST_F(AddressDataCleanerTest,
       Deduplicate_kAccountSubsetWithAccountDeduplicationEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};

  AutofillProfile local_profile = test::StandardProfile();
  test_api(local_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  test_adm_.AddProfile(local_profile);
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(
                  IsEqualForDeduplicationPurposes(),
                  {local_profile}));
}

// Tests that `kAccount` profiles which are a mergeable with a
// `kLocalOrSyncable` profile are deduplicated into the local profile.
TEST_F(AddressDataCleanerTest, Deduplicate_kAccountMerge) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};
  base::Time now = base::Time::Now();

  AutofillProfile local_profile(AddressCountryCode{"CA"});
  test::SetProfileInfo(&local_profile, "", "", "", "", "", "6543 CH BACON",
                       "APP 3", "Montreal", "QUÉBEC", "HHH999", "CA", "");
  local_profile.usage_history().set_use_date(now - base::Minutes(5));
  test_api(local_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  test_adm_.AddProfile(local_profile);

  AutofillProfile account_profile(AddressCountryCode{"CA"});
  test::SetProfileInfo(&account_profile, "", "", "", "", "", "6543, Bacon Rd",
                       "", "Montreal", "QC", "hhh 999", "CA", "+1123456789");
  account_profile.usage_history().set_use_date(now);
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  AutofillProfile expected(AddressCountryCode("CA"));
  expected.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LINE1, u"6543 CH BACON", VerificationStatus::kObserved);
  expected.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LINE2, u"APP 3",
                                            VerificationStatus::kObserved);
  expected.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Montreal",
                                            VerificationStatus::kObserved);
  expected.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"QC",
                                            VerificationStatus::kObserved);
  expected.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"hhh 999",
                                            VerificationStatus::kObserved);
  expected.SetRawInfoWithVerificationStatus(
      PHONE_HOME_WHOLE_NUMBER, u"+1123456789", VerificationStatus::kObserved);
  expected.usage_history().set_use_date(now);
  // The resulting profile should be local.
  test_api(expected).set_record_type(
      AutofillProfile::RecordType::kLocalOrSyncable);

  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(IsEqualForDeduplicationPurposes(),
                                          {expected}));
}

TEST_F(AddressDataCleanerTest, Deduplicate_kAccountNameEmailSubset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillDeduplicateAccountAddresses,
       features::kAutofillEnableSupportForNameAndEmail},
      {});

  AutofillProfile account_name_email_profile(AddressCountryCode("XX"));
  account_name_email_profile.SetInfoWithVerificationStatus(
      NAME_FULL, u"John Doe", "en-US", VerificationStatus::kUserVerified);
  account_name_email_profile.SetInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"test@gmail.com", "en-US",
      VerificationStatus::kUserVerified);
  account_name_email_profile.FinalizeAfterImport();
  test_api(account_name_email_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountNameEmail);
  test_adm_.AddProfile(account_name_email_profile);

  AutofillProfile superset_profile = test::StandardProfile();
  superset_profile.SetInfoWithVerificationStatus(
      NAME_FULL, u"John Doe", "en-US", VerificationStatus::kUserVerified);
  superset_profile.SetInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"test@gmail.com", "en-US",
      VerificationStatus::kUserVerified);
  superset_profile.FinalizeAfterImport();
  test_api(superset_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(superset_profile);

  test_api(data_cleaner_).ApplyDeduplicationRoutine();
  EXPECT_THAT(test_adm_.GetProfiles(),
              testing::UnorderedPointwise(IsEqualForDeduplicationPurposes(),
                                          {superset_profile}));
}

// TODO(crbug.com/357074792): This test is temporarily disabled. For the rollout
// of kAutofillDeduplicateAccountAddresses, deduplication is exceptionally run
// a second time per milestone, so metric changes can be observed without
// waiting one milestone per channel. During the cleanup of the feature, this
// logic will be removed and this test can be re-enabled.
TEST_F(AddressDataCleanerTest, DeduplicateOncePerMilestone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillDeduplicateAccountAddresses);
  MockAddressDataCleaner data_cleaner(
      test_adm_, /*sync_service=*/nullptr, *prefs_,
      /*alternative_state_name_map_updater=*/nullptr);

  // Deduplication should run once per milestone by default without the feature.
  EXPECT_CALL(data_cleaner, ApplyDeduplicationRoutine);
  data_cleaner.MaybeCleanupAddressData();

  // Deduplication is not called again.
  test_api(data_cleaner).ResetAreCleanupsPending();
  EXPECT_CALL(data_cleaner, ApplyDeduplicationRoutine).Times(0);
  data_cleaner.MaybeCleanupAddressData();
}

// Tests that when `kAutofillDeduplicateAccountAddresses` is enabled, the
// deduplication routine is run a second time per milestone for enrolled users.
TEST_F(AddressDataCleanerTest,
       Deduplicate_SecondTimeAccountDeduplicationEnabled) {
  // Enroll the user in the feature. This enables a second deduplication run,
  // but not a third one.
  base::test::ScopedFeatureList feature(
      features::kAutofillDeduplicateAccountAddresses);
  MockAddressDataCleaner data_cleaner(
      test_adm_, /*sync_service=*/nullptr, *prefs_,
      /*alternative_state_name_map_updater=*/nullptr);

  // The first two calls to MaybeCleanupAddressData() should run deduplication.
  EXPECT_CALL(data_cleaner, ApplyDeduplicationRoutine);
  data_cleaner.MaybeCleanupAddressData();
  test_api(data_cleaner).ResetAreCleanupsPending();
  EXPECT_CALL(data_cleaner, ApplyDeduplicationRoutine);
  data_cleaner.MaybeCleanupAddressData();
  test_api(data_cleaner).ResetAreCleanupsPending();

  // #3rd time, deduplication is not called again.
  EXPECT_CALL(data_cleaner, ApplyDeduplicationRoutine).Times(0);
  data_cleaner.MaybeCleanupAddressData();
}

TEST_F(AddressDataCleanerTest, DeleteDisusedAddresses) {
  // TODO(crbug.com/357074792): Merge this test with the one below once the
  // feature is launched.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillDeduplicateAccountAddresses);

  // Create a disused address (deletable).
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(400));
  test_adm_.AddProfile(profile1);

  // Create a recently-used address (not deletable).
  AutofillProfile profile2 = test::GetFullCanadianProfile();
  profile2.usage_history().set_use_date(AutofillClock::Now() - base::Days(4));
  test_adm_.AddProfile(profile2);

  // Create a disused account address (not deletable because the feature is
  // disabled).
  AutofillProfile account_profile = test::GetFullProfile2();
  account_profile.usage_history().set_use_date(AutofillClock::Now() -
                                               base::Days(400));
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(account_profile);

  test_api(data_cleaner_).DeleteDisusedAddresses();
  EXPECT_THAT(
      test_adm_.GetProfiles(),
      UnorderedElementsAre(Pointee(profile2), Pointee(account_profile)));
}

TEST_F(AddressDataCleanerTest, DeleteDisusedAccountAddresses) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillDeduplicateAccountAddresses};

  // Create a disused account address (deletable).
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(400));
  test_api(profile1)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_adm_.AddProfile(profile1);

  // Create a recently-used account address (not deletable).
  AutofillProfile profile2 = test::GetFullCanadianProfile();
  profile2.usage_history().set_use_date(AutofillClock::Now() - base::Days(4));
  test_api(profile2)
      .set_record_type(AutofillProfile::RecordType::kAccount);
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

// Checks that migration of phonetic names from regular name fields, does not
// run if the feature is disabled.
TEST_F(AddressDataCleanerTest, NoNameMigrationIfFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillSupportPhoneticNameForJP);
  base::HistogramTester histogram_tester;

  // Creating a profile.
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetRawInfo(NAME_FULL, u"タ ワ");
  profile.FinalizeAfterImport();
  test_adm_.AddProfile(profile);

  data_cleaner_.MaybeCleanupAddressData();
  histogram_tester.ExpectTotalCount(
      "Autofill.NumberOfNamesMigratedToAlternativeNamesDuringCleanUp", 0);
  EXPECT_THAT(test_adm_.GetProfiles(), UnorderedElementsAre(Pointee(profile)));
}

// Checks that migration of phonetic names from regular name fields,
// records the metric and migrates the name.
TEST_F(AddressDataCleanerTest, NameMigration) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  base::HistogramTester histogram_tester;

  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetRawInfo(NAME_FULL, u"タ ワ");
  profile.FinalizeAfterImport();
  test_adm_.AddProfile(profile);

  AutofillProfile expected(AddressCountryCode("JP"));
  expected.set_guid(profile.guid());
  expected.SetRawInfoWithVerificationStatus(ALTERNATIVE_FULL_NAME, u"タ ワ",
                                            VerificationStatus::kNoStatus);
  expected.SetRawInfoWithVerificationStatus(ALTERNATIVE_FAMILY_NAME, u"タ",
                                            VerificationStatus::kNoStatus);
  expected.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"ワ",
                                            VerificationStatus::kNoStatus);

  data_cleaner_.MaybeCleanupAddressData();
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfNamesMigratedToAlternativeNamesDuringCleanUp", 1, 1);
  EXPECT_THAT(test_adm_.GetProfiles(), UnorderedElementsAre(Pointee(expected)));
}

}  // namespace
}  // namespace autofill
