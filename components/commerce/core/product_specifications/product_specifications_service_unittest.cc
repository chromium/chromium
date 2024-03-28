// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <vector>

#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/sync/protocol/compare_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

sync_pb::CompareSpecifics BuildCompareSpecifics(
    const std::string& uuid,
    int64_t creation_time_micros_epoch,
    int64_t update_time_micros_epoch,
    const std::string& name,
    std::vector<std::string> urls) {
  sync_pb::CompareSpecifics specifics;
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

const sync_pb::CompareSpecifics kCompareSpecifics[] = {
    BuildCompareSpecifics("abba",
                          1710953277,
                          1710953277 + base::Time::kMillisecondsPerDay,
                          "my first set",
                          {"https://foo.com", "https://bar.com"}),
    BuildCompareSpecifics(
        "baab",
        1711035900,
        1711035900 + (2 * base::Time::kMillisecondsPerDay) / 3,
        "my next set",
        {"https://some-url.com", "https://another-url.com"})};

}  // namespace

namespace commerce {

class MockProductSpecificationsSyncBridge
    : public ProductSpecificationsSyncBridge {
 public:
  MockProductSpecificationsSyncBridge(
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
      : ProductSpecificationsSyncBridge(std::move(create_store_callback),
                                        std::move(change_processor)) {}
  ~MockProductSpecificationsSyncBridge() override = default;

  MOCK_METHOD(std::unique_ptr<syncer::MetadataChangeList>,
              CreateMetadataChangeList,
              (),
              (override));

  MOCK_METHOD(std::optional<syncer::ModelError>,
              MergeFullSyncData,
              (std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
               syncer::EntityChangeList entity_changes),
              (override));

  MOCK_METHOD(std::optional<syncer::ModelError>,
              ApplyIncrementalSyncChanges,
              (std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
               syncer::EntityChangeList entity_changes),
              (override));

  MOCK_METHOD(std::string,
              GetStorageKey,
              (const syncer::EntityData& entity_data),
              (override));

  MOCK_METHOD(std::string,
              GetClientTag,
              (const syncer::EntityData& entity_data),
              (override));

  MOCK_METHOD(void,
              GetData,
              (StorageKeyList storage_keys, DataCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAllDataForDebugging,
              (DataCallback callback),
              (override));

  void AddCompareSpecifics(const sync_pb::CompareSpecifics& compare_specifics) {
    entries_.emplace(compare_specifics.uuid(), compare_specifics);
  }
};

class ProductSpecificationsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    std::unique_ptr<MockProductSpecificationsSyncBridge> bridge =
        std::make_unique<MockProductSpecificationsSyncBridge>(
            syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store()),
            change_processor().CreateForwardingProcessor());
    bridge_ = bridge.get();
    service_ =
        std::make_unique<ProductSpecificationsService>(std::move(bridge));
    base::RunLoop().RunUntilIdle();
  }

  MockProductSpecificationsSyncBridge* bridge() { return bridge_; }

  ProductSpecificationsService* service() { return service_.get(); }

  void CheckSpecsAgainstSpecifics(
      const ProductSpecificationsSet& specifications,
      const sync_pb::CompareSpecifics& specifics) const {
    EXPECT_EQ(base::Uuid::ParseLowercase(specifics.uuid()),
              specifications.uuid());
    EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                  specifics.creation_time_unix_epoch_micros()),
              specifications.creation_time());
    EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                  specifics.update_time_unix_epoch_micros()),
              specifications.update_time());
    EXPECT_EQ(specifics.name(), specifications.name());
    std::vector<const GURL> urls;
    for (const sync_pb::ComparisonData& data : specifics.data()) {
      urls.emplace_back(data.url());
    }
    EXPECT_EQ(urls, specifications.urls());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProductSpecificationsService> service_;
  raw_ptr<MockProductSpecificationsSyncBridge> bridge_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;

  syncer::ModelTypeStore* store() { return store_.get(); }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>& change_processor() {
    return processor_;
  }
};

TEST_F(ProductSpecificationsServiceTest, TestGetProductSpecifications) {
  for (const sync_pb::CompareSpecifics& specifics : kCompareSpecifics) {
    bridge()->AddCompareSpecifics(specifics);
  }
  const std::vector<const ProductSpecificationsSet> specifications =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(2u, specifications.size());
  for (uint64_t i = 0; i < specifications.size(); i++) {
    CheckSpecsAgainstSpecifics(specifications[i], kCompareSpecifics[i]);
  }
}

}  // namespace commerce
