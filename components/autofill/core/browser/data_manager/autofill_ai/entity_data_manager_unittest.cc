// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

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

  AutofillWebDataServiceTestHelper& helper() { return helper_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
};

// Tests that the constructor of EntityDataManager queries the database.
TEST_F(EntityDataManagerTest, InitialPopulation) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();

  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      pp, base::DoNothing());
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      dl, base::DoNothing());
  helper().WaitUntilIdle();

  EntityDataManager entity_data_manager(helper().autofill_webdata_service(),
                                        /*history_service=*/nullptr,
                                        /*strike_database=*/nullptr);
  EXPECT_THAT(entity_data_manager.GetEntityInstances(), IsEmpty());

  helper().WaitUntilIdle();
  EXPECT_THAT(entity_data_manager.GetEntityInstances(),
              UnorderedElementsAre(pp, dl));
}

// Test fixture that starts with an empty database.
class EntityDataManagerTest_InitiallyEmpty : public EntityDataManagerTest {
 public:
  EntityDataManager& entity_data_manager() { return entity_data_manager_; }

  base::span<const autofill::EntityInstance> GetEntityInstances() {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstances();
  }

  base::optional_ref<const EntityInstance> GetInstance(const base::Uuid& guid) {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstance(guid);
  }

 private:
  EntityDataManager entity_data_manager_{helper().autofill_webdata_service(),
                                         /*history_service=*/nullptr,
                                         /*strike_database=*/nullptr};
};

// Tests that AddOrUpdateEntityInstance() asynchronously adds entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, AddEntityInstance) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(2);
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(dl);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, dl));
}

// Tests that recording an entity being used calls for a database entity update.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RecordEntityUsed) {
  // TODO(crbug.com/402616006): This test should re-read the entity from the db
  // and make sure the persisted information is the expected one. Update once db
  // columns are updated.
  EntityInstance pp = test::GetPassportEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  EXPECT_EQ(pp.use_count(), 0u);
  EXPECT_EQ(pp.use_date(), base::Time::FromTimeT(0));

  base::Time use_date = base::Time::Now();
  helper().WaitUntilIdle();
  entity_data_manager().RecordEntityUsed(pp.guid(), use_date);
  base::optional_ref<const EntityInstance> updated_passport =
      GetInstance(pp.guid());
  ASSERT_TRUE(updated_passport);

  EXPECT_EQ(updated_passport->use_count(), 1u);
  EXPECT_EQ(updated_passport->use_date(), use_date);
}

// Tests that AddOrUpdateEntityInstance() asynchronously updates entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, UpdateEntityInstance) {
  MockEntityDataManagerObserver observer;
  base::ScopedObservation<EntityDataManager, MockEntityDataManagerObserver>
      observation{&observer};
  observation.Observe(&entity_data_manager());

  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(2);
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
  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(3);
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

  EXPECT_CALL(observer, OnEntityInstancesChanged).Times(1);
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
  EXPECT_EQ(
      entity_data_manager().GetEntityInstance(base::Uuid::GenerateRandomV4()),
      std::nullopt);
}

}  // namespace
}  // namespace autofill
