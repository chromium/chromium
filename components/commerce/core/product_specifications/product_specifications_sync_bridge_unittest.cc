// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::vector<std::string> kInitUuid = {
    "10000000-0000-0000-0000-000000000000",
    "20000000-0000-0000-0000-000000000000",
    "30000000-0000-0000-0000-000000000000"};
const std::vector<std::string> kInitName = {"my_name", "another name",
                                            "yet another name"};
const std::vector<int64_t> kCreationTime = {1710953277, 1711035900, 1711118523};
const std::vector<int64_t> kUpdateTime = {
    kCreationTime[0] + base::Time::kMillisecondsPerDay,
    kCreationTime[1] + 2 * base::Time::kMillisecondsPerDay,
    kCreationTime[2] + base::Time::kMillisecondsPerDay};
const std::vector<std::vector<std::string>> kProductComparisonUrls = {
    {"https://foo.com/", "https://bar.com/"},
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

void VerifySpecificsAgainstIndex(
    sync_pb::ProductComparisonSpecifics* product_comparison_specifics,
    uint64_t idx) {
  EXPECT_EQ(kInitUuid[idx], product_comparison_specifics->uuid());
  EXPECT_EQ(kInitName[idx], product_comparison_specifics->name());
  EXPECT_EQ(kCreationTime[idx],
            product_comparison_specifics->creation_time_unix_epoch_micros());
  EXPECT_EQ(kUpdateTime[idx],
            product_comparison_specifics->update_time_unix_epoch_micros());
  int j = 0;
  for (auto& data : product_comparison_specifics->data()) {
    EXPECT_EQ(kProductComparisonUrls[idx][j], data.url());
    j++;
  }
}

std::string GetName(uint64_t idx) {
  return base::StringPrintf("%s_%s", kInitName[idx].c_str(),
                            kInitUuid[idx].c_str());
}

sync_pb::ProductComparisonSpecifics BuildProductComparisonSpecifics(
    std::string uuid,
    int64_t creation_time_micros_epoch,
    int64_t update_time_micros_epoch,
    std::string name,
    const std::vector<std::string>& urls) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_creation_time_unix_epoch_micros(creation_time_micros_epoch);
  specifics.set_update_time_unix_epoch_micros(update_time_micros_epoch);
  specifics.set_name(name);

  for (auto& url : urls) {
    sync_pb::ComparisonData* compare_data = specifics.add_data();
    compare_data->set_url(url);
  }
  return specifics;
}

const sync_pb::ProductComparisonSpecifics kInitCompareSpecifics[] = {
    BuildProductComparisonSpecifics(kInitUuid[0],
                                    kCreationTime[0],
                                    kUpdateTime[0],
                                    kInitName[0],
                                    kProductComparisonUrls[0]),
    BuildProductComparisonSpecifics(kInitUuid[1],
                                    kCreationTime[1],
                                    kUpdateTime[1],
                                    kInitName[1],
                                    kProductComparisonUrls[1]),
    BuildProductComparisonSpecifics(kInitUuid[2],
                                    kCreationTime[2],
                                    kUpdateTime[2],
                                    kInitName[2],
                                    kProductComparisonUrls[2])};

const sync_pb::ProductComparisonSpecifics kProductComparisonSpecifics[] = {
    BuildProductComparisonSpecifics("abba",
                                    1000,
                                    1001,
                                    "my first set",
                                    {"https://foo.com", "https://bar.com"}),
    BuildProductComparisonSpecifics(
        "baab",
        2000,
        2001,
        "my next set",
        {"https://some-url.com", "https://another-url.com"})};

syncer::EntityData MakeEntityData(
    const sync_pb::ProductComparisonSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_product_comparison() = specifics;
  entity_data.name = base::StringPrintf("%s_%s", specifics.name().c_str(),
                                        specifics.uuid().c_str());

  return entity_data;
}

void VerifyCompareSpecifics(const sync_pb::ProductComparisonSpecifics& expected,
                            const sync_pb::ProductComparisonSpecifics& actual) {
  EXPECT_EQ(expected.uuid(), actual.uuid());
  EXPECT_EQ(expected.creation_time_unix_epoch_micros(),
            actual.creation_time_unix_epoch_micros());
  EXPECT_EQ(expected.update_time_unix_epoch_micros(),
            actual.update_time_unix_epoch_micros());
  EXPECT_EQ(expected.name(), actual.name());
  for (int i = 0; i < expected.data_size(); i++) {
    EXPECT_EQ(expected.data()[i].url(), actual.data()[i].url());
  }
}

}  // namespace

namespace commerce {

class ProductSpecificationsSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    AddInitialSpecifics();
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<ProductSpecificationsSyncBridge>(
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), base::DoNothing());
    base::RunLoop().RunUntilIdle();
    initial_store_ = GetAllStoreData();
    initial_entries_ = entries();
  }

  ProductSpecificationsSyncBridge& bridge() { return *bridge_; }

  void AddInitialSpecifics() {
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (uint64_t i = 0; i < kInitUuid.size(); i++) {
      sync_pb::ProductComparisonSpecifics product_comparison_specifics;
      product_comparison_specifics.set_uuid(kInitUuid[i]);
      product_comparison_specifics.set_name(kInitName[i]);
      product_comparison_specifics.set_creation_time_unix_epoch_micros(
          kCreationTime[i]);
      product_comparison_specifics.set_update_time_unix_epoch_micros(
          kUpdateTime[i]);
      for (auto& compare_url : kProductComparisonUrls[i]) {
        sync_pb::ComparisonData* compare_data =
            product_comparison_specifics.add_data();
        compare_data->set_url(compare_url);
      }
      batch->WriteData(product_comparison_specifics.uuid(),
                       product_comparison_specifics.SerializeAsString());
    }
    CommitToStoreAndWait(std::move(batch));
  }

  std::optional<sync_pb::ProductComparisonSpecifics> AddProductSpecifications(
      const std::string name,
      const std::vector<GURL> urls) {
    return bridge().AddProductSpecifications(name, urls);
  }

  std::optional<sync_pb::ProductComparisonSpecifics>
  UpdateProductSpecificationsSet(const ProductSpecificationsSet& set) {
    return bridge().UpdateProductSpecificationsSet(set);
  }

  void DeleteProductSpecifications(const std::string& uuid) {
    bridge().DeleteProductSpecificationsSet(uuid);
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

  std::map<std::string, sync_pb::ProductComparisonSpecifics> GetAllStoreData() {
    base::RunLoop loop;
    std::map<std::string, sync_pb::ProductComparisonSpecifics>
        storage_key_to_specifics;
    bridge_->store_->ReadAllData(base::BindOnce(
        [](base::RunLoop* loop,
           std::map<std::string, sync_pb::ProductComparisonSpecifics>*
               storage_key_to_specifics,
           const std::optional<syncer::ModelError>& error,
           std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
          for (auto& record : *data_records.get()) {
            sync_pb::ProductComparisonSpecifics specifics;
            specifics.ParseFromString(record.value);
            storage_key_to_specifics->emplace(specifics.uuid(), specifics);
          }
          loop->Quit();
        },
        &loop, &storage_key_to_specifics));
    loop.Run();

    return storage_key_to_specifics;
  }

  void VerifySpecificsExists(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics) {
    std::map<std::string, sync_pb::ProductComparisonSpecifics> store_data =
        GetAllStoreData();
    EXPECT_TRUE(store_data.find(product_comparison_specifics.uuid()) !=
                store_data.end());
    EXPECT_TRUE(entries().find(product_comparison_specifics.uuid()) !=
                entries().end());
    VerifyCompareSpecifics(
        product_comparison_specifics,
        store_data.find(product_comparison_specifics.uuid())->second);
    VerifyCompareSpecifics(
        product_comparison_specifics,
        entries().find(product_comparison_specifics.uuid())->second);
  }

  void VerifySpecificsNonExistence(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics) {
    std::map<std::string, sync_pb::ProductComparisonSpecifics> store_data =
        GetAllStoreData();
    EXPECT_TRUE(store_data.find(product_comparison_specifics.uuid()) ==
                store_data.end());
    EXPECT_TRUE(entries().find(product_comparison_specifics.uuid()) ==
                entries().end());
  }

  void VerifySpecificsInitialNonExistence(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics) {
    EXPECT_TRUE(initial_store_.find(product_comparison_specifics.uuid()) ==
                initial_store_.end());
    EXPECT_TRUE(initial_entries_.find(product_comparison_specifics.uuid()) ==
                initial_entries_.end());
  }

  void VerifyEntriesAndStoreSize(uint64_t expected_size) {
    EXPECT_EQ(expected_size, GetAllStoreData().size());
    EXPECT_EQ(expected_size, entries().size());
  }

  void VerifyStoreAndEntriesSizeIncreasedBy(uint64_t size) {
    EXPECT_EQ(GetAllStoreData().size(), initial_store_.size() + size);
    EXPECT_EQ(entries().size(), initial_entries_.size() + size);
  }

  ProductSpecificationsSyncBridge::CompareSpecificsEntries& entries() {
    return bridge().entries_;
  }

  void ProcessorNotTrackingMetadata() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(false));
  }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  ProductSpecificationsSyncBridge::CompareSpecificsEntries initial_entries_;
  std::map<std::string, sync_pb::ProductComparisonSpecifics> initial_store_;
};

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_product_comparison()->set_uuid("my_uuid");
  EXPECT_EQ("my_uuid", bridge().GetStorageKey(entity));
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetClientTag) {
  syncer::EntityData entity;
  entity.specifics.mutable_product_comparison()->set_uuid("another_uuid");
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

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetDataForCommit) {
  ProductSpecificationsSyncBridge::StorageKeyList storage_keys;
  // Leave out first entry. GetDataForCommit() takes in a set of keys
  // so we want to ensure the entries with keys specified are
  // returned, rather than all entries.
  storage_keys.push_back(kInitUuid[1]);
  storage_keys.push_back(kInitUuid[2]);
  base::RunLoop run_loop;
  bridge().GetDataForCommit(
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
                key_and_data[0].second->specifics.mutable_product_comparison(),
                1);
            EXPECT_EQ(kInitUuid[2], key_and_data[1].first);
            EXPECT_EQ(GetName(2), key_and_data[1].second->name);
            VerifySpecificsAgainstIndex(
                key_and_data[1].second->specifics.mutable_product_comparison(),
                2);
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
              key_and_data[i].second->specifics.mutable_product_comparison(),
              i);
        }
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestAdd) {
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kProductComparisonSpecifics[0].uuid(),
      MakeEntityData(kProductComparisonSpecifics[0])));
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kProductComparisonSpecifics[1].uuid(),
      MakeEntityData(kProductComparisonSpecifics[1])));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();

  VerifySpecificsNonExistence(kProductComparisonSpecifics[0]);
  VerifySpecificsNonExistence(kProductComparisonSpecifics[1]);
  VerifyEntriesAndStoreSize(3);
  bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                       std::move(add_changes));

  VerifySpecificsExists(kProductComparisonSpecifics[0]);
  VerifySpecificsExists(kProductComparisonSpecifics[1]);
  VerifyEntriesAndStoreSize(5);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestApplyUpdateLaterTimestamp) {
  syncer::EntityChangeList update_changes;
  sync_pb::ProductComparisonSpecifics earlier_specifics =
      entries().begin()->second;
  sync_pb::ProductComparisonSpecifics later_specifics = earlier_specifics;
  later_specifics.set_update_time_unix_epoch_micros(
      later_specifics.update_time_unix_epoch_micros() +
      base::Time::kMillisecondsPerDay);

  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      later_specifics.uuid(), MakeEntityData(later_specifics)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();

  VerifySpecificsExists(earlier_specifics);
  VerifyEntriesAndStoreSize(3);
  bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                       std::move(update_changes));

  VerifySpecificsExists(later_specifics);
  VerifyEntriesAndStoreSize(3);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestApplyUpdateEarlierTimestamp) {
  base::RunLoop run_loop;
  syncer::EntityChangeList update_changes;
  sync_pb::ProductComparisonSpecifics expected_unchanged_specifics =
      entries().begin()->second;
  sync_pb::ProductComparisonSpecifics earlier_specifics =
      entries().begin()->second;
  earlier_specifics.set_update_time_unix_epoch_micros(
      earlier_specifics.update_time_unix_epoch_micros() -
      base::Time::kMillisecondsPerDay);

  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      earlier_specifics.uuid(), MakeEntityData(earlier_specifics)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();

  VerifySpecificsExists(expected_unchanged_specifics);
  VerifyEntriesAndStoreSize(3);
  bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                       std::move(update_changes));

  VerifySpecificsExists(expected_unchanged_specifics);
  VerifyEntriesAndStoreSize(3);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestDelete) {
  base::RunLoop run_loop;
  syncer::EntityChangeList update_changes;
  sync_pb::ProductComparisonSpecifics deleted_specifics =
      entries().begin()->second;

  update_changes.push_back(
      syncer::EntityChange::CreateDelete(deleted_specifics.uuid()));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();

  VerifySpecificsExists(deleted_specifics);
  VerifyEntriesAndStoreSize(3);
  bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                       std::move(update_changes));
  VerifySpecificsNonExistence(deleted_specifics);
  VerifyEntriesAndStoreSize(2);
}

TEST_F(ProductSpecificationsSyncBridgeTest, AddProductSpecifications) {
  const std::optional<sync_pb::ProductComparisonSpecifics> new_specifics =
      AddProductSpecifications(kInitName[0].data(),
                               {GURL(kProductComparisonUrls[0][0]),
                                GURL(kProductComparisonUrls[0][1])});
  EXPECT_TRUE(new_specifics.has_value());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(new_specifics.has_value());
  EXPECT_EQ(kInitName[0], new_specifics->name());
  EXPECT_EQ(kProductComparisonUrls[0][0], new_specifics->data()[0].url());
  EXPECT_EQ(kProductComparisonUrls[0][1], new_specifics->data()[1].url());
  VerifySpecificsInitialNonExistence(new_specifics.value());
  VerifySpecificsExists(new_specifics.value());
  VerifyStoreAndEntriesSizeIncreasedBy(1);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestUpdate) {
  VerifyEntriesAndStoreSize(3);

  sync_pb::ProductComparisonSpecifics specifics = entries().begin()->second;

  const std::string original_name = specifics.name();
  const int original_url_count = specifics.data().size();
  const std::string original_uuid = specifics.uuid();

  std::vector<GURL> urls;
  for (const sync_pb::ComparisonData& data : specifics.data()) {
    urls.emplace_back(data.url());
  }
  urls.emplace_back(GURL("http://example.com/additional_url"));

  ProductSpecificationsSet set(
      specifics.uuid(), specifics.creation_time_unix_epoch_micros(),
      specifics.update_time_unix_epoch_micros(), urls, "new name");

  EXPECT_TRUE(UpdateProductSpecificationsSet(set).has_value());
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(original_name, entries()[original_uuid].name());
  EXPECT_NE(original_url_count, entries()[original_uuid].data().size());

  VerifyEntriesAndStoreSize(3);
}

TEST_F(ProductSpecificationsSyncBridgeTest,
       TestDeleteProductSpecificationsSet) {
  VerifySpecificsExists(kInitCompareSpecifics[0]);
  DeleteProductSpecifications(kInitCompareSpecifics[0].uuid());
  base::RunLoop().RunUntilIdle();
  VerifySpecificsNonExistence(kInitCompareSpecifics[0]);
}

TEST_F(ProductSpecificationsSyncBridgeTest,
       TestDeleteProductSpecificationsSetNotTrackingMetadata) {
  ProcessorNotTrackingMetadata();
  VerifyEntriesAndStoreSize(3);
  for (const auto& product_comparison_specifics : kInitCompareSpecifics) {
    VerifySpecificsExists(product_comparison_specifics);
  }
  DeleteProductSpecifications(kInitCompareSpecifics[0].uuid());
  // Delete operation should be ineffectual because we're not tracking metadata
  VerifyEntriesAndStoreSize(3);
  for (const auto& product_comparison_specifics : kInitCompareSpecifics) {
    VerifySpecificsExists(product_comparison_specifics);
  }
}

}  // namespace commerce
