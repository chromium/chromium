// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/browser/webdata/entities/entity_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

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
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();

  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      pp, base::DoNothing());
  helper().autofill_webdata_service()->AddOrUpdateEntityInstance(
      lc, base::DoNothing());
  helper().WaitUntilIdle();

  EntityDataManager entity_data_manager(helper().autofill_webdata_service());
  EXPECT_THAT(entity_data_manager.GetEntityInstances(), IsEmpty());

  helper().WaitUntilIdle();
  EXPECT_THAT(entity_data_manager.GetEntityInstances(),
              UnorderedElementsAre(pp, lc));
}

// Test fixture that starts with an empty database.
class EntityDataManagerTest_InitiallyEmpty : public EntityDataManagerTest {
 public:
  EntityDataManager& entity_data_manager() { return entity_data_manager_; }

  std::vector<EntityInstance> GetEntityInstances() {
    helper().WaitUntilIdle();
    return entity_data_manager().GetEntityInstances();
  }

 private:
  EntityDataManager entity_data_manager_{helper().autofill_webdata_service()};
};

// Tests that AddOrUpdateEntityInstance() asynchronously adds entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, AddEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(lc);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, lc));
}

// Tests that AddOrUpdateEntityInstance() asynchronously updates entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, UpdateEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp));

  pp = test::GetPassportEntityInstance(
      {.name = u"Karlsson", .date_modified = test::kJune2017 - base::Days(1)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(pp));
}

// Tests that RemoveEntityInstance() asynchronously removes entities.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RemoveEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(lc);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, lc));

  entity_data_manager().RemoveEntityInstance(pp.guid());
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(lc));
}

// Tests that removing a non-existing entity is a no-op.
TEST_F(EntityDataManagerTest_InitiallyEmpty, RemoveEntityInstance_NonExisting) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(lc);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, lc));

  entity_data_manager().RemoveEntityInstance(pp.guid());
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(lc));
  entity_data_manager().RemoveEntityInstance(pp.guid());  // No-op.
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(lc));
}

// Tests that removing entities in a date range updates the cache.
TEST_F(EntityDataManagerTest_InitiallyEmpty,
       RemoveEntityInstancesModifiedBetween) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(1)});
  EntityInstance lc = test::GetLoyaltyCardEntityInstance(
      {.date_modified = test::kJune2017 + base::Days(1)});
  entity_data_manager().AddOrUpdateEntityInstance(pp);
  entity_data_manager().AddOrUpdateEntityInstance(lc);
  ASSERT_THAT(GetEntityInstances(), UnorderedElementsAre(pp, lc));

  entity_data_manager().RemoveEntityInstancesModifiedBetween(
      test::kJune2017 - base::Days(1), test::kJune2017);
  EXPECT_THAT(GetEntityInstances(), UnorderedElementsAre(lc));

  entity_data_manager().RemoveEntityInstancesModifiedBetween(
      test::kJune2017, test::kJune2017 + base::Days(2) + base::Seconds(1));
  EXPECT_THAT(GetEntityInstances(), IsEmpty());
}

}  // namespace
}  // namespace autofill
