// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

class TabGroupSyncServiceTest : public testing::Test {
 public:
  TabGroupSyncServiceTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  ~TabGroupSyncServiceTest() override = default;

  void SetUp() override {
    model_ = std::make_unique<SavedTabGroupModel>();
    tab_group_sync_service_ = std::make_unique<TabGroupSyncServiceImpl>(
        std::move(model_), processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
            store_.get()));
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    task_environment_.RunUntilIdle();
  }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<TabGroupSyncServiceImpl> tab_group_sync_service_;
};

TEST_F(TabGroupSyncServiceTest, ServiceConstruction) {
  // TODO(b/326546431): Add more tests.
}

}  // namespace tab_groups
