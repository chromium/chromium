// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/entities/entity_table.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/browser/webdata/entities/entity_table_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// Test fixture for synchronous database operations.
class EntityTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestWebDatabase"),
                       &encryptor_));
  }

  EntityTable& table() { return table_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::ScopedTempDir temp_dir_;
  const os_crypt_async::Encryptor encryptor_ =
      os_crypt_async::GetTestEncryptorForTesting();
  EntityTable table_;
  WebDatabase db_;
};

// Tests updating entity instances.
TEST_F(EntityTableTest, AddOrUpdateEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(pp));

  // Updating a non-existing instance adds it.
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(lc));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));

  pp = test::GetPassportEntityInstance({
      .name = u"Karlsson",
      .date_modified = test::kJune2017 - base::Days(1),
  });
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));
}

// Tests removing individual entity instances.
TEST_F(EntityTableTest, RemoveEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(lc));

  // Removing an element once removes it.
  // Removing it a second time succeeds but has no effect.
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));
  EXPECT_TRUE(table().RemoveEntityInstance(pp.guid()));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(lc));
  EXPECT_TRUE(table().RemoveEntityInstance(pp.guid()));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(lc));

  // Same for the other element.
  EXPECT_TRUE(table().RemoveEntityInstance(lc.guid()));
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());
  EXPECT_TRUE(table().RemoveEntityInstance(lc.guid()));
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());
}

// Tests removing a date range of entity instances.
TEST_F(EntityTableTest, RemoveEntityInstancesModifiedBetween) {
  auto instances =
      std::array{test::GetPassportEntityInstance(
                     {.date_modified = test::kJune2017 - base::Days(11)}),
                 test::GetLoyaltyCardEntityInstance(
                     {.date_modified = test::kJune2017 - base::Days(10)})};
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(instances[0]));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(instances[1]));
  ASSERT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements outside of the date range are not affected.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[0].date_modified() - base::Days(10),
      instances[0].date_modified() - base::Days(1)));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements outside of the date range are not affected.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[1].date_modified() + base::Days(1),
      instances[1].date_modified() + base::Days(10)));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements in the date range are removed.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[0].date_modified() - base::Days(1),
      instances[1].date_modified() + base::Days(1)));
  EXPECT_THAT(table().GetEntityInstances(), IsEmpty());
}

// Tests that entity instances without any valid attributes are not returned
// from the database.
TEST_F(EntityTableTest, GetEntityInstancesSkipsEmptyInstances) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());

  EXPECT_TRUE(table().AddOrUpdateEntityInstance(pp));
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(lc));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));

  // Manipulate the attribute instances: changing their type simulates a change
  // of the entity schema.
  sql::Statement s;
  s.Assign(test_api(table()).db()->GetUniqueStatement(
      R"(UPDATE attributes
         SET type = type || 'some-garbage-suffix'
         WHERE entity_guid = ?)"));
  s.BindString(0, pp.guid().AsLowercaseString());
  ASSERT_TRUE(s.Run()) << "The UPDATE failed: "
                       << test_api(table()).db()->GetErrorMessage()
                       << " (Check the table and column names in the "
                          "UpdateBuilder() call above.)";
  ASSERT_GT(test_api(table()).db()->GetLastChangeCount(), 0);

  EXPECT_THAT(table().GetEntityInstances(), ElementsAre(lc));
}

}  // namespace
}  // namespace autofill
