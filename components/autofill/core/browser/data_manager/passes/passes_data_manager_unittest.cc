
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/passes/passes_data_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/passes_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/browser/webdata/passes/passes_table.h"
#include "components/sync/base/data_type.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class PassesDataManagerTest : public testing::Test {
 public:
  PassesDataManagerTest() {
    auto table = std::make_unique<PassesTable>();
    passes_table_ = table.get();
    helper_ =
        std::make_unique<AutofillWebDataServiceTestHelper>(std::move(table));
  }

  AutofillWebDataServiceTestHelper& helper() { return *helper_; }

  AutofillWebDataService& webdata_service() {
    return *helper().autofill_webdata_service();
  }

  PassesTable& passes_table() { return *passes_table_; }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<PassesTable> passes_table_;
  std::unique_ptr<AutofillWebDataServiceTestHelper> helper_;
};

// Tests that the `PassseDataManager` correctly loads loyalty cards from the
// database in the constructor.
TEST_F(PassesDataManagerTest, GetLoyaltyCards) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();

  passes_table().AddOrUpdateLoyaltyCard(card1);
  passes_table().AddOrUpdateLoyaltyCard(card2);

  PassesDataManager passes_data_manager(&webdata_service());
  EXPECT_THAT(passes_data_manager.GetLoyaltyCards(), IsEmpty());

  helper().WaitUntilIdle();
  EXPECT_THAT(passes_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

// Verify that the `PassesDataManager` correctly updates the list of loyalty
// cards when the Chrome Sync writes them to the database.
TEST_F(PassesDataManagerTest, DataChangedBySync) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  passes_table().AddOrUpdateLoyaltyCard(card1);

  PassesDataManager passes_data_manager(&webdata_service());
  helper().WaitUntilIdle();
  EXPECT_THAT(passes_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1));

  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  passes_table().AddOrUpdateLoyaltyCard(card2);
  // Make sure all async tasks are executed.
  helper().WaitUntilIdle();

  webdata_service().GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_LOYALTY_CARD);
      }));
  // `WaitUntilIdle()` needs to be called twice here:
  // * `NotifyOnAutofillChangedBySync()` posts a task to the UI sequence.
  // * the task to fetch the list of loyalty cards is posted to the db sequence.
  // * the task to update the cache in the `PassesDataManager` is then posted to
  //   the UI sequence.
  helper().WaitUntilIdle();
  helper().WaitUntilIdle();
  EXPECT_THAT(passes_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

}  // namespace
}  // namespace autofill
