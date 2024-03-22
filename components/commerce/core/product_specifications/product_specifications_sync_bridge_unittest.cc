// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/compare_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::vector<std::string> kInitUuid = {"asdf", "zdxc", "zetf"};
const std::vector<std::string> kInitName = {"my_name", "another name",
                                            "yet another name"};
const std::vector<int64_t> kCreationTime = {1710953277, 1711035900, 1711118523};
const std::vector<int64_t> kUpdateTime = {
    kCreationTime[0] + base::Time::kMillisecondsPerDay,
    kCreationTime[1] + 2 * base::Time::kMillisecondsPerDay,
    kCreationTime[2] + base::Time::kMillisecondsPerDay};
const std::vector<std::vector<std::string>> kCompareUrls = {
    {"https://foo.com", "https://bar.com"},
    {"https://foo-bar.com", "https://bar-foo.com"},
    {"https://amazon.com/dp/12345",
     "https://www.gap.com/browse/product.do?pid=39573"}};

std::vector<syncer::KeyAndData> GetKeyAndData(syncer::DataBatch* data_batch) {
  std::vector<syncer::KeyAndData> key_and_data;
  while (data_batch->HasNext()) {
    key_and_data.push_back(data_batch->Next());
  }
  return key_and_data;
}

void VerifySpecificsAgainstIndex(sync_pb::CompareSpecifics* compare_specifics,
                                 uint64_t idx) {
  EXPECT_EQ(kInitUuid[idx], compare_specifics->uuid());
  EXPECT_EQ(kInitName[idx], compare_specifics->name());
  EXPECT_EQ(kCreationTime[idx],
            compare_specifics->creation_time_unix_epoch_micros());
  EXPECT_EQ(kUpdateTime[idx],
            compare_specifics->update_time_unix_epoch_micros());
  int j = 0;
  for (auto& data : compare_specifics->data()) {
    EXPECT_EQ(kCompareUrls[idx][j], data.url());
    j++;
  }
}

std::string GetName(uint64_t idx) {
  return base::StringPrintf("%s_%s", kInitName[idx].c_str(),
                            kInitUuid[idx].c_str());
}

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
  EXPECT_EQ(3u, entries().size());

  int i = 0;
  for (auto entry = entries().begin(); entry != entries().end(); entry++) {
    EXPECT_EQ(entry->first, entry->second.uuid());
    VerifySpecificsAgainstIndex(&entry->second, i);
    i++;
  }
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetData) {
  ProductSpecificationsSyncBridge::StorageKeyList storage_keys;
  // Leave out first entry. GetData takes in a set of keys
  // so we want to ensure the entries with keys specified are
  // returned, rather than all entries.
  storage_keys.push_back(kInitUuid[1]);
  storage_keys.push_back(kInitUuid[2]);
  base::RunLoop run_loop;
  bridge().GetData(
      std::move(storage_keys),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<syncer::DataBatch> data_batch) {
            EXPECT_TRUE(data_batch);
            std::vector<syncer::KeyAndData> key_and_data =
                GetKeyAndData(data_batch.get());
            EXPECT_EQ(2u, key_and_data.size());
            EXPECT_EQ(kInitUuid[1], key_and_data[0].first);
            EXPECT_EQ(GetName(1), key_and_data[0].second->name);
            VerifySpecificsAgainstIndex(
                key_and_data[0].second->specifics.mutable_compare(), 1);
            EXPECT_EQ(kInitUuid[2], key_and_data[1].first);
            EXPECT_EQ(GetName(2), key_and_data[1].second->name);
            VerifySpecificsAgainstIndex(
                key_and_data[1].second->specifics.mutable_compare(), 2);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetDataForDebugging) {
  base::RunLoop run_loop;
  bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
      [&](std::unique_ptr<syncer::DataBatch> data_batch) {
        EXPECT_TRUE(data_batch);
        std::vector<syncer::KeyAndData> key_and_data =
            GetKeyAndData(data_batch.get());
        EXPECT_EQ(3u, key_and_data.size());
        for (uint64_t i = 0; i < kInitUuid.size(); i++) {
          EXPECT_EQ(kInitUuid[i], key_and_data[i].first);
          EXPECT_EQ(GetName(i), key_and_data[i].second->name);
          VerifySpecificsAgainstIndex(
              key_and_data[i].second->specifics.mutable_compare(), i);
        }
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace commerce
