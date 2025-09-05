// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"

#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner_test_api.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class EntityInstanceCleanerTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    entity_data_manager_ = std::make_unique<EntityDataManager>(
        pref_service_.get(), /*identity_manager=*/nullptr,
        webdata_helper_.autofill_webdata_service(), /*history_service=*/nullptr,
        /*strike_database=*/nullptr);
    cleaner_ = std::make_unique<EntityInstanceCleaner>(
        entity_data_manager_.get(), &sync_service_, pref_service_.get());
  }

  sync_preferences::TestingPrefServiceSyncable& pref_service() {
    return *pref_service_;
  }
  EntityInstanceCleaner& cleaner() { return *cleaner_; }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  syncer::TestSyncService sync_service_;
  std::unique_ptr<EntityDataManager> entity_data_manager_;
  std::unique_ptr<EntityInstanceCleaner> cleaner_;
};

TEST_F(EntityInstanceCleanerTest, CleanupsArePending) {
  EXPECT_TRUE(test_api(cleaner()).AreCleanupsPending());
  test_api(cleaner()).MaybeCleanupEntityInstanceData();
  EXPECT_FALSE(test_api(cleaner()).AreCleanupsPending());
}

TEST_F(EntityInstanceCleanerTest, DeduplicationNotRunIfMilestoneIsTheSame) {
  int current_milestone = version_info::GetMajorVersionNumberAsInt();
  pref_service().SetInteger(prefs::kAutofillAiLastVersionDeduped,
                            current_milestone);
  test_api(cleaner()).MaybeCleanupEntityInstanceData();
  // No deduplication should have happened, so the pref should be the same.
  EXPECT_EQ(pref_service().GetInteger(prefs::kAutofillAiLastVersionDeduped),
            current_milestone);
}

TEST_F(EntityInstanceCleanerTest, DeduplicationRunIfMilestoneIsDifferent) {
  int current_milestone = version_info::GetMajorVersionNumberAsInt();
  pref_service().SetInteger(prefs::kAutofillAiLastVersionDeduped,
                            current_milestone - 1);
  test_api(cleaner()).MaybeCleanupEntityInstanceData();
  // Deduplication should have happened, so the pref should be updated.
  EXPECT_EQ(pref_service().GetInteger(prefs::kAutofillAiLastVersionDeduped),
            current_milestone);
}

}  // namespace autofill
