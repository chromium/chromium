// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class EntityInstanceCleanerTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    entity_data_manager_ = std::make_unique<EntityDataManager>(
        &pref_service(), /*identity_manager=*/nullptr,
        /*sync_service=*/&sync_service(),
        webdata_helper_.autofill_webdata_service(),
        /*history_service=*/nullptr,
        /*strike_database=*/nullptr);
    cleaner_ = std::make_unique<EntityInstanceCleaner>(
        entity_data_manager_.get(), &sync_service(), &pref_service());
  }

  syncer::TestSyncService& sync_service() { return sync_service_; }

  sync_preferences::TestingPrefServiceSyncable& pref_service() {
    return *pref_service_;
  }
  EntityDataManager& entity_data_manager() { return *entity_data_manager_; }

  EntityInstanceCleaner& cleaner() { return *cleaner_; }
  AutofillWebDataServiceTestHelper* webdata_helper() {
    return &webdata_helper_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiDedupeEntities};
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  syncer::TestSyncService sync_service_;
  std::unique_ptr<EntityDataManager> entity_data_manager_;
  std::unique_ptr<EntityInstanceCleaner> cleaner_;
};

TEST_F(EntityInstanceCleanerTest, DeduplicationNotRunIfMilestoneIsTheSame) {
  int current_milestone = version_info::GetMajorVersionNumberAsInt();
  pref_service().SetInteger(prefs::kAutofillAiLastVersionDeduped,
                            current_milestone);
  sync_service().FireStateChanged();
  // No deduplication should have happened, so the pref should be the same.
  EXPECT_EQ(pref_service().GetInteger(prefs::kAutofillAiLastVersionDeduped),
            current_milestone);
}

TEST_F(EntityInstanceCleanerTest, DeduplicationRunIfMilestoneIsDifferent) {
  int current_milestone = version_info::GetMajorVersionNumberAsInt();
  pref_service().SetInteger(prefs::kAutofillAiLastVersionDeduped,
                            current_milestone - 1);
  sync_service().FireStateChanged();
  // Deduplication should have happened, so the pref should be updated.
  EXPECT_EQ(pref_service().GetInteger(prefs::kAutofillAiLastVersionDeduped),
            current_milestone);
}

TEST_F(EntityInstanceCleanerTest, DuplicatedLocalEntitiesAreRemoved) {
  EntityInstance entity1 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance entity2 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance entity3 = test::GetPassportEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  entity_data_manager().AddOrUpdateEntityInstance(entity1);
  entity_data_manager().AddOrUpdateEntityInstance(entity2);
  entity_data_manager().AddOrUpdateEntityInstance(entity3);
  webdata_helper()->WaitUntilIdle();
  ASSERT_EQ(entity_data_manager().GetEntityInstances().size(), 3u);

  base::HistogramTester histogram_tester;
  sync_service().FireStateChanged();
  webdata_helper()->WaitUntilIdle();
  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();

  EXPECT_THAT(instances.size(), 1u);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.AllEntities",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.Passport", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.Passport", 2, 1);
}

TEST_F(EntityInstanceCleanerTest, EntityThatIsSubsetOfAnotherIsRemoved) {
  EntityInstance entity1 =
      test::GetPassportEntityInstanceWithRandomGuid({.expiry_date = nullptr});
  EntityInstance entity2 = test::GetPassportEntityInstanceWithRandomGuid();
  entity_data_manager().AddOrUpdateEntityInstance(entity1);
  entity_data_manager().AddOrUpdateEntityInstance(entity2);
  webdata_helper()->WaitUntilIdle();
  ASSERT_EQ(entity_data_manager().GetEntityInstances().size(), 2u);

  base::HistogramTester histogram_tester;
  sync_service().FireStateChanged();
  webdata_helper()->WaitUntilIdle();

  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();
  EXPECT_THAT(instances.size(), 1u);
  EXPECT_EQ(instances[0].guid(), entity2.guid());
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.AllEntities",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.AllEntities", 1,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.Passport", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.Passport", 1, 1);
}

TEST_F(EntityInstanceCleanerTest, DifferentEntities_NoneIsRemoved) {
  EntityInstance entity1 =
      test::GetPassportEntityInstanceWithRandomGuid({.name = u"Jon snow"});
  EntityInstance entity2 =
      test::GetPassportEntityInstanceWithRandomGuid({.name = u"Sansa"});
  entity_data_manager().AddOrUpdateEntityInstance(entity1);
  entity_data_manager().AddOrUpdateEntityInstance(entity2);
  webdata_helper()->WaitUntilIdle();
  ASSERT_EQ(entity_data_manager().GetEntityInstances().size(), 2u);

  base::HistogramTester histogram_tester;
  sync_service().FireStateChanged();
  webdata_helper()->WaitUntilIdle();

  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();
  EXPECT_THAT(instances.size(), 2u);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.AllEntities",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.AllEntities", 0,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.Passport", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.Passport", 0, 1);
}

TEST_F(EntityInstanceCleanerTest,
       DuplicatedLocalEntities_FeatureOff_NotRemoved) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiDedupeEntities);
  EntityInstance entity1 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance entity2 = test::GetPassportEntityInstanceWithRandomGuid();
  EntityInstance entity3 = test::GetPassportEntityInstanceWithRandomGuid(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  entity_data_manager().AddOrUpdateEntityInstance(entity1);
  entity_data_manager().AddOrUpdateEntityInstance(entity2);
  entity_data_manager().AddOrUpdateEntityInstance(entity3);
  webdata_helper()->WaitUntilIdle();
  ASSERT_EQ(entity_data_manager().GetEntityInstances().size(), 3u);

  base::HistogramTester histogram_tester;
  sync_service().FireStateChanged();
  webdata_helper()->WaitUntilIdle();
  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();

  EXPECT_THAT(instances.size(), 3u);
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.AllEntities",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.AllEntities", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered.Passport", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped.Passport", 0);
}

}  // namespace autofill
