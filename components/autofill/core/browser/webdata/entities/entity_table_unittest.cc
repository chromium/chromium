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
#include "components/autofill/core/common/autofill_features.h"
#include "components/webdata/common/web_database.h"
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
              db_.Init(temp_dir_.GetPath().AppendASCII("TestWebDatabase")));
  }

  EntityTable& table() { return table_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::ScopedTempDir temp_dir_;
  EntityTable table_;
  WebDatabase db_;
};

// Tests adding entity instances.
TEST_F(EntityTableTest, AddEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());

  // Added elements are in the table.
  EXPECT_TRUE(table().AddEntityInstance(pp));
  EXPECT_THAT(table().GetEntityInstances(), ElementsAre(pp));
  EXPECT_TRUE(table().AddEntityInstance(lc));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));

  // Adding a conflicting entity fails because of the primary-key violation.
  // Nonetheless it leaves the database in a bad state: some attributes may
  // be added before the failure, so reading the entity afterwards may obtain a
  // union of the two conflicting entities.
  EXPECT_FALSE(table().AddEntityInstance(pp));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));
}

// Tests updating entity instances.
TEST_F(EntityTableTest, UpdateEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_TRUE(table().AddEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(pp));

  // Updating a non-existing instance adds it.
  EXPECT_TRUE(table().UpdateEntityInstance(lc));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));

  pp = test::GetPassportEntityInstance({
      .name = "Karlsson",
      .date_modified = test::kJune2017 - base::Days(1),
  });
  EXPECT_TRUE(table().UpdateEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, lc));
}

// Tests removing individual entity instances.
TEST_F(EntityTableTest, RemoveEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance lc = test::GetLoyaltyCardEntityInstance();
  ASSERT_TRUE(table().AddEntityInstance(pp));
  ASSERT_TRUE(table().AddEntityInstance(lc));

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
  ASSERT_TRUE(table().AddEntityInstance(instances[0]));
  ASSERT_TRUE(table().AddEntityInstance(instances[1]));
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

}  // namespace
}  // namespace autofill
