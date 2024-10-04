// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <optional>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
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
#include "components/sync/base/unique_position.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
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
  *result_listener << "Actual name:  " << arg.name() << "\n";
  *result_listener << "Actual urls:  "
                   << base::JoinString(base::ToVector(arg.urls(), &GURL::spec),
                                       ", ")
                   << "\n";
  *result_listener << "Expected name:  " << name << "\n";
  *result_listener << "Expeted urls:  "
                   << base::JoinString(base::ToVector(urls, &GURL::spec), ", ")
                   << "\n";
  return arg.name() == name && arg.urls() == urls;
}

MATCHER_P2(HasProductSpecsNameUrlInfos, name, url_infos, "") {
  std::vector<GURL> urls;
  for (const auto& url_info : url_infos) {
    urls.push_back(url_info.url);
  }
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
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    service_ = std::make_unique<ProductSpecificationsService>(
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store()),
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

  void DisableMultiSpecFlag() {
    scoped_feature_list_.InitAndDisableFeature(
        commerce::kProductSpecificationsMultiSpecifics);
  }

  void EnableMigrateProductSpecificationsSets() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        commerce::kProductSpecificationsMultiSpecifics,
        {
            {commerce::kProductSpecsMigrateToMultiSpecificsParam, "true"},
        });
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
               std::unique_ptr<syncer::DataTypeStore::RecordList>
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

  void CheckProductSpecificationsExists(
      const std::vector<ProductSpecificationsSet> query_sets) {
    for (const auto& query_set : query_sets) {
      EXPECT_TRUE(HasProductSpecifications(query_set))
          << "Set " << query_set.name() << " not found\n";
    }
  }

  void CheckProductSpecificationsAbsent(
      const std::vector<ProductSpecificationsSet>& query_sets) {
    for (const auto& query_set : query_sets) {
      EXPECT_FALSE(HasProductSpecifications(query_set))
          << "Set " << query_set.name() << " found\n";
    }
  }

  bool HasProductSpecifications(const ProductSpecificationsSet& query_set) {
    for (const auto& stored_set : service_->GetAllProductSpecifications()) {
      if (query_set.uuid() == stored_set.uuid() &&
          query_set.name() == stored_set.name() &&
          query_set.urls() == stored_set.urls()) {
        return true;
      }
    }
    return false;
  }

  syncer::DataTypeStore* store() { return store_.get(); }

  std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries() {
    return service()->bridge_->entries_;
  }

  ProductSpecificationsSet FindProductSpecificationsSet(
      const base::Uuid& query_uuid) {
    const std::vector<commerce::ProductSpecificationsSet> sets =
        service()->GetAllProductSpecifications();
    for (const ProductSpecificationsSet& set : sets) {
      if (set.uuid() == query_uuid) {
        return set;
      }
    }
    NOTREACHED() << "Set with uuid " << query_uuid.AsLowercaseString()
                 << " not found\n";
  }

  void ApplyIncrementalSyncChangesForTesting(
      const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                  syncer::EntityChange::ChangeType>>
          to_change) {
    bridge()->ApplyIncrementalSyncChangesForTesting(to_change);
  }

  void VerifyProductSpecificationsSet(
      const base::Uuid uuid,
      std::optional<ProductSpecificationsSet> expected) {
    base::RunLoop loop;
    service()->GetSetByUuid(
        uuid, base::BindOnce(
                  [](const std::optional<ProductSpecificationsSet> expected,
                     const std::optional<ProductSpecificationsSet> set) {
                    EXPECT_EQ(expected.has_value(), set.has_value());
                    if (expected.has_value()) {
                      EXPECT_EQ(expected.value().uuid(), set.value().uuid());
                      EXPECT_EQ(expected.value().name(), set.value().name());
                      // TODO(crbug.com/353746117) Investigate why time checks
                      // are failing on win-rel.
                      EXPECT_EQ(expected.value().urls(), set.value().urls());
                    }
                  },
                  std::move(expected))
                  .Then(loop.QuitClosure()));
    loop.Run();
  }

  void AddSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> to_add) {
    bridge()->AddSpecifics(to_add);
  }

  void DeleteSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> to_delete) {
    bridge()->DeleteSpecifics(to_delete);
  }

  void MigrateLegacySpecificsIfApplicable() {
    service()->MigrateLegacySpecificsIfApplicable();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProductSpecificationsService> service_;
  raw_ptr<ProductSpecificationsSyncBridge> bridge_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  testing::NiceMock<MockProductSpecificationsSetObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  change_processor() {
    return processor_;
  }
};

class ProductSpecificationsServiceSyncDisabledTest
    : public ProductSpecificationsServiceTest {
 public:
  void SetUp() override {
    ProductSpecificationsServiceTest::SetUp();
    initial_set_ = std::make_unique<ProductSpecificationsSet>(
        service()
            ->AddProductSpecificationsSet(kProductSpecsName,
                                          {UrlInfo(GURL(kProductOneUrl), u""),
                                           UrlInfo(GURL(kProductTwoUrl), u"")})
            .value());
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(false));
    service()->DisableInitializedForTesting();
  }

  ProductSpecificationsSet* initial_set() { return initial_set_.get(); }

 private:
  std::unique_ptr<ProductSpecificationsSet> initial_set_;
};

class ProductSpecificationsServiceWithTitleTest
    : public ProductSpecificationsServiceTest {
 public:
  void SetUp() override {
    ProductSpecificationsServiceTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        commerce::kProductSpecificationsSyncTitle);
  }
};

TEST_F(ProductSpecificationsServiceTest, TestGetProductSpecifications) {
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
  std::vector<GURL> expected_product_urls{GURL(kProductOneUrl),
                                          GURL(kProductTwoUrl)};
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetAdded(HasProductSpecsNameUrl(
                  kProductSpecsName, expected_product_urls)))
      .Times(1);
  std::optional<ProductSpecificationsSet> product_spec_set =
      service()->AddProductSpecificationsSet(
          kProductSpecsName, {UrlInfo(GURL(kProductOneUrl), u""),
                              UrlInfo(GURL(kProductTwoUrl), u"")});
  EXPECT_TRUE(product_spec_set.has_value());
  EXPECT_EQ(kProductSpecsName, product_spec_set.value().name());
  EXPECT_EQ(kProductOneUrl, product_spec_set.value().urls()[0].spec());
  EXPECT_EQ(kProductTwoUrl, product_spec_set.value().urls()[1].spec());
}

TEST_F(ProductSpecificationsServiceTest, TestRemoveProductSpecifications) {
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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

  const std::vector<UrlInfo> new_urls = {
      UrlInfo(GURL("http://example.com/updated"), u"")};

  service()->SetUrls(uuid_to_modify, new_urls);

  const std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(uuid_to_modify);

  EXPECT_TRUE(updated_set.has_value());
  EXPECT_EQ(new_urls[0].url.spec(), updated_set->urls()[0].spec());
  EXPECT_GT(updated_set->update_time(), specifications[0].update_time());
  EXPECT_EQ(updated_set->creation_time(), specifications[0].creation_time());
}

TEST_F(ProductSpecificationsServiceTest, TestSetName) {
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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

  const std::vector<UrlInfo> new_urls = {
      UrlInfo(GURL("http://example.com/updated"), u"")};

  service()->SetUrls(uuid_to_modify, new_urls);
  service()->SetName(uuid_to_modify, "new name");

  const std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(uuid_to_modify);

  EXPECT_FALSE(updated_set.has_value());
}

TEST_F(ProductSpecificationsServiceTest, TestObserverUpdateSpecifics) {
  DisableMultiSpecFlag();
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
  DisableMultiSpecFlag();
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
  std::vector<UrlInfo> expected_product_urls{
      UrlInfo(GURL(kProductOneUrl), u""), UrlInfo(GURL(kProductTwoUrl), u"")};
  service()->AddProductSpecificationsSet(kProductSpecsName,
                                         expected_product_urls);
  base::RunLoop().RunUntilIdle();

  std::vector<ProductSpecificationsSet> sets =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(1u, sets.size());
  EXPECT_EQ(kProductSpecsName, sets[0].name());
  for (size_t i = 0; i < expected_product_urls.size(); i++) {
    EXPECT_EQ(expected_product_urls[i].url, sets[0].urls()[i]);
  }
}

TEST_F(ProductSpecificationsServiceTest,
       TestGetProductSpecificationsLargeURLList) {
  // Test robustness of ordering - restored product specifications should be
  // in the same order they are passed to AddProductSpecificationsSet.
  std::vector<UrlInfo> expected_product_urls;
  for (size_t i = 1; i <= kMaxTableSize; i++) {
    expected_product_urls.push_back(
        UrlInfo(GURL(base::StringPrintf("https://example.com/%d", i)), u""));
  }
  // Randomize order
  // Items are ordered according to the input order, but some randomization
  // has been added here to make the test example more realistic (i.e. it's
  // unlikely the input order would be alphabetical in a real input case.
  Swap(expected_product_urls, {{2, 9}, {4, 3}, {7, 1}});

  service()->AddProductSpecificationsSet(kProductSpecsName,
                                         expected_product_urls);

  base::RunLoop().RunUntilIdle();

  std::vector<ProductSpecificationsSet> sets =
      service()->GetAllProductSpecifications();
  EXPECT_EQ(1u, sets.size());
  EXPECT_EQ(kProductSpecsName, sets[0].name());
  for (size_t i = 0; i < expected_product_urls.size(); i++) {
    EXPECT_EQ(expected_product_urls[i].url, sets[0].urls()[i]);
  }
}

TEST_F(ProductSpecificationsServiceTest,
       TestAddProductSpecificationsMultipleSpecifics) {
  std::vector<UrlInfo> expected_urls = {UrlInfo(GURL("https://foo.com/"), u""),
                                        UrlInfo(GURL("https://bar.com/"), u"")};
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
          if (specifics.product_comparison_item().url() ==
              expected_urls[i].url) {
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

TEST_F(ProductSpecificationsServiceTest,
       TestAddProductSpecificationsMultipleSpecifics_MaxSize) {
  // Create a list with one too many URLs.
  std::vector<UrlInfo> initial_url_list;
  for (size_t i = 0; i < kMaxTableSize + 1; i++) {
    initial_url_list.push_back(
        UrlInfo(GURL(base::StringPrintf("http://example.com/%d", i)), u""));
  }

  // The expected URL set should be the max count without the extra.
  std::vector<GURL> expected_urls;
  for (size_t i = 0; i < kMaxTableSize; i++) {
    expected_urls.push_back(initial_url_list[i].url);
  }

  std::optional<ProductSpecificationsSet> created_set =
      service()->AddProductSpecificationsSet("name", initial_url_list);

  EXPECT_TRUE(created_set.has_value());

  auto set_infos = created_set->url_infos();
  EXPECT_EQ(kMaxTableSize, set_infos.size());
  for (auto& info : set_infos) {
    ASSERT_TRUE(base::Contains(expected_urls, info.url));
  }
}

TEST_F(ProductSpecificationsServiceTest, TestDeleteProductSpecsMultiSpecifics) {
  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i < 3; i++) {
    sets.push_back(service()
                       ->AddProductSpecificationsSet(
                           base::StringPrintf("Set %d", i),
                           {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
                       .value());
  }
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists(sets);

  EXPECT_CALL(
      *observer(),
      OnProductSpecificationsSetRemoved(IsSetWithUuid(
          base::Uuid::ParseLowercase(sets[1].uuid().AsLowercaseString()))))
      .Times(1);
  service()->DeleteProductSpecificationsSet(sets[1].uuid().AsLowercaseString());
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists({sets[0], sets[2]});
  CheckProductSpecificationsAbsent({sets[1]});
}

TEST_F(ProductSpecificationsServiceTest, TestSetUrlsMultiSpecifics) {
  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i <= 2; i++) {
    sets.push_back(service()
                       ->AddProductSpecificationsSet(
                           base::StringPrintf("Set %d", i),
                           {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
                       .value());
  }
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists(sets);

  std::vector<UrlInfo> new_urls{UrlInfo(GURL("https://x.example.com"), u""),
                                UrlInfo(GURL("https://y.example.com"), u""),
                                UrlInfo(GURL("https://z.example.com"), u"")};

  const ProductSpecificationsSet set_to_modify = sets[1];

  EXPECT_CALL(
      *observer(),
      OnProductSpecificationsSetUpdate(
          HasProductSpecsNameUrl(set_to_modify.name(), set_to_modify.urls()),
          HasProductSpecsNameUrlInfos(set_to_modify.name(), new_urls)))
      .Times(1);
  service()->SetUrls(set_to_modify.uuid(), new_urls);
  base::RunLoop().RunUntilIdle();

  std::vector<ProductSpecificationsSet> all_sets =
      service()->GetAllProductSpecifications();
  const ProductSpecificationsSet* modified_set = nullptr;
  for (const ProductSpecificationsSet& set : all_sets) {
    if (set.uuid() == set_to_modify.uuid()) {
      modified_set = &set;
    }
  }
  EXPECT_NE(nullptr, modified_set) << "Couldn't find modified set";
  EXPECT_EQ(new_urls, modified_set->url_infos());
}

TEST_F(ProductSpecificationsServiceTest, TestSetUrlsMultiSpecifics_MaxSize) {
  // Create a list the max number of URLs.
  std::vector<UrlInfo> url_list;
  for (size_t i = 0; i < kMaxTableSize; i++) {
    url_list.push_back(
        UrlInfo(GURL(base::StringPrintf("http://example.com/%d", i)), u""));
  }

  // The expected URL set should be the max count without the extra.
  std::vector<GURL> expected_urls;
  for (size_t i = 0; i < url_list.size(); i++) {
    expected_urls.push_back(url_list[i].url);
  }

  std::optional<ProductSpecificationsSet> created_set =
      service()->AddProductSpecificationsSet("name", url_list);

  EXPECT_TRUE(created_set.has_value());

  // Add one more URL to go over the max.
  url_list.push_back(UrlInfo(GURL("http://example.com/over_max"), u""));

  std::optional<ProductSpecificationsSet> updated_set =
      service()->SetUrls(created_set->uuid(), url_list);

  EXPECT_TRUE(updated_set.has_value());

  auto set_infos = updated_set->url_infos();
  EXPECT_EQ(kMaxTableSize, set_infos.size());
  for (auto& info : set_infos) {
    ASSERT_TRUE(base::Contains(expected_urls, info.url));
  }
}

TEST_F(ProductSpecificationsServiceTest, TestSetNameMultiSpecifics) {
  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i < 2; i++) {
    sets.push_back(service()
                       ->AddProductSpecificationsSet(
                           base::StringPrintf("Set %d", i),
                           {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
                       .value());
  }
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists(sets);

  ProductSpecificationsSet set_to_modify =
      FindProductSpecificationsSet(sets[1].uuid());
  EXPECT_EQ("Set 1", set_to_modify.name());
  EXPECT_CALL(
      *observer(),
      OnProductSpecificationsSetUpdate(
          HasProductSpecsNameUrl(set_to_modify.name(), set_to_modify.urls()),
          HasProductSpecsNameUrl("New name", set_to_modify.urls())))
      .Times(1);
  service()->SetName(set_to_modify.uuid(), "New name");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("New name",
            FindProductSpecificationsSet(set_to_modify.uuid()).name());
}

TEST_F(ProductSpecificationsServiceTest, TestSetNameMultiSpecifics_MaxLength) {
  std::string long_name = "";
  for (size_t i = 0; i < kMaxNameLength * 2; i++) {
    long_name += "A";
  }

  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i < 2; i++) {
    sets.push_back(
        service()
            ->AddProductSpecificationsSet(
                long_name, {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
            .value());
  }
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists(sets);

  ProductSpecificationsSet set_to_modify =
      FindProductSpecificationsSet(sets[1].uuid());
  EXPECT_EQ(kMaxNameLength,
            FindProductSpecificationsSet(set_to_modify.uuid()).name().length());

  EXPECT_CALL(*observer(), OnProductSpecificationsSetUpdate(
                               HasProductSpecsNameUrl(set_to_modify.name(),
                                                      set_to_modify.urls()),
                               testing::_))
      .Times(1);
  service()->SetName(set_to_modify.uuid(), long_name);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kMaxNameLength,
            FindProductSpecificationsSet(set_to_modify.uuid()).name().length());
}

TEST_F(ProductSpecificationsServiceTest,
       TestSetNameMultiSpecificsTopLevelSpecificAbsent) {
  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i < 2; i++) {
    sets.push_back(service()
                       ->AddProductSpecificationsSet(
                           base::StringPrintf("Set %d", i),
                           {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
                       .value());
  }
  base::RunLoop().RunUntilIdle();
  CheckProductSpecificationsExists(sets);

  const base::Uuid& uuid_to_modify = sets[1].uuid();
  auto it = entries().find(uuid_to_modify.AsLowercaseString());
  entries().erase(it);
  EXPECT_EQ(std::nullopt, service()->SetName(uuid_to_modify, "New name"));
}

TEST_F(ProductSpecificationsServiceTest, TestGetByUuidMultiSpecifics) {
  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(service()
                     ->AddProductSpecificationsSet(
                         "Set 0", {UrlInfo(GURL("https://a.example.com"), u""),
                                   UrlInfo(GURL("https://b.example.com"), u"")})
                     .value());
  sets.push_back(service()
                     ->AddProductSpecificationsSet(
                         "Set 1", {UrlInfo(GURL("https://c.example.com"), u""),
                                   UrlInfo(GURL("https://d.example.com"), u"")})
                     .value());
  sets.push_back(service()
                     ->AddProductSpecificationsSet(
                         "Set 2", {UrlInfo(GURL("https://e.example.com"), u""),
                                   UrlInfo(GURL("https://f.example.com"), u"")})
                     .value());

  CheckProductSpecificationsExists(sets);

  const base::Uuid& uuid_to_get = sets[1].uuid();
  std::optional<ProductSpecificationsSet> set =
      service()->GetSetByUuid(uuid_to_get);
  EXPECT_NE(std::nullopt, set);
  EXPECT_EQ("Set 1", set.value().name());
  EXPECT_EQ(2u, set.value().urls().size());
  EXPECT_EQ(GURL("https://c.example.com"), set.value().urls()[0]);
  EXPECT_EQ(GURL("https://d.example.com"), set.value().urls()[1]);
}

TEST_F(ProductSpecificationsServiceTest,
       TestGetByUuidMultiSpecificsTopLevelAbsent) {
  // TODO(crbug.com/353256094) Ensure flag is set in constructor of test.
  std::vector<ProductSpecificationsSet> sets;
  for (int i = 0; i < 2; i++) {
    sets.push_back(service()
                       ->AddProductSpecificationsSet(
                           base::StringPrintf("Set %d", i),
                           {UrlInfo(GURL("https://a.example.com"), u""),
                            UrlInfo(GURL("https://b.example.com"), u"")})
                       .value());
  }

  CheckProductSpecificationsExists(sets);

  const base::Uuid& uuid_to_get = sets[1].uuid();
  auto it = entries().find(uuid_to_get.AsLowercaseString());
  // Remove top level entry to simulate losing it (e.g. failed to sync
  // top level entry). At this point we can't reconstruct a
  // ProductSpecificationsSet so the GetSetByUuid API returns std:nullopt.
  entries().erase(it);
  EXPECT_EQ(std::nullopt, service()->GetSetByUuid(uuid_to_get));
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest,
       TestGetProductSpecifications) {
  EXPECT_TRUE(service()->GetAllProductSpecifications().empty());
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest, TestGetSetByUuid) {
  EXPECT_EQ(std::nullopt, service()->GetSetByUuid(base::Uuid::ParseLowercase(
                              "50000000-0000-0000-0000-000000000000")));
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest,
       TestAddProductSpecificationsSet) {
  EXPECT_EQ(std::nullopt,
            service()->AddProductSpecificationsSet(
                "Name", {UrlInfo(GURL("https://a.example.com"), u""),
                         UrlInfo(GURL("https://b.example.com"), u"")}));
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest, TestSetUrls) {
  EXPECT_EQ(std::nullopt,
            service()->SetUrls(base::Uuid::ParseLowercase(
                                   "50000000-0000-0000-0000-000000000000"),
                               {UrlInfo(GURL("https://a.example.com"), u""),
                                UrlInfo(GURL("https://b.example.com"), u"")}));
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest, TestSetName) {
  EXPECT_EQ(std::nullopt,
            service()->SetName(base::Uuid::ParseLowercase(
                                   "50000000-0000-0000-0000-000000000000"),
                               "new name"));
}

TEST_F(ProductSpecificationsServiceSyncDisabledTest, TestDelete) {
  EXPECT_TRUE(entries().find(initial_set()->uuid().AsLowercaseString()) !=
              entries().end());
  EXPECT_CALL(*observer(), OnProductSpecificationsSetRemoved(testing::_))
      .Times(0);
  service()->DeleteProductSpecificationsSet(
      initial_set()->uuid().AsLowercaseString());
  EXPECT_TRUE(entries().find(initial_set()->uuid().AsLowercaseString()) !=
              entries().end());
}

TEST_F(ProductSpecificationsServiceTest, TestMultiSpecificsAdded) {
  std::string expected_name = "New set";
  std::vector<UrlInfo> expected_urls = {
      UrlInfo(GURL("https://a.example.com"), u""),
      UrlInfo(GURL("https://b.example.com"), u"")};

  // Add ProductSpecificationsSet to acquire underlying specifics which are
  // then used to simulate the specifics being sent from the sync server.
  std::optional<ProductSpecificationsSet> expected_set =
      service()->AddProductSpecificationsSet(expected_name, expected_urls);
  std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                        syncer::EntityChange::ChangeType>>
      to_change;
  for (const auto& [_, specifics] : entries()) {
    to_change.emplace_back(specifics, syncer::EntityChange::ACTION_ADD);
  }
  // Now specifics have been acquired, delete ProductSpecificationsSet to
  // ensure the bridge reacts appropriately to the specifics being sent
  // from the server (i.e. reconstructs ProductSpecificationsSet and passes
  // it to the observer method).
  service()->DeleteProductSpecificationsSet(
      expected_set.value().uuid().AsLowercaseString());
  VerifyProductSpecificationsSet(expected_set.value().uuid(), std::nullopt);

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetAdded(
                  HasProductSpecsNameUrlInfos(expected_name, expected_urls)))
      .Times(1);
  // Simulate specifics being sent from the sync server via
  // 'ApplyIncrementalSyncChanges'
  ApplyIncrementalSyncChangesForTesting(to_change);
  VerifyProductSpecificationsSet(expected_set.value().uuid(), expected_set);
}

TEST_F(ProductSpecificationsServiceTest, TestMultiSpecificsSetUrls) {
  // TODO(crbug.com/353979028) investigate re-writing tests in
  // crrev.com/c/5713999 as unit tests in the bridge unit tests.

  // Add ProductSpecificationsSet, then update its urls to acquire the
  // underlying specifics which are then used to simulate the specifics
  // being sent from the sync server.
  std::optional<ProductSpecificationsSet> set_to_modify =
      service()->AddProductSpecificationsSet(
          "New set", {UrlInfo(GURL("https://a.example.com"), u""),
                      UrlInfo(GURL("https://b.example.com"), u"")});
  std::vector<sync_pb::ProductComparisonSpecifics> to_remove;
  // Item level specifics should be removed as part of simulating a
  // SetUrls(...), then syncing to another device.
  for (auto& [_, specifics] : entries()) {
    if (specifics.has_product_comparison_item()) {
      to_remove.push_back(specifics);
    }
  }
  service()->SetUrls(set_to_modify->uuid(),
                     {UrlInfo(GURL("https://x.example.com"), u""),
                      UrlInfo(GURL("https://y.example.com"), u""),
                      UrlInfo(GURL("https://z.example.com"), u"")});
  std::vector<sync_pb::ProductComparisonSpecifics> to_add;
  // New Item level specifics should be added as the other part of simulating
  // SetUrls(...) then syncing to another device.
  for (auto& [_, specifics] : entries()) {
    if (specifics.has_product_comparison_item()) {
      to_add.push_back(specifics);
    }
  }

  std::optional<ProductSpecificationsSet> expected_set =
      service()->GetSetByUuid(set_to_modify->uuid());

  // Change underlying specifics back to before SetUrls(...) was called
  AddSpecifics(to_remove);
  DeleteSpecifics(to_add);
  std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                        syncer::EntityChange::ChangeType>>
      to_change;
  for (auto& specific : to_add) {
    to_change.emplace_back(specific, syncer::EntityChange::ACTION_ADD);
  }
  for (auto& specific : to_remove) {
    to_change.emplace_back(specific, syncer::EntityChange::ACTION_DELETE);
  }

  EXPECT_CALL(
      *observer(),
      OnProductSpecificationsSetUpdate(
          HasProductSpecsNameUrl(set_to_modify->name(), set_to_modify->urls()),
          HasProductSpecsNameUrl(expected_set->name(), expected_set->urls())))
      .Times(1);

  // Simulate add/delete specifics over the network
  ApplyIncrementalSyncChangesForTesting(to_change);
  VerifyProductSpecificationsSet(expected_set->uuid(), expected_set);
}

TEST_F(ProductSpecificationsServiceTest, TestMultiSpecificsSetNameUpdate) {
  std::optional<ProductSpecificationsSet> new_set =
      service()->AddProductSpecificationsSet(
          "New set", {UrlInfo(GURL("https://a.example.com"), u""),
                      UrlInfo(GURL("https://b.example.com"), u"")});
  std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                        syncer::EntityChange::ChangeType>>
      to_change;
  for (const auto& [_, specifics] : entries()) {
    // To simulate a name update find the top level specific and change
    // its name.
    if (specifics.has_product_comparison()) {
      sync_pb::ProductComparisonSpecifics updated_specifics = specifics;
      updated_specifics.mutable_product_comparison()->set_name("New name");
      updated_specifics.set_update_time_unix_epoch_millis(
          new_set->update_time().InMillisecondsSinceUnixEpoch() +
          base::Time::kMillisecondsPerDay);
      to_change.emplace_back(updated_specifics,
                             syncer::EntityChange::ACTION_UPDATE);
    }
  }
  // After specifics update is synced new set is expected to have
  // - Same UUID
  // - Same creation time
  // - New update itme (latest specifics update time)
  // - Same URLs
  // - New name
  ProductSpecificationsSet expected_set(
      new_set->uuid().AsLowercaseString(),
      new_set->creation_time().InMillisecondsSinceUnixEpoch(),
      to_change[0].first.update_time_unix_epoch_millis(),
      {GURL("https://a.example.com"), GURL("https://b.example.com")},
      "New name");

  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetUpdate(
                  testing::_, HasProductSpecsNameUrl(expected_set.name(),
                                                     expected_set.urls())))
      .Times(1);
  ApplyIncrementalSyncChangesForTesting(to_change);
  VerifyProductSpecificationsSet(expected_set.uuid(),
                                 std::make_optional(expected_set));
}

TEST_F(ProductSpecificationsServiceTest, TestMultiSpecificsDelete) {
  std::optional<ProductSpecificationsSet> new_set =
      service()->AddProductSpecificationsSet(
          "New set", {UrlInfo(GURL("https://a.example.com"), u""),
                      UrlInfo(GURL("https://b.example.com"), u"")});
  std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                        syncer::EntityChange::ChangeType>>
      to_change;
  // Acquire underlying specifics of new set so a delete operation from the
  // server can be simulated on said specifics.
  for (const auto& [_, specifics] : entries()) {
    if (specifics.has_product_comparison() ||
        specifics.has_product_comparison_item()) {
      to_change.emplace_back(specifics, syncer::EntityChange::ACTION_DELETE);
    }
  }
  EXPECT_CALL(*observer(),
              OnProductSpecificationsSetRemoved(
                  HasProductSpecsNameUrl(new_set->name(), new_set->urls())))
      .Times(1);
  ApplyIncrementalSyncChangesForTesting(to_change);
  VerifyProductSpecificationsSet(new_set->uuid(), std::nullopt);
}

TEST_F(ProductSpecificationsServiceTest, TestMigration) {
  std::string expected_name = "test_name";
  std::vector<GURL> expected_urls = {GURL("https://a.example.com/"),
                                     GURL("https://b.example.com/")};
  base::Uuid expected_uuid =
      base::Uuid::ParseLowercase("50000000-0000-0000-0000-000000000000");
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid(expected_uuid.AsLowercaseString());
  specifics.set_name(expected_name);
  for (const GURL& url : expected_urls) {
    specifics.add_data()->set_url(url.spec());
  }
  AddSpecifics({specifics});
  EnableMigrateProductSpecificationsSets();
  EXPECT_EQ(std::nullopt, service()->GetSetByUuid(expected_uuid));
  MigrateLegacySpecificsIfApplicable();
  std::optional<ProductSpecificationsSet> migrated_set =
      service()->GetSetByUuid(expected_uuid);
  EXPECT_NE(std::nullopt, migrated_set);
  EXPECT_EQ(expected_uuid, migrated_set->uuid());
  EXPECT_EQ(expected_urls, migrated_set->urls());
  // TODO(crbug.com/353746117) add in time checks
  EXPECT_EQ(expected_name, migrated_set->name());
}

TEST_F(ProductSpecificationsServiceTest,
       TestMultiSpecificsIgnoredForSingleSpecificsFlagOff) {
  DisableMultiSpecFlag();
  std::string multi_specs_set_uuid = "50000000-0000-0000-0000-000000000000";
  sync_pb::ProductComparisonSpecifics top_level;
  top_level.set_uuid(multi_specs_set_uuid);
  top_level.mutable_product_comparison()->set_name("test_name");

  sync_pb::ProductComparisonSpecifics item_level;
  item_level.set_uuid("30000000-0000-0000-0000-000000000000");
  item_level.mutable_product_comparison_item()->set_product_comparison_uuid(
      "50000000-0000-0000-0000-000000000000");
  item_level.mutable_product_comparison_item()->set_url(
      "https://a.example.com/");
  *item_level.mutable_product_comparison_item()->mutable_unique_position() =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto();

  AddSpecifics({top_level, item_level});
  std::vector<ProductSpecificationsSet> sets =
      service()->GetAllProductSpecifications();

  EXPECT_EQ(sets.end(), base::ranges::find_if(sets, [&multi_specs_set_uuid](
                                                        const auto& query_set) {
              return query_set.uuid().AsLowercaseString() ==
                     multi_specs_set_uuid;
            }));
}

TEST_F(ProductSpecificationsServiceTest,
       TestMultiSpecificsIgnoredForSingleSpecificsFlagOn) {
  std::string multi_specs_set_uuid = "50000000-0000-0000-0000-000000000000";
  sync_pb::ProductComparisonSpecifics top_level;
  top_level.set_uuid(multi_specs_set_uuid);
  top_level.mutable_product_comparison()->set_name("test_name");

  sync_pb::ProductComparisonSpecifics item_level;
  item_level.set_uuid("30000000-0000-0000-0000-000000000000");
  item_level.mutable_product_comparison_item()->set_product_comparison_uuid(
      "50000000-0000-0000-0000-000000000000");
  item_level.mutable_product_comparison_item()->set_url(
      "https://a.example.com/");
  *item_level.mutable_product_comparison_item()->mutable_unique_position() =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto();

  AddSpecifics({top_level, item_level});
  std::vector<ProductSpecificationsSet> multi_specifics_sets =
      service()->GetAllProductSpecifications();

  const auto iter = base::ranges::find_if(
      multi_specifics_sets, [&multi_specs_set_uuid](const auto& query_set) {
        return query_set.uuid().AsLowercaseString() == multi_specs_set_uuid;
      });
  EXPECT_TRUE(iter != multi_specifics_sets.end());
  EXPECT_EQ(multi_specs_set_uuid, iter->uuid().AsLowercaseString());
  EXPECT_EQ("test_name", iter->name());
  EXPECT_EQ(1u, iter->urls().size());
  EXPECT_EQ("https://a.example.com/", iter->urls()[0].spec());
}

TEST_F(ProductSpecificationsServiceWithTitleTest, TestTitle) {
  const ProductSpecificationsSet added_set_with_titles =
      service()
          ->AddProductSpecificationsSet(
              kProductSpecsName,
              {UrlInfo(GURL(kProductOneUrl), u"product one title"),
               UrlInfo(GURL(kProductTwoUrl), u"product two title")})
          .value();
  std::optional<ProductSpecificationsSet> set_with_titles =
      service()->GetSetByUuid(added_set_with_titles.uuid());
  EXPECT_TRUE(set_with_titles.has_value());
  for (const auto& expected_title :
       {u"product one title", u"product two title"}) {
    const auto iter =
        base::ranges::find_if(set_with_titles->url_infos(),
                              [&expected_title](const UrlInfo& query_url_info) {
                                return query_url_info.title == expected_title;
                              });
    EXPECT_TRUE(iter != set_with_titles->url_infos().end());
  }
}

TEST_F(ProductSpecificationsServiceWithTitleTest, SetUrlWithTitle) {
  const ProductSpecificationsSet added_set =
      service()
          ->AddProductSpecificationsSet(
              kProductSpecsName,
              {UrlInfo(GURL(kProductOneUrl), u"product one title"),
               UrlInfo(GURL(kProductTwoUrl), u"product two title")})
          .value();
  service()->SetUrls(added_set.uuid(),
                     {UrlInfo(GURL("https://x.example.com/"), u"product x"),
                      UrlInfo(GURL("https://y.example.com/"), u"product y")});

  std::optional<ProductSpecificationsSet> updated_set =
      service()->GetSetByUuid(added_set.uuid());
  EXPECT_TRUE(updated_set.has_value());
  std::map<GURL, UrlInfo> lookup;
  for (const auto& url_info : updated_set->url_infos()) {
    lookup[url_info.url] = url_info;
  }
  EXPECT_EQ(2u, lookup.size());
  EXPECT_EQ(u"product x", lookup[GURL("https://x.example.com/")].title);
  EXPECT_EQ(u"product y", lookup[GURL("https://y.example.com/")].title);
}

}  // namespace commerce
