// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_namespace_impl.h"

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/dom_storage/test_support/storage_area_test_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

using NamespaceEntry = SessionStorageMetadata::NamespaceEntry;

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

MATCHER(OKStatus, "Equality matcher for type OK DbStatus") {
  return arg.ok();
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;
  MOCK_METHOD2(OnDataMapCreation,
               void(int64_t map_id, SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(int64_t map_id));
  MOCK_METHOD1(OnCommitResult, void(DbStatus));
};

class SessionStorageNamespaceImplTest
    : public base::test::WithFeatureOverride,
      public testing::Test,
      public SessionStorageNamespaceImpl::Delegate {
 public:
  SessionStorageNamespaceImplTest()
      : base::test::WithFeatureOverride(kDomStorageSqlite),
        test_namespace_id1_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
        test_namespace_id2_(
            base::Uuid::GenerateRandomV4().AsLowercaseString()) {}
  ~SessionStorageNamespaceImplTest() override = default;

  void SetUp() override {
    // Create an in-memory database that already has a namespace saved.
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage,
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt,
        base::BindLambdaForTesting([&](DbStatus) { loop.Quit(); }));
    loop.Run();

    auto map_locator =
        metadata_.RegisterNewMap(test_namespace_id1_, test_storage_key1_);
    EXPECT_EQ(map_locator->map_id().value(), 0);

    // Put some data in one of the maps.
    FakeCommitter committer(database_.get(), map_locator->Clone());
    committer.PutMapKeyValueSync(StdStringToUint8Vector("key1"),
                                 StdStringToUint8Vector("data1"));
  }

  // Creates a SessionStorageNamespaceImpl, saves it in the namespaces_ map,
  // and returns a pointer to the object.
  SessionStorageNamespaceImpl* CreateSessionStorageNamespaceImpl(
      const std::string& namespace_id) {
    DCHECK(namespaces_.find(namespace_id) == namespaces_.end());
    SessionStorageAreaImpl::RegisterNewAreaMap map_id_callback =
        base::BindRepeating(
            &SessionStorageNamespaceImplTest::RegisterNewAreaMap,
            base::Unretained(this));

    auto namespace_impl = std::make_unique<SessionStorageNamespaceImpl>(
        namespace_id, &listener_, std::move(map_id_callback), this);
    auto* namespace_impl_ptr = namespace_impl.get();
    namespaces_[namespace_id] = std::move(namespace_impl);
    return namespace_impl_ptr;
  }

  scoped_refptr<DomStorageDatabase::SharedMapLocator> RegisterNewAreaMap(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key) {
    return metadata_.RegisterNewMap(namespace_id, storage_key);
  }

  void RegisterShallowClonedNamespace(
      const std::string& source_namespace,
      const std::string& destination_namespace,
      const SessionStorageNamespaceImpl::StorageKeyAreas& areas_to_clone)
      override {
    auto source_namespace_entry =
        metadata_.GetOrCreateNamespaceEntry(source_namespace);
    auto namespace_entry =
        metadata_.GetOrCreateNamespaceEntry(destination_namespace);
    metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                             namespace_entry);

    ASSERT_NO_FATAL_FAILURE(PutMetadataSync(
        *database_,
        SessionStorageMetadata::ToDomStorageMetadata(namespace_entry)));

    auto it = namespaces_.find(destination_namespace);
    if (it == namespaces_.end()) {
      auto* namespace_impl =
          CreateSessionStorageNamespaceImpl(destination_namespace);
      namespace_impl->PopulateAsClone(database_.get(), namespace_entry,
                                      areas_to_clone);
      return;
    }
    it->second->PopulateAsClone(database_.get(), namespace_entry,
                                areas_to_clone);
  }

  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      int64_t map_id) override {
    auto it = data_maps_.find(map_id);
    if (it == data_maps_.end())
      return nullptr;
    return it->second;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const std::string test_namespace_id1_;
  const std::string test_namespace_id2_;
  const blink::StorageKey test_storage_key1_ =
      blink::StorageKey::CreateFromStringForTesting("https://host1.com/");
  const blink::StorageKey test_storage_key2_ =
      blink::StorageKey::CreateFromStringForTesting("https://host2.com/");
  SessionStorageMetadata metadata_;

  std::map<std::string, std::unique_ptr<SessionStorageNamespaceImpl>>
      namespaces_;
  std::map</*map_id=*/int64_t, scoped_refptr<SessionStorageDataMap>> data_maps_;

  testing::StrictMock<MockListener> listener_;
  std::unique_ptr<AsyncDomStorageDatabase> database_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    SessionStorageNamespaceImplTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<SessionStorageNamespaceImplTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

TEST_P(SessionStorageNamespaceImplTest, MetadataLoad) {
  // Exercises creation, population, binding, and getting all data.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::StorageArea> storage_area_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area_1.get());
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, MetadataLoadWithMapOperations) {
  // Exercises creation, population, binding, and a map operation, and then
  // getting all the data.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::StorageArea> storage_area_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce([&](auto error) { commit_loop.Quit(); });
  test::PutSync(storage_area_1.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area_1.get());
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);

  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, CloneBeforeBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  SessionStorageNamespaceImpl* namespace_impl1 =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);
  SessionStorageNamespaceImpl* namespace_impl2 =
      CreateSessionStorageNamespaceImpl(test_namespace_id2_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl1->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();

  ASSERT_TRUE(namespace_impl2->IsPopulated());

  SessionStorageMetadata::NamespaceEntry namespace_entry2 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_);

  mojo::Remote<blink::mojom::StorageArea> storage_area_2;
  namespace_impl2->OpenArea(test_storage_key1_,
                            storage_area_2.BindNewPipeAndPassReceiver(),
                            namespace_entry2);

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  auto commit_callback = base::BarrierClosure(2, commit_loop.QuitClosure());
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(2)
      .WillRepeatedly([&](auto error) { commit_callback.Run(); });
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);
  test::PutSync(storage_area_2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area_2.get());
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);
  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, CloneAfterBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  // Unlike the test above, we create a new area for the test_storage_key2_ in
  // the new namespace.
  SessionStorageNamespaceImpl* namespace_impl1 =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);
  SessionStorageNamespaceImpl* namespace_impl2 =
      CreateSessionStorageNamespaceImpl(test_namespace_id2_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  namespace_impl1->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver());

  // Set that we are waiting for clone, so binding is possible.
  namespace_impl2->SetPendingPopulationFromParentNamespace(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry2 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_);

  // Get a new area.
  mojo::Remote<blink::mojom::StorageArea> storage_area_n2_o1;
  mojo::Remote<blink::mojom::StorageArea> storage_area_n2_o2;
  namespace_impl2->OpenArea(test_storage_key1_,
                            storage_area_n2_o1.BindNewPipeAndPassReceiver(),
                            namespace_entry2);

  namespace_impl2->OpenArea(test_storage_key2_,
                            storage_area_n2_o2.BindNewPipeAndPassReceiver(),
                            namespace_entry2);

  // Finally do the clone.
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();
  ASSERT_TRUE(namespace_impl2->IsPopulated());

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce([&](auto error) { commit_loop.Quit(); });
  test::PutSync(storage_area_n2_o2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area_n2_o1.get());
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  data = test::GetAllSync(storage_area_n2_o2.get());
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);
  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, RemoveStorageKeyData) {
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::StorageArea> storage_area_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);

  // Create an observer to make sure the deletion is observed.
  testing::StrictMock<test::MockStorageAreaObserver> mock_observer;
  storage_area_1->AddObserver(mock_observer.Bind());
  storage_area_1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted(true, "\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce([&](auto error) { commit_loop.Quit(); });
  namespace_impl->RemoveStorageKeyData(test_storage_key1_, base::DoNothing());
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area_1.get());
  EXPECT_EQ(0ul, data.size());

  // Check that the observer was notified.
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, RemoveStorageKeyDataWithoutBinding) {
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  namespace_impl->RemoveStorageKeyData(test_storage_key1_, base::DoNothing());
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  namespaces_.clear();
}

TEST_P(SessionStorageNamespaceImplTest, PurgeUnused) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::StorageArea> storage_area_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);
  EXPECT_TRUE(
      namespace_impl->HasAreaForStorageKeyForTesting(test_storage_key1_));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  storage_area_1.reset();
  EXPECT_TRUE(
      namespace_impl->HasAreaForStorageKeyForTesting(test_storage_key1_));

  namespace_impl->FlushAreasForTesting();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(
      namespace_impl->HasAreaForStorageKeyForTesting(test_storage_key1_));

  namespaces_.clear();
}

}  // namespace

TEST_P(SessionStorageNamespaceImplTest, ReopenClonedAreaAfterPurge) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  SessionStorageDataMap* data_map;
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_))
      .WillOnce(testing::SaveArg<1>(&data_map));

  SessionStorageMetadata::NamespaceEntry namespace_entry1 =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);

  namespace_impl->PopulateFromMetadata(database_.get(), namespace_entry1);

  mojo::Remote<blink::mojom::StorageArea> storage_area_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);

  // Save the data map, as if we did a clone:
  data_maps_[data_map->map_locator().map_id().value()] = data_map;

  storage_area_1.reset();
  namespace_impl->FlushAreasForTesting();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(
      namespace_impl->HasAreaForStorageKeyForTesting(test_storage_key1_));

  namespace_impl->OpenArea(test_storage_key1_,
                           storage_area_1.BindNewPipeAndPassReceiver(),
                           namespace_entry1);
  storage_area_1.FlushForTesting();

  EXPECT_EQ(namespace_impl->storage_key_areas_[test_storage_key1_]->data_map(),
            data_map);

  data_maps_.clear();

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);

  namespaces_.clear();
}

}  // namespace storage
