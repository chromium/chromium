// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/home_and_work_metadata_store.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::Bucket;

class HomeAndWorkMetadataStoreTest : public testing::Test {
 public:
  HomeAndWorkMetadataStoreTest() : prefs_(test::PrefServiceForTesting()) {}

  PrefService* pref_service() { return prefs_.get(); }

  syncer::TestSyncService* sync_service() { return &sync_service_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableSupportForHomeAndWork};
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
};

// Tests that any Home and Work metadata persisted with an update
// `ApplyChange()` is restored by `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Update_HomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  profile.usage_history().set_use_count(5);
  profile.usage_history().set_use_date(base::Time::Now() - base::Minutes(3));

  EXPECT_CALL(on_change, Run());
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::UPDATE, profile.guid(), profile));

  AutofillProfile modified_profile = profile;
  modified_profile.usage_history().set_use_count(123);
  modified_profile.usage_history().set_use_date(base::Time::Now() -
                                                base::Minutes(345));
  EXPECT_THAT(metadata_store.ApplyMetadata(
                  std::vector<AutofillProfile>{modified_profile},
                  /*is_initial_load=*/false),
              testing::ElementsAre(profile));
}

// Tests that non Home and Work addresses are not affected by
// `ApplyChange()` and `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Update_NonHomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  profile.usage_history().set_use_count(5);
  profile.usage_history().set_use_date(base::Time::Now() - base::Minutes(3));

  EXPECT_CALL(on_change, Run()).Times(0);
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::UPDATE, profile.guid(), profile));

  AutofillProfile modified_profile = profile;
  modified_profile.usage_history().set_use_count(123);
  modified_profile.usage_history().set_use_date(base::Time::Now() -
                                                base::Minutes(345));
  EXPECT_THAT(metadata_store.ApplyMetadata(
                  std::vector<AutofillProfile>{modified_profile},
                  /*is_initial_load=*/false),
              testing::ElementsAre(modified_profile));
}

// Tests that after a HIDE_IN_AUTOFILL change, `ApplyMetadata()` removes Home
// and Work profiles.
TEST_F(HomeAndWorkMetadataStoreTest, Remove_HomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  EXPECT_CALL(on_change, Run());
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::HIDE_IN_AUTOFILL, profile.guid(), profile));

  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile},
                                   /*is_initial_load=*/false),
      testing::IsEmpty());

  // Once the modification date increases, the address reappears.
  profile.usage_history().set_modification_date(base::Time::Now() +
                                                base::Minutes(1));
  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile},
                                   /*is_initial_load=*/false),
      testing::ElementsAre(profile));
}

// Tests that during the first load, it is emitted whether users with a H/W
// address chose to remove it.
TEST_F(HomeAndWorkMetadataStoreTest, Remove_HomeAndWork_Metric) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  AutofillProfile home = test::GetFullProfile();
  test_api(home).set_record_type(AutofillProfile::RecordType::kAccountHome);
  AutofillProfile work = test::GetFullProfile();
  test_api(work).set_record_type(AutofillProfile::RecordType::kAccountWork);

  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::HIDE_IN_AUTOFILL, home.guid(), home));

  base::HistogramTester histogram_tester;
  ASSERT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{home, work},
                                   /*is_initial_load=*/true),
      testing::ElementsAre(work));
  histogram_tester.ExpectUniqueSample(
      "Autofill.HomeAndWork.RemovedFromChrome.Home", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.HomeAndWork.RemovedFromChrome.Work", false, 1);
}

// Tests that non Home and Work addresses are not affected by
// `ApplyChange()` and `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Remove_NonHomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(on_change, Run()).Times(0);
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::HIDE_IN_AUTOFILL, profile.guid(), profile));

  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile},
                                   /*is_initial_load=*/false),
      testing::ElementsAre(profile));
}

// Tests that observers are notified when a pref is changed from outside the
// class, e.g. through sync.
TEST_F(HomeAndWorkMetadataStoreTest, MetadataChangeThroughSync) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          on_change.Get());
  EXPECT_CALL(on_change, Run());
  pref_service()->SetDict(prefs::kAutofillHomeMetadata, base::DictValue());
}

// Tests that for H/W addresses, metadata is default initialized to boost it
// above other addresses in terms of frecency.
TEST_F(HomeAndWorkMetadataStoreTest, DefaultValues) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  AutofillProfile home = test::GetFullProfile();
  test_api(home).set_record_type(AutofillProfile::RecordType::kAccountHome);
  AutofillProfile work = test::GetFullProfile();
  test_api(work).set_record_type(AutofillProfile::RecordType::kAccountWork);
  AutofillProfile other = test::GetFullCanadianProfile();
  other.usage_history().set_use_count(123);
  task_environment().FastForwardBy(base::Minutes(2));

  std::vector<AutofillProfile> profiles = metadata_store.ApplyMetadata(
      {home, work, other}, /*is_initial_load=*/false);
  // Note that `AutofillProfile::operator==` doesn't compare usage information.
  ASSERT_THAT(profiles, testing::ElementsAre(home, work, other));
  base::Time comparison_time = base::Time::Now();
  EXPECT_TRUE(profiles[0].HasGreaterRankingThan(&profiles[1], comparison_time));
  EXPECT_TRUE(profiles[1].HasGreaterRankingThan(&profiles[2], comparison_time));

  // Further calls of `ApplyMetadata()` shouldn't boost H/W. By using the
  // `other` addresses sufficiently often, it becomes the top ranked address.
  other.usage_history().set_use_count(321);
  profiles = metadata_store.ApplyMetadata({home, work, other},
                                          /*is_initial_load=*/false);
  ASSERT_THAT(profiles, testing::ElementsAre(home, work, other));
  EXPECT_TRUE(profiles[0].HasGreaterRankingThan(&profiles[1], comparison_time));
  EXPECT_TRUE(profiles[2].HasGreaterRankingThan(&profiles[0], comparison_time));
}

// Tests that prefs are cleared when sync is disabled, so their values don't
// leak into other accounts.
TEST_F(HomeAndWorkMetadataStoreTest, ClearPrefs) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  // Store some metadata.
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::UPDATE, profile.guid(), profile));
  ASSERT_FALSE(pref_service()->GetDict(prefs::kAutofillHomeMetadata).empty());

  // Disable sync and expect that the metadata is cleared.
  sync_service()->SetSignedOut();
  sync_service()->FireStateChanged();
  EXPECT_TRUE(pref_service()->GetDict(prefs::kAutofillHomeMetadata).empty());
}

// Tests that Autofill.HomeAndWork.SilentUpdates.Performed is emitted whenever
// silent updates are performed.
TEST_F(HomeAndWorkMetadataStoreTest, SilentUpdatesPerformed) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  // Simulate two silent updates.
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  base::HistogramTester histogram_tester;
  metadata_store.RecordSilentUpdate(profile);
  metadata_store.RecordSilentUpdate(profile);

  // Expect two samples, indicating that the same address was updated twice.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.HomeAndWork.SilentUpdates.Performed"),
              BucketsAre(Bucket(1, 1), Bucket(2, 1)));
}

// Tests that Autofill.HomeAndWork.SilentUpdates.Lost is emitted on sign-out.
TEST_F(HomeAndWorkMetadataStoreTest, SilentUpdatesLost) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  AutofillProfile home = test::GetFullProfile();
  test_api(home).set_record_type(AutofillProfile::RecordType::kAccountHome);
  AutofillProfile work = test::GetFullProfile();
  test_api(work).set_record_type(AutofillProfile::RecordType::kAccountWork);
  metadata_store.RecordSilentUpdate(home);
  metadata_store.RecordSilentUpdate(work);

  base::HistogramTester histogram_tester;
  sync_service()->SetSignedOut();
  sync_service()->FireStateChanged();
  histogram_tester.ExpectUniqueSample("Autofill.HomeAndWork.SilentUpdates.Lost",
                                      2, 1);
}

// Tests that Autofill.HomeAndWork.SilentUpdates.Usage is emitted correctly.
TEST_F(HomeAndWorkMetadataStoreTest, SilentUpdateUsage) {
  HomeAndWorkMetadataStore metadata_store(pref_service(), sync_service(),
                                          base::DoNothing());
  AutofillProfile home = test::GetFullProfile();
  test_api(home).set_record_type(AutofillProfile::RecordType::kAccountHome);
  AutofillProfile work = test::GetFullProfile();
  test_api(work).set_record_type(AutofillProfile::RecordType::kAccountWork);
  metadata_store.RecordSilentUpdate(home);

  base::HistogramTester histogram_tester;
  metadata_store.RecordProfileFill(home);
  histogram_tester.ExpectUniqueSample(
      "Autofill.HomeAndWork.SilentUpdates.Usage", true, 1);
  metadata_store.RecordProfileFill(work);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.HomeAndWork.SilentUpdates.Usage"),
              BucketsAre(Bucket(true, 1), Bucket(false, 1)));
}

}  // namespace

}  // namespace autofill
