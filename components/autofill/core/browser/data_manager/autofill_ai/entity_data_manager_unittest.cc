// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::Bucket;
using base::BucketsAre;
using ::testing::AtLeast;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class MockEntityDataManagerObserver : public EntityDataManager::Observer {
 public:
  MockEntityDataManagerObserver() = default;
  MockEntityDataManagerObserver(const MockEntityDataManagerObserver&) = delete;
  MockEntityDataManagerObserver& operator=(
      const MockEntityDataManagerObserver&) = delete;
  ~MockEntityDataManagerObserver() override = default;

  MOCK_METHOD(void, OnEntityInstancesChanged, (), (override));
};

// Test fixture for the asynchronous database operations in EntityDataManager.
class EntityDataManagerTest : public testing::Test {
 public:
  EntityDataManagerTest() = default;

  void TearDown() override { sync_service_.Shutdown(); }

  AutofillWebDataServiceTestHelper& helper() { return helper_; }

  TestAutofillClient& client() { return client_; }

  syncer::TestSyncService& sync_service() { return sync_service_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  TestAutofillClient client_;
  syncer::TestSyncService sync_service_;
};

// Tests that the constructor of EntityDataManager queries the database.
TEST_F(EntityDataManagerTest, InitialPopulation) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  EntityInstance fr = test::GetFlightReservationEntityInstance();

  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      pp, base::DoNothing());
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      dl, base::DoNothing());
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      fr, base::DoNothing());
  helper().WaitUntilIdle();

  EntityDataManager entity_data_manager(client().GetPrefs(),
                                        /*identity_manager=*/nullptr,
                                        &sync_service(),
                                        helper().autofill_webdata_service(),
                                        /*history_service=*/nullptr,
                                        /*strike_database=*/nullptr);
  EXPECT_THAT(entity_data_manager.GetEntityInstances(), IsEmpty());

  helper().WaitUntilIdle();
  EXPECT_THAT(entity_data_manager.GetEntityInstances(),
              UnorderedElementsAre(pp, dl, fr));
}

// Tests that the constructor of EntityDataManager queries the database.
TEST_F(EntityDataManagerTest, StorageMetrics) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance vehicle = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      passport, base::DoNothing());
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      vehicle, base::DoNothing());
  helper().WaitUntilIdle();

  base::HistogramTester histogram_tester;
  EntityDataManager entity_data_manager(client().GetPrefs(),
                                        /*identity_manager=*/nullptr,
                                        &sync_service(),
                                        helper().autofill_webdata_service(),
                                        /*history_service=*/nullptr,
                                        /*strike_database=*/nullptr);
  helper().WaitUntilIdle();
  EXPECT_THAT(entity_data_manager.GetEntityInstances(),
              UnorderedElementsAre(passport, vehicle));

  // Metrics should correctly reflect that the user has one local passport and
  // one Wallet vehicle stored.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport.Local", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport.ServerWallet", 0, 1);

  histogram_tester.ExpectUniqueSample("Autofill.Ai.StoredEntitiesCount.Vehicle",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Vehicle.Local", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Vehicle.ServerWallet", 1, 1);
}

// Tests the emission of opt-in metrics that are emitted on EDM creation, i.e.
// on profile startup.
// TODO(crbug.com/445879337): Fix Linux MSan Test failure and re-enable the
// test.
TEST_F(EntityDataManagerTest, OptInMetric) {
  ASSERT_FALSE(GetAutofillAiOptInStatus(client()));
  base::HistogramTester histogram_tester;
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Ai.OptIn.Status.Startup"),
      BucketsAre(Bucket(0, 1)));

  client().SetUpPrefsAndIdentityForAutofillAi();
  ASSERT_TRUE(GetAutofillAiOptInStatus(client()));

  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Ai.OptIn.Status.Startup"),
      BucketsAre(Bucket(0, 1), Bucket(1, 1)));
}

// Test fixture that starts with an empty database.
class EntityDataManagerTest_InitiallyEmpty : public EntityDataManagerTest {
 public:
  EntityDataManagerTest_InitiallyEmpty()
      : entity_data_manager_(client().GetPrefs(),
                             /*identity_manager=*/nullptr,
                             &sync_service(),
                             helper().autofill_webdata_service(),
                             /*history_service=*/nullptr,
                             /*strike_database=*/nullptr) {}

  EntityDataManager& entity_data_manager() { return entity_data_manager_; }

  base::span<const autofill::EntityInstance> GetEntityInstances() {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstances();
  }

  base::optional_ref<const EntityInstance> GetInstance(
      const EntityInstance::EntityId& guid) {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstance(guid);
  }

 private:
  EntityDataManager entity_data_manager_;
};

// Tests that AddOrUpdateEntityInstance() asynchronously adds entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, AddEntityInstance) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  EntityInstance fr = test::GetFlightReservationEntityInstance();
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(AtLeast(3));
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  entity_data_manager().AddOrUpdateEntityInstance(fr);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl, fr));
}

// Tests that AddOrUpdateEntityInstance() correctly adds entities with an id
// that's not formatted as GUID.
TEST_F(EntityDataManagerTest_InitiallyEmpty, AddEntityInstanceNonGuidFormatId) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance vr = test::GetVehicleEntityInstanceWithRandomGuid(
      {.guid = "non-guid-format",
       .record_type = EntityInstance::RecordType::kServerWallet});
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(AtLeast(1));
  entity_data_manager().AddOrUpdateEntityInstance(vr);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(vr));
}

// Tests that recording an entity being used calls for a database entity update.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RecordEntityUsed) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp =
      test::GetPassportEntityInstance({.use_date = base::Time::FromTimeT(0)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  EXPECT_EQ(pp.use_count(), 0u);
  EXPECT_EQ(pp.use_date(), base::Time::FromTimeT(0));

  base::Time use_date = base::Time::Now();
  helper().WaitUntilIdle();
  entity_data_manager().RecordEntityUsed(pp.guid(), use_date);

  auto check_metadata = [&](const EntityInstance::EntityId& guid) {
    base::optional_ref<const EntityInstance> entity = GetInstance(guid);
    ASSERT_TRUE(entity);
    EXPECT_EQ(entity->use_count(), 1u);
    EXPECT_EQ(entity->use_date(), use_date);
  };
  check_metadata(pp.guid());

  // Re-read entities from the DB.
  EXPECT_CALL(observer, OnEntityInstancesChanged);
  helper().autofill_webdata_service()->GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_VALUABLE);
      }));
  helper().WaitUntilIdle();
  check_metadata(pp.guid());
}

// Tests that AddOrUpdateEntityInstance() asynchronously updates entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, UpdateEntityInstance) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(AtLeast(2));
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp));

  pp = test::GetPassportEntityInstance(
      {.name = u"Karlsson", .date_modified = test::kJune2017 - base::Days(1)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(pp));
}

// Tests that RemoveEntityInstance() asynchronously removes entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RemoveEntityInstance) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(AtLeast(3));
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl));

  entity_data_manager().RemoveEntityInstance(pp.guid());
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(dl));
}

// Tests that removing a non-existing entity is a no-op.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RemoveEntityInstance_NonExisting) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl));

  entity_data_manager().RemoveEntityInstance(pp.guid());
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(dl));
  entity_data_manager().RemoveEntityInstance(pp.guid());  // No-op.
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(dl));
}

// Tests that removing entities in a date range updates the cache.
TEST_F(EntityDataManagerTest_InitiallyEmpty,
       RemoveEntityInstancesModifiedBetween) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(1)});
  EntityInstance dl = test::GetDriversLicenseEntityInstance(
      {.date_modified = test::kJune2017 + base::Days(1)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl));

  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(AtLeast(1));
  entity_data_manager().RemoveEntityInstancesModifiedBetween(
      test::kJune2017 - base::Days(1), test::kJune2017);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(dl));

  entity_data_manager().RemoveEntityInstancesModifiedBetween(
      test::kJune2017, test::kJune2017 + base::Days(2) + base::Seconds(1));
  EXPECT_THAT(GetEntityInstances(), IsEmpty());
}

// Tests that entities can be retrieved by GUID.
TEST_F(EntityDataManagerTest_InitiallyEmpty, GetEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl));

  EXPECT_THAT(entity_data_manager().GetEntityInstance(pp.guid()), Optional(pp));
  EXPECT_EQ(entity_data_manager().GetEntityInstance(
                EntityInstance::EntityId(base::Uuid::GenerateRandomV4())),
            std::nullopt);
}

// Tests that a change notification for AUTOFILL_VALUABLE from sync triggers a
// reload of entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, OnAutofillValuableChangedBySync) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  // 1. Add an entity directly to the DB to simulate a sync change.
  EntityInstance vh = test::GetVehicleEntityInstance();
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      vh, base::DoNothing());

  // The EDM's cache is not updated yet.
  EXPECT_THAT(GetEntityInstances(), IsEmpty());

  // 2. Trigger the sync notification.
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(1);
  helper().autofill_webdata_service()->GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_VALUABLE);
      }));
  helper().WaitUntilIdle();
  // 3. Verify that the cache is reloaded.
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(vh));
}

// Tests that a change notification for other data types does not trigger a
// reload.
TEST_F(EntityDataManagerTest_InitiallyEmpty, OnOtherDataTypeChangedBySync) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  // 1. Add an entity directly to the DB.
  EntityInstance vh = test::GetVehicleEntityInstance();
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      vh, base::DoNothing());

  // The EDM's cache is not updated yet.
  EXPECT_THAT(GetEntityInstances(), IsEmpty());

  // 2. Trigger the sync notification for a different data type.
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(0);
  helper().autofill_webdata_service()->GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_PROFILE);
      }));
  helper().WaitUntilIdle();
  // 3. Verify that the cache is NOT reloaded.
  EXPECT_THAT(GetEntityInstances(), IsEmpty());
}

// Tests that a change notification for AUTOFILL_VALUABLE_METADATA from sync
// triggers a reload of entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty,
       OnAutofillValuableMetadataChangedBySync) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  // 1. Add an entity directly to the DB to simulate a sync change.
  EntityInstance vh = test::GetVehicleEntityInstance();
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      vh, base::DoNothing());

  // The EDM's cache is not updated yet.
  EXPECT_THAT(GetEntityInstances(), IsEmpty());

  // 2. Trigger the sync notification.
  EXPECT_CALL(observer, OnEntityInstancesChanged);
  helper().autofill_webdata_service()->GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_VALUABLE_METADATA);
      }));
  helper().WaitUntilIdle();
  // 3. Verify that the cache is reloaded.
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(vh));
}

// Tests that the syncable pref is correctly migrated from the account keyed
// pref.
TEST_F(EntityDataManagerTest,
       SyncablePrefIsOffAndAccountKeyPrefIsOn_MigratePrefValue) {
  base::HistogramTester histogram_tester;
  // Opt the user in.
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  ASSERT_TRUE(client().SetUpPrefsAndIdentityForAutofillAi());
  ASSERT_TRUE(GetAutofillAiOptInStatus(client()));

  // This emulates the user being added to the experiment and restarting chrome
  // (the migration happens at startup).
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiSetSyncablePrefFromAccountPref};
  // Recreate the entity data manager the trigger possible pref migration.
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  EXPECT_TRUE(prefs::IsAutofillAiSyncedOptInStatusEnabled(client().GetPrefs()));
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.OptIn.PrefMigration",
      EntityDataManager::AutofillAiPrefMigrationStatus::kPrefMigratedEnabled,
      1);
}

// Tests that no migration happens if the syncable pref is already enabled.
TEST_F(EntityDataManagerTest, SyncablePrefIsOn_DoNotMigrate) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiSetSyncablePrefFromAccountPref};

  base::HistogramTester histogram_tester;
  // At first the user is not opted-in, therefore no migration happens.
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));

  // Opt the user in, which also enables the syncable pref.
  ASSERT_TRUE(client().SetUpPrefsAndIdentityForAutofillAi());

  // Recreate the entity data manager the trigger possible pref migration.
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  // The first construction of the `EntityDataManager` triggered no migration
  // because the user was not opted-in.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.OptIn.PrefMigration",
      EntityDataManager::AutofillAiPrefMigrationStatus::
          kPrefNotMigratedAccountPrefNeverSet,
      1);
  // The first construction of the `EntityDataManager` triggered no migration
  // because the synced pref has already been set.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.OptIn.PrefMigration",
      EntityDataManager::AutofillAiPrefMigrationStatus::
          kPrefNotMigratedAlreadySet,
      1);
}

TEST_F(
    EntityDataManagerTest,
    SyncablePrefIsOffAndAccountKeyPrefIsOn_FeatureOff_DoNotMigratePrefValue) {
  base::HistogramTester histogram_tester;
  client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      client().GetPrefs(), client().GetIdentityManager(), &sync_service(),
      helper().autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*strike_database=*/nullptr));
  histogram_tester.ExpectTotalCount("Autofill.Ai.OptIn.PrefMigration", 0);
}

}  // namespace
}  // namespace autofill
