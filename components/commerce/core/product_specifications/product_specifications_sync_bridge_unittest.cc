// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
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
            product_comparison_specifics->creation_time_unix_epoch_millis());
  EXPECT_EQ(kUpdateTime[idx],
            product_comparison_specifics->update_time_unix_epoch_millis());
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
    int64_t creation_time_millis_epoch,
    int64_t update_time_millis_epoch,
    std::string name,
    const std::vector<std::string>& urls) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_creation_time_unix_epoch_millis(creation_time_millis_epoch);
  specifics.set_update_time_unix_epoch_millis(update_time_millis_epoch);
  specifics.set_name(name);

  for (auto& url : urls) {
    sync_pb::ComparisonData* compare_data = specifics.add_data();
    compare_data->set_url(url);
  }
  return specifics;
}

const std::vector<sync_pb::ProductComparisonSpecifics> kInitCompareSpecifics = {
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

const std::vector<sync_pb::ProductComparisonSpecifics>
    kProductComparisonSpecifics = {
        BuildProductComparisonSpecifics("40000000-0000-0000-0000-000000000000",
                                        1000,
                                        1001,
                                        "my first set",
                                        {"https://foo.com", "https://bar.com"}),
        BuildProductComparisonSpecifics(
            "50000000-0000-0000-0000-000000000000",
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
  EXPECT_EQ(expected.creation_time_unix_epoch_millis(),
            actual.creation_time_unix_epoch_millis());
  EXPECT_EQ(expected.update_time_unix_epoch_millis(),
            actual.update_time_unix_epoch_millis());
  EXPECT_EQ(expected.name(), actual.name());
  for (int i = 0; i < expected.data_size(); i++) {
    EXPECT_EQ(expected.data()[i].url(), actual.data()[i].url());
  }
}

MATCHER_P(IsMetadataSize, size, "") {
  return arg.get()->GetAllMetadata().size() == size;
}

}  // namespace

namespace commerce {

class ProductSpecificationsSyncBridgeObserver
    : public ProductSpecificationsSyncBridge::Delegate {
 public:
  MOCK_METHOD(
      void,
      OnSpecificsAdded,
      (const std::vector<sync_pb::ProductComparisonSpecifics> specifics),
      (override));
};

class ProductSpecificationsSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
    AddInitialSpecifics();
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    bridge_ = std::make_unique<ProductSpecificationsSyncBridge>(
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), base::DoNothing(), observer());
    base::RunLoop().RunUntilIdle();
    initial_store_ = GetAllStoreData();
    initial_entries_ = entries();
  }

  ProductSpecificationsSyncBridge& bridge() { return *bridge_; }

  void AddInitialSpecifics() {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (uint64_t i = 0; i < kInitUuid.size(); i++) {
      sync_pb::ProductComparisonSpecifics product_comparison_specifics;
      product_comparison_specifics.set_uuid(kInitUuid[i]);
      product_comparison_specifics.set_name(kInitName[i]);
      product_comparison_specifics.set_creation_time_unix_epoch_millis(
          kCreationTime[i]);
      product_comparison_specifics.set_update_time_unix_epoch_millis(
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

  void AddSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {
    return bridge().AddSpecifics(specifics);
  }

  void DeleteSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics>& specifics) {
    bridge().DeleteSpecifics(specifics);
  }

  void CommitToStoreAndWait(
      std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
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
           std::unique_ptr<syncer::DataTypeStore::RecordList> data_records) {
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

  std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries() {
    return bridge().entries_;
  }

  // Number of entries in an arbitrary |bridge|.
  static uint64_t bridge_entries_size(ProductSpecificationsSyncBridge* bridge) {
    return bridge->entries_.size();
  }

  void ProcessorNotTrackingMetadata() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(false));
  }

  sync_pb::ProductComparisonSpecifics ProductSpecificationSetToProto(
      const ProductSpecificationsSet& set) {
    return set.ToProto();
  }

  syncer::DataTypeStore* store() { return store_.get(); }

  syncer::MockDataTypeLocalChangeProcessor& processor() { return processor_; }

  void UpdateSpecifics(const sync_pb::ProductComparisonSpecifics& specifics) {
    bridge().UpdateSpecifics(specifics);
  }

  testing::NiceMock<ProductSpecificationsSyncBridgeObserver>* observer() {
    return &observer_;
  }

  std::unique_ptr<syncer::EntityData> CreateEntityData(
      const sync_pb::ProductComparisonSpecifics& specifics) {
    return bridge().CreateEntityData(specifics);
  }

  sync_pb::ProductComparisonSpecifics TrimSpecificsForCaching(
      const sync_pb::ProductComparisonSpecifics& specifics) {
    return bridge().TrimSpecificsForCaching(specifics);
  }

 protected:
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  testing::NiceMock<ProductSpecificationsSyncBridgeObserver> observer_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  std::map<std::string, sync_pb::ProductComparisonSpecifics> initial_entries_;
  std::map<std::string, sync_pb::ProductComparisonSpecifics> initial_store_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProductSpecificationsSyncMultiSpecsBridgeTest
    : public ProductSpecificationsSyncBridgeTest {
 public:
  void SetUp() override {
    ProductSpecificationsSyncBridgeTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        commerce::kProductSpecificationsMultiSpecifics);
  }
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

  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge().GetDataForCommit(std::move(storage_keys));

  EXPECT_TRUE(data_batch);
  std::vector<syncer::KeyAndData> key_and_data =
      GetKeyAndData(data_batch.get());
  EXPECT_EQ(2u, key_and_data.size());
  EXPECT_EQ(kInitUuid[1], key_and_data[0].first);
  EXPECT_EQ(GetName(1), key_and_data[0].second->name);
  VerifySpecificsAgainstIndex(
      key_and_data[0].second->specifics.mutable_product_comparison(), 1);
  EXPECT_EQ(kInitUuid[2], key_and_data[1].first);
  EXPECT_EQ(GetName(2), key_and_data[1].second->name);
  VerifySpecificsAgainstIndex(
      key_and_data[1].second->specifics.mutable_product_comparison(), 2);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetDataForDebugging) {
  std::unique_ptr<syncer::DataBatch> data_batch =
      bridge().GetAllDataForDebugging();
  EXPECT_TRUE(data_batch);
  std::vector<syncer::KeyAndData> key_and_data =
      GetKeyAndData(data_batch.get());
  EXPECT_EQ(3u, key_and_data.size());
  for (uint64_t i = 0; i < kInitUuid.size(); i++) {
    EXPECT_EQ(kInitUuid[i], key_and_data[i].first);
    EXPECT_EQ(GetName(i), key_and_data[i].second->name);
    VerifySpecificsAgainstIndex(
        key_and_data[i].second->specifics.mutable_product_comparison(), i);
  }
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
  later_specifics.set_update_time_unix_epoch_millis(
      later_specifics.update_time_unix_epoch_millis() +
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
  earlier_specifics.set_update_time_unix_epoch_millis(
      earlier_specifics.update_time_unix_epoch_millis() -
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

TEST_F(ProductSpecificationsSyncBridgeTest, AddSpecifics) {
  AddSpecifics(kProductComparisonSpecifics);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kProductComparisonSpecifics.size(); i++) {
    VerifySpecificsInitialNonExistence(kProductComparisonSpecifics[i]);
    VerifySpecificsExists(kProductComparisonSpecifics[i]);
  }
  VerifyStoreAndEntriesSizeIncreasedBy(2);
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestUpdate) {
  VerifyEntriesAndStoreSize(3);

  sync_pb::ProductComparisonSpecifics original = entries().begin()->second;
  sync_pb::ProductComparisonSpecifics updated = original;
  updated.set_name("new name");
  sync_pb::ComparisonData* data = updated.add_data();
  data->set_url("http://example.com/additional_url");
  UpdateSpecifics(updated);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(original.name(), entries()[original.uuid()].name());
  EXPECT_NE(original.data().size(), entries()[original.uuid()].data().size());

  VerifyEntriesAndStoreSize(3);
}

TEST_F(ProductSpecificationsSyncBridgeTest,
       TestDeleteProductSpecificationsSet) {
  VerifySpecificsExists(kInitCompareSpecifics[0]);
  DeleteSpecifics({kInitCompareSpecifics[0]});
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
  DeleteSpecifics({kInitCompareSpecifics[0]});
  // Delete operation should be ineffectual because we're not tracking metadata
  VerifyEntriesAndStoreSize(3);
  for (const auto& product_comparison_specifics : kInitCompareSpecifics) {
    VerifySpecificsExists(product_comparison_specifics);
  }
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestDeleteMultiSpecifics) {
  VerifySpecificsExists(kInitCompareSpecifics[0]);
  VerifySpecificsExists(kInitCompareSpecifics[1]);
  DeleteSpecifics({kInitCompareSpecifics[0], kInitCompareSpecifics[1]});
  base::RunLoop().RunUntilIdle();
  VerifySpecificsNonExistence(kInitCompareSpecifics[0]);
  VerifySpecificsNonExistence(kInitCompareSpecifics[1]);
}

TEST_F(ProductSpecificationsSyncBridgeTest,
       TestTrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_product_comparison() = kInitCompareSpecifics[0];
  EXPECT_TRUE(entity_specifics.mutable_product_comparison()->has_uuid());
  EXPECT_TRUE(entity_specifics.mutable_product_comparison()->has_name());
  EXPECT_TRUE(entity_specifics.mutable_product_comparison()
                  ->has_creation_time_unix_epoch_millis());
  EXPECT_TRUE(entity_specifics.mutable_product_comparison()
                  ->has_update_time_unix_epoch_millis());
  EXPECT_EQ(kInitCompareSpecifics[0].data_size(),
            entity_specifics.mutable_product_comparison()->data_size());

  sync_pb::EntitySpecifics trimmed =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics);
  EXPECT_FALSE(trimmed.product_comparison().has_uuid());
  EXPECT_FALSE(trimmed.product_comparison().has_name());
  EXPECT_FALSE(
      trimmed.product_comparison().has_creation_time_unix_epoch_millis());
  EXPECT_FALSE(
      trimmed.product_comparison().has_update_time_unix_epoch_millis());
  EXPECT_EQ(0, trimmed.product_comparison().data_size());
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestSupportedFieldsMetadataCache) {
  sync_pb::EntityMetadata entity_metadata;
  // Simulate entity with supported field in cache.
  entity_metadata.mutable_possibly_trimmed_base_specifics()
      ->mutable_product_comparison()
      ->set_name(kInitName[0]);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store()->CreateWriteBatch();
  batch->GetMetadataChangeList()->UpdateMetadata(kInitUuid[0], entity_metadata);
  CommitToStoreAndWait(std::move(batch));

  EXPECT_CALL(processor(), ModelReadyToSync(IsMetadataSize(0u)));

  // Simulate creating bridge with entity with supported field in cache.
  base::RunLoop loop;
  std::unique_ptr<ProductSpecificationsSyncBridge> new_bridge =
      std::make_unique<ProductSpecificationsSyncBridge>(
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store()),
          processor().CreateForwardingProcessor(),
          base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                         loop.QuitClosure()),
          observer());
  loop.Run();
  EXPECT_EQ(0u, bridge_entries_size(new_bridge.get()));
}

TEST_F(ProductSpecificationsSyncBridgeTest,
       TestNoSupportedFieldsMetadataCache) {
  sync_pb::EntityMetadata entity_metadata;
  // Simulate entity with no supported field in cache.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store()->CreateWriteBatch();
  batch->GetMetadataChangeList()->UpdateMetadata(kInitUuid[0], entity_metadata);
  CommitToStoreAndWait(std::move(batch));

  EXPECT_CALL(processor(), ModelReadyToSync(IsMetadataSize(1u)));

  // Simulate creating bridge with no entity with supported field in cache.
  base::RunLoop loop;
  std::unique_ptr<ProductSpecificationsSyncBridge> new_bridge =
      std::make_unique<ProductSpecificationsSyncBridge>(
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store()),
          processor().CreateForwardingProcessor(),
          base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                         loop.QuitClosure()),
          observer());
  loop.Run();
  EXPECT_EQ(3u, bridge_entries_size(new_bridge.get()));
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestCreateEntityDataLegacy) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("70000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  specifics.set_name("test_name");
  sync_pb::ComparisonData* data = specifics.add_data();
  data->set_url("https://a.example.com");
  sync_pb::ComparisonData* another_data = specifics.add_data();
  another_data->set_url("https://b.example.com");

  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(specifics);
  EXPECT_EQ("test_name_70000000-0000-0000-0000-000000000000",
            entity_data->name);
  EXPECT_EQ("70000000-0000-0000-0000-000000000000",
            entity_data->specifics.product_comparison().uuid());
  EXPECT_EQ(1000000000000, entity_data->specifics.product_comparison()
                               .creation_time_unix_epoch_millis());
  EXPECT_EQ(2000000000000, entity_data->specifics.product_comparison()
                               .update_time_unix_epoch_millis());
  EXPECT_EQ("test_name", entity_data->specifics.product_comparison().name());
  EXPECT_EQ("https://a.example.com",
            entity_data->specifics.product_comparison().data()[0].url());
  EXPECT_EQ("https://b.example.com",
            entity_data->specifics.product_comparison().data()[1].url());
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestCreateEntityDataFallback) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("70000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  sync_pb::ComparisonData* data = specifics.add_data();
  data->set_url("https://a.example.com");
  sync_pb::ComparisonData* another_data = specifics.add_data();
  another_data->set_url("https://b.example.com");

  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(specifics);
  EXPECT_EQ("70000000-0000-0000-0000-000000000000", entity_data->name);
  EXPECT_EQ("70000000-0000-0000-0000-000000000000",
            entity_data->specifics.product_comparison().uuid());
  EXPECT_EQ(1000000000000, entity_data->specifics.product_comparison()
                               .creation_time_unix_epoch_millis());
  EXPECT_EQ(2000000000000, entity_data->specifics.product_comparison()
                               .update_time_unix_epoch_millis());
  EXPECT_EQ("https://a.example.com",
            entity_data->specifics.product_comparison().data()[0].url());
  EXPECT_EQ("https://b.example.com",
            entity_data->specifics.product_comparison().data()[1].url());
}

TEST_F(ProductSpecificationsSyncMultiSpecsBridgeTest,
       TestCreateEntityDataTopLevelSpecifics) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("70000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  specifics.mutable_product_comparison()->set_name("test_name");

  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(specifics);

  EXPECT_EQ("product_comparison_70000000-0000-0000-0000-000000000000_test_name",
            entity_data->name);
  EXPECT_EQ("70000000-0000-0000-0000-000000000000",
            entity_data->specifics.product_comparison().uuid());
  EXPECT_EQ(1000000000000, entity_data->specifics.product_comparison()
                               .creation_time_unix_epoch_millis());
  EXPECT_EQ(2000000000000, entity_data->specifics.product_comparison()
                               .update_time_unix_epoch_millis());
  EXPECT_EQ(
      "test_name",
      entity_data->specifics.product_comparison().product_comparison().name());
}

TEST_F(ProductSpecificationsSyncMultiSpecsBridgeTest,
       TestCreateEntityDataItemLevelSpecifics) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("50000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  specifics.mutable_product_comparison_item()->set_product_comparison_uuid(
      "70000000-0000-0000-0000-000000000000");
  specifics.mutable_product_comparison_item()->set_url("https://a.example.com");
  syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  *specifics.mutable_product_comparison_item()->mutable_unique_position() =
      unique_position.ToProto();

  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(specifics);

  EXPECT_EQ(
      "product_comparison_item_70000000-0000-0000-0000-000000000000_50000000-"
      "0000-0000-0000-"
      "000000000000",
      entity_data->name);
  EXPECT_EQ("50000000-0000-0000-0000-000000000000",
            entity_data->specifics.product_comparison().uuid());
  EXPECT_EQ(1000000000000, entity_data->specifics.product_comparison()
                               .creation_time_unix_epoch_millis());
  EXPECT_EQ(2000000000000, entity_data->specifics.product_comparison()
                               .update_time_unix_epoch_millis());
  EXPECT_EQ("70000000-0000-0000-0000-000000000000",
            entity_data->specifics.product_comparison()
                .product_comparison_item()
                .product_comparison_uuid());
  EXPECT_EQ("https://a.example.com", entity_data->specifics.product_comparison()
                                         .product_comparison_item()
                                         .url());
  EXPECT_TRUE(unique_position.Equals(syncer::UniquePosition::FromProto(
      entity_data->specifics.product_comparison()
          .product_comparison_item()
          .unique_position())));
}

// TODO(crbug.com/354231134) expand TestTrimSpecificsForCachingTopLevelSpecific
// and TestTrimSpecificsForCachingProductComparisonItem to include unsupported
// fields.
TEST_F(ProductSpecificationsSyncMultiSpecsBridgeTest,
       TestTrimSpecificsForCachingTopLevelSpecific) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("50000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  specifics.mutable_product_comparison()->set_name("test_name");

  sync_pb::ProductComparisonSpecifics trimmed_specifics =
      TrimSpecificsForCaching(specifics);
  EXPECT_FALSE(trimmed_specifics.has_uuid());
  EXPECT_FALSE(trimmed_specifics.has_creation_time_unix_epoch_millis());
  EXPECT_FALSE(trimmed_specifics.has_update_time_unix_epoch_millis());
  EXPECT_FALSE(trimmed_specifics.has_name());
  EXPECT_TRUE(trimmed_specifics.data().empty());
  EXPECT_FALSE(trimmed_specifics.has_product_comparison());
  EXPECT_FALSE(trimmed_specifics.has_product_comparison_item());
}

TEST_F(ProductSpecificationsSyncMultiSpecsBridgeTest,
       TestTrimSpecificsForCachingProductComparisonItem) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("50000000-0000-0000-0000-000000000000");
  specifics.set_creation_time_unix_epoch_millis(1000000000000);
  specifics.set_update_time_unix_epoch_millis(2000000000000);
  specifics.mutable_product_comparison_item()->set_product_comparison_uuid(
      "70000000-0000-0000-0000-000000000000");
  specifics.mutable_product_comparison_item()->set_url("https://a.example.com");
  syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  *specifics.mutable_product_comparison_item()->mutable_unique_position() =
      unique_position.ToProto();

  sync_pb::ProductComparisonSpecifics trimmed_specifics =
      TrimSpecificsForCaching(specifics);
  EXPECT_FALSE(trimmed_specifics.has_uuid());
  EXPECT_FALSE(trimmed_specifics.has_creation_time_unix_epoch_millis());
  EXPECT_FALSE(trimmed_specifics.has_update_time_unix_epoch_millis());
  EXPECT_FALSE(trimmed_specifics.has_name());
  EXPECT_TRUE(trimmed_specifics.data().empty());
  EXPECT_FALSE(trimmed_specifics.has_product_comparison());
  EXPECT_FALSE(trimmed_specifics.has_product_comparison_item());
}

// TODO(crbug.com/354165274) write a test that ensures no single specifics
// format specifics are written when the multi specifics flag is on.

}  // namespace commerce
