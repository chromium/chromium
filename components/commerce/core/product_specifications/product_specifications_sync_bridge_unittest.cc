// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/protocol/compare_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::vector<std::string> kInitUuid = {"asdf", "zdxc"};
const std::vector<std::string> kInitName = {"my_name", "another name"};
const std::vector<int64_t> kCreationTime = {1710953277, 1711035900};
const std::vector<int64_t> kUpdateTime = {
    kCreationTime[0] + base::Time::kMillisecondsPerDay,
    kCreationTime[1] + 2 * base::Time::kMillisecondsPerDay};
const std::vector<std::vector<std::string>> kCompareUrls = {
    {"https://foo.com", "https://bar.com"},
    {"https://foo-bar.com", "https://bar-foo.com"}};

}  // namespace

namespace commerce {

class ProductSpecificationsSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    AddInitialSpecifics();
    bridge_ = std::make_unique<ProductSpecificationsSyncBridge>(
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor());
    base::RunLoop().RunUntilIdle();
  }

  ProductSpecificationsSyncBridge& bridge() { return *bridge_; }

  void AddInitialSpecifics() {
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (uint64_t i = 0; i < kInitUuid.size(); i++) {
      sync_pb::CompareSpecifics compare_specifics;
      compare_specifics.set_uuid(kInitUuid[i]);
      compare_specifics.set_name(kInitName[i]);
      compare_specifics.set_creation_time_unix_epoch_micros(kCreationTime[i]);
      compare_specifics.set_update_time_unix_epoch_micros(kUpdateTime[i]);
      for (auto& compare_url : kCompareUrls[i]) {
        sync_pb::ComparisonData* compare_data = compare_specifics.add_data();
        compare_data->set_url(compare_url);
      }
      batch->WriteData(compare_specifics.uuid(),
                       compare_specifics.SerializeAsString());
    }
    CommitToStoreAndWait(std::move(batch));
  }

  void CommitToStoreAndWait(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch) {
    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop,
               const std::optional<syncer::ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  ProductSpecificationsSyncBridge::CompareSpecificsEntries& entries() {
    return bridge().entries_;
  }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
};

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_compare()->set_uuid("my_uuid");
  EXPECT_EQ("my_uuid", bridge().GetStorageKey(entity));
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetClientTag) {
  syncer::EntityData entity;
  entity.specifics.mutable_compare()->set_uuid("another_uuid");
  EXPECT_EQ("another_uuid", bridge().GetClientTag(entity));
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestInitialization) {
  EXPECT_EQ(2u, entries().size());

  int i = 0;
  for (auto entry = entries().begin(); entry != entries().end(); entry++) {
    EXPECT_EQ(entry->first, entry->second.uuid());
    EXPECT_EQ(kInitUuid[i], entry->second.uuid());
    EXPECT_EQ(kInitName[i], entry->second.name());
    EXPECT_EQ(kCreationTime[i],
              entry->second.creation_time_unix_epoch_micros());
    EXPECT_EQ(kUpdateTime[i], entry->second.update_time_unix_epoch_micros());
    int j = 0;
    for (auto& data : entry->second.data()) {
      EXPECT_EQ(kCompareUrls[i][j], data.url());
      j++;
    }
    i++;
  }
}

}  // namespace commerce
