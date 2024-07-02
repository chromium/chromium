// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kProductOneUrl[] = "https://example.com/productone";
const char kProductTwoUrl[] = "https://example.com.com/producttwo";
const char kProductSpecsName[] = "name";

sync_pb::ProductComparisonSpecifics BuildProductComparisonSpecifics(
    const std::string& uuid,
    int64_t creation_time_millis_epoch,
    int64_t update_time_millis_epoch,
    const std::string& name,
    std::vector<std::string> urls) {
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

void CheckSpecsAgainstSpecifics(
    const commerce::ProductSpecificationsSet& specifications,
    const sync_pb::ProductComparisonSpecifics& specifics) {
  EXPECT_EQ(base::Uuid::ParseLowercase(specifics.uuid()),
            specifications.uuid());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                specifics.creation_time_unix_epoch_millis()),
            specifications.creation_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                specifics.update_time_unix_epoch_millis()),
            specifications.update_time());
  EXPECT_EQ(specifics.name(), specifications.name());
  std::vector<GURL> urls;
  for (const sync_pb::ComparisonData& data : specifics.data()) {
    urls.emplace_back(data.url());
  }
  EXPECT_EQ(urls, specifications.urls());
}

const sync_pb::ProductComparisonSpecifics kProductComparisonSpecifics[] = {
    BuildProductComparisonSpecifics(
        "abe18411-bd7e-4819-b9b5-11e66e0ad8b4",
        1710953277,
        1710953277 + base::Time::kMillisecondsPerDay,
        "my first set",
        {"https://foo.com", "https://bar.com"}),
    BuildProductComparisonSpecifics(
        "f448709c-fe1f-44ea-883e-f46267b97d29",
        1711035900,
        1711035900 + (2 * base::Time::kMillisecondsPerDay) / 3,
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

void AddTestSpecifics(commerce::ProductSpecificationsSyncBridge* bridge) {
  syncer::EntityChangeList add_changes;
  for (const auto& specifics : kProductComparisonSpecifics) {
    add_changes.push_back(syncer::EntityChange::CreateAdd(
        specifics.uuid(), MakeEntityData(specifics)));
  }
  bridge->ApplyIncrementalSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
}

template <class T>
void Swap(std::vector<T>& items, std::vector<std::pair<int, int>> indices) {
  for (auto& indice_pair : indices) {
    std::swap(items[indice_pair.first], items[indice_pair.second]);
  }
}

MATCHER_P(HasAllProductSpecs, product_comparison_specifics, "") {
  std::vector<GURL> specifics_urls;
  for (const sync_pb::ComparisonData& data :
       product_comparison_specifics.data()) {
    specifics_urls.emplace_back(data.url());
  }
  return arg.uuid().AsLowercaseString() ==
             product_comparison_specifics.uuid() &&
         arg.creation_time() == base::Time::FromMillisecondsSinceUnixEpoch(
                                    product_comparison_specifics
                                        .creation_time_unix_epoch_millis()) &&
         arg.update_time() == base::Time::FromMillisecondsSinceUnixEpoch(
                                  product_comparison_specifics
                                      .update_time_unix_epoch_millis()) &&
         arg.name() == product_comparison_specifics.name() &&
         arg.urls() == specifics_urls;
}

MATCHER_P(IsSetWithUuid, uuid, "") {
  return arg.uuid() == uuid;
}

MATCHER_P2(HasProductSpecsNameUrl, name, urls, "") {
  return arg.name() == name && arg.urls() == urls;
}

MATCHER_P(HasProductSpecsName, name, "") {
  return arg == name;
}

}  // namespace

namespace commerce {

class MockProductSpecificationsSetObserver
    : public ProductSpecificationsSet::Observer {
 public:
  MOCK_METHOD(void,
              OnProductSpecificationsSetAdded,
              (const ProductSpecificationsSet& set),
              (override));

  MOCK_METHOD(void,
              OnProductSpecificationsSetUpdate,
              (const ProductSpecificationsSet& before,
               const ProductSpecificationsSet& after),
              (override));

  MOCK_METHOD(void,
              OnProductSpecificationsSetNameUpdate,
              (const std::string& before, const std::string& after),
              (override));

  MOCK_METHOD(void,
              OnProductSpecificationsSetRemoved,
              (const ProductSpecificationsSet& set),
              (override));
};

class ProductSpecificationsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    service_ = std::make_unique<ProductSpecificationsService>(
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store()),
        change_processor().CreateForwardingProcessor());
    service_->AddObserver(&observer_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { service_->RemoveObserver(&observer_); }

  ProductSpecificationsSyncBridge* bridge() { return service_->bridge_.get(); }

  ProductSpecificationsService* service() { return service_.get(); }

  testing::NiceMock<MockProductSpecificationsSetObserver>* observer() {
    return &observer_;
  }

  void AddCompareSpecificsForTesting(
      sync_pb::ProductComparisonSpecifics compare_specifics) {
    bridge()->AddCompareSpecificsForTesting(compare_specifics);
  }

  void SetIsInitialized(bool is_initialized) {
    service()->is_initialized_ = is_initialized;
  }

  void OnInit() { service()->OnInit(); }

  uint64_t GetDeferredOperationsSize() {
    return service()->deferred_operations_.size();
  }

  void EnableMultiSpecFlag() {
    scoped_feature_list_.InitAndEnableFeature(
        commerce::kProductSpecificationsMultiSpecifics);
  }

  std::map<std::string, sync_pb::ProductComparisonSpecifics> GetAllStoreData() {
    base::RunLoop loop;
    std::map<std::string, sync_pb::ProductComparisonSpecifics>
        storage_key_to_specifics;
    store()->ReadAllData(
        base::BindOnce(
            [](std::map<std::string, sync_pb::ProductComparisonSpecifics>*
                   storage_key_to_specifics,
               const std::optional<syncer::ModelError>& error,
               std::unique_ptr<syncer::ModelTypeStore::RecordList>
                   data_records) {
              for (auto& record : *data_records.get()) {
                sync_pb::ProductComparisonSpecifics specifics;
                specifics.ParseFromString(record.value);
                storage_key_to_specifics->emplace(specifics.uuid(), specifics);
              }
            },
            &storage_key_to_specifics)
            .Then(loop.QuitClosure()));
    loop.Run();

    return storage_key_to_specifics;
  }

  syncer::ModelTypeStore* store() { return store_.get(); }

  std::map<std::string, sync_pb::ProductComparisonSpecifics> entries() {
    return service()->bridge_->entries_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProductSpecificationsService> service_;
  raw_ptr<ProductSpecificationsSyncBridge> bridge_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  testing::NiceMock<MockProductSpecificationsSetObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>& change_processor() {
    return processor_;
  }
};

TEST_F(ProductSpecificationsServiceTest, TestGetProductSpecifications) {
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }
  const std::vector<ProductSpecificationsSet> specifications =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(2u, specifications.size());
  for (uint64_t i = 0; i < specifications.size(); i++) {
    CheckSpecsAgainstSpecifics(specifications[i],
                               kProductComparisonSpecifics[i]);
  }
}

TEST_F(ProductSpecificationsServiceTest, TestGetProductSpecificationsAsync) {
  SetIsInitialized(false);
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }
  base::RunLoop run_loop;

  service()->GetAllProductSpecifications(
      base::BindOnce([](const std::vector<ProductSpecificationsSet>
                            specifications) {
        EXPECT_EQ(2u, specifications.size());
        for (uint64_t i = 0; i < specifications.size(); i++) {
          CheckSpecsAgainstSpecifics(specifications[i],
                                     kProductComparisonSpecifics[i]);
        }
      }).Then(run_loop.QuitClosure()));

  EXPECT_EQ(1u, GetDeferredOperationsSize());
  OnInit();
  run_loop.Run();
}

TEST_F(ProductSpecificationsServiceTest, TestGetSetByUuidAsync) {
  SetIsInitialized(false);
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }
  base::RunLoop run_loop;

  service()->GetSetByUuid(
      base::Uuid::ParseLowercase(kProductComparisonSpecifics[0].uuid()),
      base::BindOnce([](std::optional<ProductSpecificationsSet> specification) {
        EXPECT_TRUE(specification.has_value());
        CheckSpecsAgainstSpecifics(specification.value(),
                                   kProductComparisonSpecifics[0]);
      }).Then(run_loop.QuitClosure()));

  EXPECT_EQ(1u, GetDeferredOperationsSize());
  OnInit();
  run_loop.Run();
}

TEST_F(ProductSpecificationsServiceTest,
       TestGetSetByUuidAsyncAlreadyInitialized) {
  SetIsInitialized(true);
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }
  base::RunLoop run_loop;

  service()->GetSetByUuid(
      base::Uuid::ParseLowercase(kProductComparisonSpecifics[0].uuid()),
      base::BindOnce([](std::optional<ProductSpecificationsSet> specification) {
        EXPECT_TRUE(specification.has_value());
        CheckSpecsAgainstSpecifics(specification.value(),
                                   kProductComparisonSpecifics[0]);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ProductSpecificationsServiceTest, TestAddProductSpecificationsSuccess) {
  std::vector<GURL> expected_product_urls{GURL(kProductOneUrl),
                                          GURL(kProductTwoUrl)};
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetAdded(HasProductSpecsNameUrl(
                  kProductSpecsName, expected_product_urls)))
      .Times(1);
  std::optional<ProductSpecificationsSet> product_spec_set =
      service()->AddProductSpecificationsSet(
          kProductSpecsName, {GURL(kProductOneUrl), GURL(kProductTwoUrl)});
  EXPECT_TRUE(product_spec_set.has_value());
  EXPECT_EQ(kProductSpecsName, product_spec_set.value().name());
  EXPECT_EQ(kProductOneUrl, product_spec_set.value().urls()[0].spec());
  EXPECT_EQ(kProductTwoUrl, product_spec_set.value().urls()[1].spec());
}

TEST_F(ProductSpecificationsServiceTest, TestRemoveProductSpecifications) {
  AddTestSpecifics(bridge());
  EXPECT_CALL(
      *observer(),
      OnProductSpecificationsSetRemoved(IsSetWithUuid(
          base::Uuid::ParseLowercase(kProductComparisonSpecifics[0].uuid()))))
      .Times(1);
  service()->DeleteProductSpecificationsSet(
      kProductComparisonSpecifics[0].uuid());
}

TEST_F(ProductSpecificationsServiceTest, TestObserverNewSpecifics) {
  syncer::EntityChangeList add_changes;
  for (const auto& specifics : kProductComparisonSpecifics) {
    add_changes.push_back(syncer::EntityChange::CreateAdd(
        specifics.uuid(), MakeEntityData(specifics)));
    EXPECT_CALL(*observer(),
                OnProductSpecificationsSetAdded(HasAllProductSpecs(specifics)))
        .Times(1);
  }
  bridge()->ApplyIncrementalSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
}

TEST_F(ProductSpecificationsServiceTest, TestSetUrls) {
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }

  const std::vector<ProductSpecificationsSet> specifications =
      service()->GetAllProductSpecifications();

  const base::Uuid uuid_to_modify = specifications[0].uuid();

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  HasAllProductSpecs(kProductComparisonSpecifics[0]),
                  IsSetWithUuid(uuid_to_modify)))
      .Times(1);

  const std::vector<GURL> new_urls = {GURL("http://example.com/updated")};

  service()->SetUrls(uuid_to_modify, new_urls);

  const std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(uuid_to_modify);

  EXPECT_TRUE(updated_set.has_value());
  EXPECT_EQ(new_urls[0].spec(), updated_set->urls()[0].spec());
  EXPECT_GT(updated_set->update_time(), specifications[0].update_time());
  EXPECT_EQ(updated_set->creation_time(), specifications[0].creation_time());
}

TEST_F(ProductSpecificationsServiceTest, TestSetName) {
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }

  const std::vector<ProductSpecificationsSet> specifications =
      service()->GetAllProductSpecifications();

  const base::Uuid uuid_to_modify = specifications[0].uuid();

  const std::string new_name = "updated name";
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  HasAllProductSpecs(kProductComparisonSpecifics[0]),
                  IsSetWithUuid(uuid_to_modify)))
      .Times(1);
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetNameUpdate(
                  HasProductSpecsName(kProductComparisonSpecifics[0].name()),
                  HasProductSpecsName(new_name)))
      .Times(1);

  service()->SetName(uuid_to_modify, new_name);

  const std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(uuid_to_modify);

  EXPECT_TRUE(updated_set.has_value());
  EXPECT_EQ(new_name, updated_set->name());
  EXPECT_GT(updated_set->update_time(), specifications[0].update_time());
  EXPECT_EQ(updated_set->creation_time(), specifications[0].creation_time());
}

TEST_F(ProductSpecificationsServiceTest, TestSetNameAndUrls_BadId) {
  for (const sync_pb::ProductComparisonSpecifics& specifics :
       kProductComparisonSpecifics) {
    AddCompareSpecificsForTesting(specifics);
  }

  const std::vector<ProductSpecificationsSet> specifications =
      service()->GetAllProductSpecifications();

  const base::Uuid uuid_to_modify =
      base::Uuid::ParseLowercase("90000000-0000-0000-0000-000000000000");

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetNameUpdate(testing::_, testing::_))
      .Times(0);

  const std::vector<GURL> new_urls = {GURL("http://example.com/updated")};

  service()->SetUrls(uuid_to_modify, new_urls);
  service()->SetName(uuid_to_modify, "new name");

  const std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(uuid_to_modify);

  EXPECT_FALSE(updated_set.has_value());
}

TEST_F(ProductSpecificationsServiceTest, TestObserverUpdateSpecifics) {
  AddTestSpecifics(bridge());
  syncer::EntityChangeList update_changes;
  sync_pb::ProductComparisonSpecifics new_specifics =
      kProductComparisonSpecifics[0];
  sync_pb::ComparisonData* new_specifics_data = new_specifics.add_data();
  new_specifics_data->set_url("https://newurl.com/");
  new_specifics.set_update_time_unix_epoch_millis(
      new_specifics.update_time_unix_epoch_millis() +
      base::Time::kMillisecondsPerDay);
  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      new_specifics.uuid(), MakeEntityData(new_specifics)));

  // Won't be updated because the update timestamp hasn't increased.
  sync_pb::ProductComparisonSpecifics noupdate_specifics =
      kProductComparisonSpecifics[1];
  sync_pb::ComparisonData* noupdate_specifics_data =
      noupdate_specifics.add_data();
  noupdate_specifics_data->set_url("https://newurl.com/");
  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      noupdate_specifics.uuid(), MakeEntityData(noupdate_specifics)));

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  HasAllProductSpecs(kProductComparisonSpecifics[0]),
                  HasAllProductSpecs(new_specifics)))
      .Times(1);
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  HasAllProductSpecs(kProductComparisonSpecifics[1]),
                  HasAllProductSpecs(noupdate_specifics)))
      .Times(0);
  bridge()->ApplyIncrementalSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(update_changes));
}

TEST_F(ProductSpecificationsServiceTest, TestObserverRemoveSpecifics) {
  AddTestSpecifics(bridge());
  syncer::EntityChangeList remove_changes;
  for (const auto& specifics : kProductComparisonSpecifics) {
    remove_changes.push_back(
        syncer::EntityChange::CreateDelete(specifics.uuid()));
    EXPECT_CALL(*observer(), OnProductSpecificationsSetRemoved(IsSetWithUuid(
                                 base::Uuid::ParseLowercase(specifics.uuid()))))
        .Times(1);
  }
  bridge()->ApplyIncrementalSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(remove_changes));
}

TEST_F(ProductSpecificationsServiceTest,
       TestGetProductSpecificationsMultiSpecifics) {
  EnableMultiSpecFlag();

  std::vector<GURL> expected_product_urls{GURL(kProductOneUrl),
                                          GURL(kProductTwoUrl)};
  service()->AddProductSpecificationsSet(kProductSpecsName,
                                         expected_product_urls);
  base::RunLoop().RunUntilIdle();

  std::vector<ProductSpecificationsSet> sets =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(1u, sets.size());
  EXPECT_EQ(kProductSpecsName, sets[0].name());
  for (size_t i = 0; i < expected_product_urls.size(); i++) {
    EXPECT_EQ(expected_product_urls[i], sets[0].urls()[i]);
  }
}

TEST_F(ProductSpecificationsServiceTest,
       TestGetProductSpecificationsLargeURLList) {
  EnableMultiSpecFlag();

  // Test robustness of ordering - restored product specifications should be
  // in the same order they are passed to AddProductSpecificationsSet.
  std::vector<GURL> expected_product_urls;
  for (int i = 1; i <= 20; i++) {
    expected_product_urls.push_back(
        GURL(base::StringPrintf("https://example.com/%d", i)));
  }
  // Randomize order
  // Items are ordered according to the input order, but some randomization
  // has been added here to make the test example more realistic (i.e. it's
  // unlikely the input order would be alphabetical in a real input case.
  Swap(expected_product_urls, {{2, 10}, {4, 3}, {15, 1}});

  service()->AddProductSpecificationsSet(kProductSpecsName,
                                         expected_product_urls);

  base::RunLoop().RunUntilIdle();

  std::vector<ProductSpecificationsSet> sets =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(1u, sets.size());
  EXPECT_EQ(kProductSpecsName, sets[0].name());
  for (size_t i = 0; i < expected_product_urls.size(); i++) {
    EXPECT_EQ(expected_product_urls[i], sets[0].urls()[i]);
  }
}

TEST_F(ProductSpecificationsServiceTest,
       TestAddProductSpecificationsMultipleSpecifics) {
  EnableMultiSpecFlag();
  std::vector<GURL> expected_urls = {GURL("https://foo.com/"),
                                     GURL("https://bar.com/")};
  service()->AddProductSpecificationsSet("name", expected_urls);

  // Check specifics stored in memory as well as the store.
  for (auto& specifics_map : {entries(), GetAllStoreData()}) {
    EXPECT_EQ(3u, specifics_map.size());

    sync_pb::ProductComparisonItem item_specifics[2];
    sync_pb::ProductComparisonSpecifics top_level;
    std::vector<std::string> urls;
    std::string name = "";
    for (auto& [_, specifics] : specifics_map) {
      // Legacy fields should not be used when storing product specifications
      // across multiple specifics.
      EXPECT_FALSE(specifics.has_name());
      EXPECT_TRUE(specifics.data().empty());

      // Specific should contain a top level ProductComparison or a
      // ProductComparisonItem, not both (or neither).
      EXPECT_EQ(specifics.has_product_comparison(),
                !specifics.has_product_comparison_item());

      // Find item level specifics in the same order the URLs were passed to
      // AddProductSpecifications
      if (specifics.has_product_comparison_item()) {
        for (int i = 0; i < 2; i++) {
          if (specifics.product_comparison_item().url() == expected_urls[i]) {
            item_specifics[i] = specifics.product_comparison_item();
          }
        }
      }
      if (specifics.has_product_comparison()) {
        top_level = specifics;
      }
    };
    // Check ordering passed to AddProductSpecifications is preserved
    EXPECT_TRUE(
        syncer::UniquePosition::FromProto(item_specifics[0].unique_position())
            .LessThan(syncer::UniquePosition::FromProto(
                item_specifics[1].unique_position())));
    // Check name and URLs.
    EXPECT_EQ("name", top_level.product_comparison().name());
    for (auto& item_specific : item_specifics) {
      EXPECT_EQ(top_level.uuid(), item_specific.product_comparison_uuid());
    }
  }
}

}  // namespace commerce
