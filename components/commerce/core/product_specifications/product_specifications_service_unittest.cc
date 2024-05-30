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
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
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
    int64_t creation_time_micros_epoch,
    int64_t update_time_micros_epoch,
    const std::string& name,
    std::vector<std::string> urls) {
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

void CheckSpecsAgainstSpecifics(
    const commerce::ProductSpecificationsSet& specifications,
    const sync_pb::ProductComparisonSpecifics& specifics) {
  EXPECT_EQ(base::Uuid::ParseLowercase(specifics.uuid()),
            specifications.uuid());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                specifics.creation_time_unix_epoch_micros()),
            specifications.creation_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                specifics.update_time_unix_epoch_micros()),
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
                                        .creation_time_unix_epoch_micros()) &&
         arg.update_time() == base::Time::FromMillisecondsSinceUnixEpoch(
                                  product_comparison_specifics
                                      .update_time_unix_epoch_micros()) &&
         arg.name() == product_comparison_specifics.name() &&
         arg.urls() == specifics_urls;
}

MATCHER_P(IsSetWithUuid, uuid, "") {
  return arg.uuid() == uuid;
}

MATCHER_P2(HasProductSpecsNameUrl, name, urls, "") {
  return arg.name() == name && arg.urls() == urls;
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

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProductSpecificationsService> service_;
  raw_ptr<MockProductSpecificationsSyncBridge> bridge_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  testing::NiceMock<MockProductSpecificationsSetObserver> observer_;

  syncer::ModelTypeStore* store() { return store_.get(); }

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

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  HasAllProductSpecs(kProductComparisonSpecifics[0]),
                  IsSetWithUuid(uuid_to_modify)))
      .Times(1);

  const std::string new_name = "updated name";

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
  new_specifics.set_update_time_unix_epoch_micros(
      new_specifics.update_time_unix_epoch_micros() +
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

}  // namespace commerce
