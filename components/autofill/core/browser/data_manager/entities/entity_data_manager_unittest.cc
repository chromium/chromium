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
#include "components/autofill/core/browser/webdata/entities/entity_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// Test fixture for the asynchronous database operations in EntityDataManager.
class EntityDataManagerTest : public testing::Test {
 public:
  EntityDataManagerTest() {
    web_database_service_->AddTable(std::make_unique<EntityTable>());
    web_database_service_->LoadDatabase(os_crypt_.get());
    autofill_webdata_service_->Init(base::NullCallback());
  }

  void TearDown() override { web_database_service_->ShutdownDatabase(); }

  EntityDataManager& entity_data_manager() { return entity_data_manager_; }

  std::vector<EntityInstance> GetEntityInstances() {
    base::test::TestFuture<std::vector<EntityInstance>> instances;
    entity_data_manager().LoadEntityInstances(instances.GetCallback());
    return instances.Get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  scoped_refptr<WebDatabaseService> web_database_service_{
      base::MakeRefCounted<WebDatabaseService>(
          base::FilePath(WebDatabase::kInMemoryPath),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault())};
  scoped_refptr<AutofillWebDataService> autofill_webdata_service_ =
      base::MakeRefCounted<AutofillWebDataService>(
          web_database_service_,
          base::SingleThreadTaskRunner::GetCurrentDefault());
  EntityDataManager entity_data_manager_{autofill_webdata_service_};
};

TEST_F(EntityDataManagerTest, InitiallyEmpty) {
  EXPECT_THAT(GetEntityInstances(), IsEmpty());
}

// Tests that AddEntityInstance() asynchronously adds entities.
TEST_F(EntityDataManagerTest, AddEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddEntityInstance(pp);
  entity_data_manager().AddEntityInstance(lc);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp, lc));
}

// Test that adding different entities ignores the second entity.
// That is, the database is not corrupted.
TEST_F(EntityDataManagerTest, AddEntityInstance_Conflict) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance(
      {.guid = pp.guid().AsLowercaseString()});
  ASSERT_EQ(pp.guid(), lc.guid());

  entity_data_manager().AddEntityInstance(pp);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp));
  entity_data_manager().AddEntityInstance(lc);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp));
}

// Tests that UpdateEntityInstance() asynchronously updates entities.
TEST_F(EntityDataManagerTest, UpdateEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddEntityInstance(pp);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp));

  pp = test::GetPassportEntityInstance(
      {.name = "Karlsson", .date_modified = test::kJune2017 - base::Days(1)});
  entity_data_manager().UpdateEntityInstance(pp);
  entity_data_manager().UpdateEntityInstance(lc);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp, lc));
}

// Tests that RemoveEntityInstance() asynchronously removes entities.
TEST_F(EntityDataManagerTest, RemoveEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  entity_data_manager().AddEntityInstance(pp);
  entity_data_manager().AddEntityInstance(lc);
  EXPECT_THAT(GetEntityInstances(), ElementsAre(pp, lc));

  entity_data_manager().RemoveEntityInstance(pp.guid());
  EXPECT_THAT(GetEntityInstances(), ElementsAre(lc));
}

}  // namespace
}  // namespace autofill
