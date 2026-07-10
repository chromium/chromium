// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/network/autofill_ai/mock_autofill_ai_personal_context_access_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
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
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
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
//
// Do not use this fixture directly. To keep some order in the test, use
// EntityDataManagerTest_InitiallyEmpty or
// EntityDataManagerTest_InitiallyPopulated or create a new subclass.
class EntityDataManagerTestBase : public testing::Test {
 public:
  EntityDataManagerTestBase() = default;

  void SetUp() override {
    PopulateDatabase(*helper().autofill_webdata_service());
    helper().WaitUntilIdle();
    client_ = std::make_unique<TestAutofillClient>();
    client_->set_entity_data_manager(BuildEntityDataManager());
  }

  void TearDown() override {
    client_.reset();
    sync_service_.Shutdown();
  }

  virtual void PopulateDatabase(AutofillWebDataService& db) = 0;

  AutofillWebDataServiceTestHelper& helper() { return helper_; }

  TestAutofillClient& client() { return *client_; }

  syncer::TestSyncService& sync_service() { return sync_service_; }


  EntityDataManager& entity_data_manager() {
    return *client().GetEntityDataManager();
  }

  MockAutofillAiPersonalContextAccessManager& pcontext_manager() {
    return pcontext_manager_;
  }

  base::span<const EntityInstance> GetEntityInstances() {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstances();
  }

  base::optional_ref<const EntityInstance> GetEntityInstance(
      const EntityInstance::EntityId& guid) {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstance(guid);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  // Recreating the EntityDataManager in the middle of the test is discouraged
  // because this prevents any other code from keeping a pointer to the
  // EntityDataManager.
  void RecreateEntityDataManager_Discouraged() {
    client_->set_entity_data_manager(BuildEntityDataManager());
  }

 private:
  std::unique_ptr<EntityDataManager> BuildEntityDataManager() {
    return std::make_unique<EntityDataManager>(
        client_->GetPrefs(), client_->GetIdentityManager(), &sync_service_,
        helper_.autofill_webdata_service(),
        /*history_service=*/nullptr, &pcontext_manager_,
        /*strike_database=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US"));
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  testing::NiceMock<MockAutofillAiPersonalContextAccessManager>
      pcontext_manager_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillClient> client_;
};

class EntityDataManagerTest_InitiallyEmpty : public EntityDataManagerTestBase {
 public:
  void PopulateDatabase(AutofillWebDataService& db) override {}
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
  EXPECT_EQ(pp.use_count(), 0);
  EXPECT_EQ(pp.use_date(), base::Time::FromTimeT(0));

  base::Time use_date = base::Time::Now();
  helper().WaitUntilIdle();
  entity_data_manager().RecordEntityUsed(pp.guid(), use_date);

  auto check_metadata = [&](const EntityInstance::EntityId& guid) {
    base::optional_ref<const EntityInstance> entity = GetEntityInstance(guid);
    ASSERT_TRUE(entity);
    EXPECT_EQ(entity->use_count(), 1);
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

// Tests that when the re-auth becomes unavailable, Wallet private passes are
// removed.
TEST_F(EntityDataManagerTest_InitiallyEmpty, ValidateEntityReauthRequirements) {
  EntityInstance local_private_pass = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance wallet_private_pass =
      test::MaskEntityInstance(test::GetDriversLicenseEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  EntityInstance public_pass = test::GetVehicleEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(local_private_pass);
  entity_data_manager().AddOrUpdateEntityInstance(wallet_private_pass);
  entity_data_manager().AddOrUpdateEntityInstance(public_pass);
  ASSERT_THAT(GetEntityInstances(),
              UnorderedElementsAre(local_private_pass, wallet_private_pass,
                                   public_pass));
  entity_data_manager().SetReauthAvailability(false);
  helper().WaitUntilIdle();
  ASSERT_THAT(GetEntityInstances(),
              UnorderedElementsAre(local_private_pass, public_pass));
}

class EntityDataManagerTest_InitiallyPopulated
    : public EntityDataManagerTestBase {
 public:
  void PopulateDatabase(AutofillWebDataService& db) override {
    db.AddOrUpdateEntityInstance(passport_, base::DoNothing());
    db.AddOrUpdateEntityInstance(vehicle_, base::DoNothing());
  }

  const EntityInstance& passport() const { return passport_; }
  const EntityInstance& vehicle() const { return vehicle_; }

 private:
  EntityInstance passport_ = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance vehicle_ = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
};

// Tests that the constructor of EntityDataManager queries the database.
TEST_F(EntityDataManagerTest_InitiallyPopulated, StorageMetrics) {
  helper().WaitUntilIdle();
  EXPECT_THAT(GetEntityInstances(),
              UnorderedElementsAre(passport(), vehicle()));

  // Metrics should correctly reflect that the user has one local passport and
  // one Wallet vehicle stored.
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport.Local", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Passport.ServerWallet", 0, 1);

  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Vehicle", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Vehicle.Local", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.StoredEntitiesCount.Vehicle.ServerWallet", 1, 1);
}

// Tests the emission of opt-in metrics that are emitted on EDM creation, i.e.,
// on profile startup.
TEST_F(EntityDataManagerTest_InitiallyEmpty, OptInMetric) {
  using enum AutofillAiOptInStatus;
  ASSERT_FALSE(GetAutofillAiOptInStatus(client()));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("Autofill.Ai.OptIn.Status.Startup"),
      BucketsAre(Bucket(kOptedOut, 1)));

  client().SetUpPrefsAndIdentityForAutofillAi();
  ASSERT_TRUE(GetAutofillAiOptInStatus(client()));

  RecreateEntityDataManager_Discouraged();
  EXPECT_THAT(
      histogram_tester().GetAllSamples("Autofill.Ai.OptIn.Status.Startup"),
      BucketsAre(Bucket(kOptedOut, 1), Bucket(kOptedIn, 1)));
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

TEST_F(
    EntityDataManagerTest_InitiallyEmpty,
    SyncablePrefIsOffAndAccountKeyPrefIsOn_FeatureOff_DoNotMigratePrefValue) {
  histogram_tester().ExpectTotalCount("Autofill.Ai.OptIn.PrefMigration", 0);
}

// Tests that whenever pContext entities are prefetched, they are stored in the
// data manager.
TEST_F(EntityDataManagerTest_InitiallyEmpty, OnPrefetchContextComplete) {
  // Wait for the database to load to prevent additional observer events.
  helper().WaitUntilIdle();
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance order = test::GetOrderEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance shipment = test::GetShipmentEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  EXPECT_CALL(observer, OnEntityInstancesChanged);
  entity_data_manager().OnPrefetchContextComplete(
      pcontext_manager(), std::vector<EntityInstance>{order, shipment});
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(order, shipment));
}

// Tests that whenever pContext entities are evicted, they are removed from the
// data manager.
TEST_F(EntityDataManagerTest_InitiallyEmpty, OnMaskedEntityTypeEvicted) {
  // Wait for the database to load to prevent additional observer events.
  helper().WaitUntilIdle();
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance order = test::GetOrderEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance shipment = test::GetShipmentEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  // Prefetch entities of different types.
  // Expect OnEntityInstancesChanged() to be called once after prefetching and
  // once after eviction.
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(2);
  entity_data_manager().OnPrefetchContextComplete(
      pcontext_manager(), std::vector<EntityInstance>{order, shipment});
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(order, shipment));

  // Evict orders.
  entity_data_manager().OnMaskedEntityTypeEvicted(pcontext_manager(),
                                                  order.type());
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(shipment));
}

// Tests that prefetched personal context entities are filtered out if they
// represent the same entity as an already cached local/Wallet entity.
TEST_F(EntityDataManagerTest_InitiallyEmpty,
       OnPrefetchContextComplete_FiltersDuplicateIncomingEntities) {
  EntityInstance flight_local =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET123",
           .confirmation_code = u"CONF123",
           .name = u"Jon Doe",
           .record_type = EntityInstance::RecordType::kLocal});
  EntityInstance passport_wallet =
      test::MaskEntityInstance(test::GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"123456789",
           .record_type = EntityInstance::RecordType::kServerWallet}));
  entity_data_manager().AddOrUpdateEntityInstance(flight_local);
  entity_data_manager().AddOrUpdateEntityInstance(passport_wallet);
  ASSERT_THAT(GetEntityInstances(),
              UnorderedElementsAre(flight_local, passport_wallet));

  EntityInstance flight_pc =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET999",
           .confirmation_code = u"CONF123",
           .name = u"Jane Doe",
           .record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_pc =
      test::MaskEntityInstance(test::GetPassportEntityInstanceWithRandomGuid(
          {.name = u"Jon Doe",
           .number = u"123456789",
           .record_type = EntityInstance::RecordType::kPersonalContext}));
  EntityInstance license_pc =
      test::GetDriversLicenseEntityInstanceWithRandomGuid(
          {.number = u"DL9999",
           .record_type = EntityInstance::RecordType::kPersonalContext});
  entity_data_manager().OnPrefetchContextComplete(
      pcontext_manager(),
      std::vector<EntityInstance>{flight_pc, passport_pc, license_pc});
  helper().WaitUntilIdle();

  EXPECT_THAT(GetEntityInstances(),
              UnorderedElementsAre(flight_local, passport_wallet, license_pc));
}

// Tests that if a duplicate personal context entity is prefetched before the
// database is loaded, it is filtered out once the database load completes.
TEST_F(EntityDataManagerTest_InitiallyEmpty,
       LoadEntitiesFromDatabase_FiltersDuplicateCachedPersonalContextEntities) {
  EntityInstance flight_local =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET123",
           .confirmation_code = u"CONF123",
           .name = u"Jon Doe",
           .record_type = EntityInstance::RecordType::kLocal});
  entity_data_manager().AddOrUpdateEntityInstance(flight_local);

  // Recreating the data manager queues a database load task in its constructor.
  RecreateEntityDataManager_Discouraged();

  // Prefetch a duplicate personal context entity while the database load is
  // still pending.
  EntityInstance flight_pc =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET999",
           .confirmation_code = u"CONF123",
           .name = u"Jane Doe",
           .record_type = EntityInstance::RecordType::kPersonalContext});
  entity_data_manager().OnPrefetchContextComplete(
      pcontext_manager(), std::vector<EntityInstance>{flight_pc});

  // Wait for the database
  helper().WaitUntilIdle();

  EXPECT_THAT(entity_data_manager().GetEntityInstances(),
              UnorderedElementsAre(flight_local));
}

// Tests that if a local/Wallet entity is added or updated after a duplicate
// personal context entity is already cached, the duplicate is filtered out.
TEST_F(
    EntityDataManagerTest_InitiallyEmpty,
    AddOrUpdateEntityInstance_FiltersDuplicateCachedPersonalContextEntities) {
  helper().WaitUntilIdle();
  EntityInstance flight_pc =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET999",
           .confirmation_code = u"CONF123",
           .name = u"Jane Doe",
           .record_type = EntityInstance::RecordType::kPersonalContext});
  entity_data_manager().OnPrefetchContextComplete(
      pcontext_manager(), std::vector<EntityInstance>{flight_pc});
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(flight_pc));

  EntityInstance flight_local =
      test::GetFlightReservationEntityInstanceWithRandomGuid(
          {.ticket_number = u"TICKET123",
           .confirmation_code = u"CONF123",
           .name = u"Jon Doe",
           .record_type = EntityInstance::RecordType::kLocal});
  entity_data_manager().AddOrUpdateEntityInstance(flight_local);
  helper().WaitUntilIdle();

  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(flight_local));
}

}  // namespace
}  // namespace autofill
