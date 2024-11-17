// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_namespace_impl.h"

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/storage_area_test_util.h"
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

MATCHER(OKStatus, "Equality matcher for type OK leveldb::Status") {
  return arg.ok();
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(leveldb::Status));
};

class SessionStorageNamespaceImplTest
    : public testing::Test,
      public SessionStorageNamespaceImpl::Delegate {
 public:
  SessionStorageNamespaceImplTest()
      : test_namespace_id1_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
        test_namespace_id2_(
            base::Uuid::GenerateRandomV4().AsLowercaseString()) {}
  ~SessionStorageNamespaceImplTest() override = default;

  void RunBatch(std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks) {
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    database_->RunBatchDatabaseTasks(
        std::move(tasks),
        base::BindLambdaForTesting([&](leveldb::Status) { loop.Quit(); }));
    loop.Run();
  }

  void SetUp() override {
    // Create a database that already has a namespace saved.
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::OpenInMemory(
        std::nullopt, "SessionStorageNamespaceImplTest",
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        base::BindLambdaForTesting([&](leveldb::Status) { loop.Quit(); }));
    loop.Run();

    metadata_.SetupNewDatabase();
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto entry = metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);
    auto map_id =
        metadata_.RegisterNewMap(entry, test_storage_key1_, &save_tasks);
    DCHECK(map_id->KeyPrefix() == StdStringToUint8Vector("map-0-"));
    RunBatch(std::move(save_tasks));

    // Put some data in one of the maps.
    base::RunLoop put_loop;
    database_->database().PostTaskWithThisObject(
        base::BindLambdaForTesting([&](const DomStorageDatabase& db) {
          ASSERT_TRUE(db.Put(StdStringToUint8Vector("map-0-key1"),
                             StdStringToUint8Vector("data1"))
                          .ok());
          put_loop.Quit();
        }));
    put_loop.Run();
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

  scoped_refptr<SessionStorageMetadata::MapData> RegisterNewAreaMap(
      NamespaceEntry namespace_entry,
      const blink::StorageKey& storage_key) {
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto map_data =
        metadata_.RegisterNewMap(namespace_entry, storage_key, &save_tasks);
    RunBatch(std::move(save_tasks));
    return map_data;
  }

  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      const std::string& destination_namespace,
      const SessionStorageNamespaceImpl::StorageKeyAreas& areas_to_clone)
      override {
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto namespace_entry =
        metadata_.GetOrCreateNamespaceEntry(destination_namespace);
    metadata_.RegisterShallowClonedNamespace(source_namespace, namespace_entry,
                                             &save_tasks);
    RunBatch(std::move(save_tasks));

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
      const std::vector<uint8_t>& map_number_as_bytes) override {
    auto it = data_maps_.find(map_number_as_bytes);
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
  std::map<std::vector<uint8_t>, scoped_refptr<SessionStorageDataMap>>
      data_maps_;

  testing::StrictMock<MockListener> listener_;
  std::unique_ptr<AsyncDomStorageDatabase> database_;
};

TEST_F(SessionStorageNamespaceImplTest, MetadataLoad) {
  // Exercises creation, population, binding, and getting all data.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::StorageArea> leveldb_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, MetadataLoadWithMapOperations) {
  // Exercises creation, population, binding, and a map operation, and then
  // getting all the data.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::StorageArea> leveldb_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  test::PutSync(leveldb_1.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);

  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, CloneBeforeBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  SessionStorageNamespaceImpl* namespace_impl1 =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);
  SessionStorageNamespaceImpl* namespace_impl2 =
      CreateSessionStorageNamespaceImpl(test_namespace_id2_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl1->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();

  ASSERT_TRUE(namespace_impl2->IsPopulated());

  mojo::Remote<blink::mojom::StorageArea> leveldb_2;
  namespace_impl2->OpenArea(test_storage_key1_,
                            leveldb_2.BindNewPipeAndPassReceiver());

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  auto commit_callback = base::BarrierClosure(2, commit_loop.QuitClosure());
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([&](auto error) { commit_callback.Run(); }));
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  test::PutSync(leveldb_2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_2.get(), &data));
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, CloneAfterBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  // Unlike the test above, we create a new area for the test_storage_key2_ in
  // the new namespace.
  SessionStorageNamespaceImpl* namespace_impl1 =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);
  SessionStorageNamespaceImpl* namespace_impl2 =
      CreateSessionStorageNamespaceImpl(test_namespace_id2_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl1->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver());

  // Set that we are waiting for clone, so binding is possible.
  namespace_impl2->SetPendingPopulationFromParentNamespace(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  // Get a new area.
  mojo::Remote<blink::mojom::StorageArea> leveldb_n2_o1;
  mojo::Remote<blink::mojom::StorageArea> leveldb_n2_o2;
  namespace_impl2->OpenArea(test_storage_key1_,
                            leveldb_n2_o1.BindNewPipeAndPassReceiver());
  namespace_impl2->OpenArea(test_storage_key2_,
                            leveldb_n2_o2.BindNewPipeAndPassReceiver());

  // Finally do the clone.
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();
  ASSERT_TRUE(namespace_impl2->IsPopulated());

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  test::PutSync(leveldb_n2_o2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), std::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  data.clear();
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o2.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, RemoveStorageKeyData) {
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::StorageArea> leveldb_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());

  // Create an observer to make sure the deletion is observed.
  testing::StrictMock<test::MockLevelDBObserver> mock_observer;
  leveldb_1->AddObserver(mock_observer.Bind());
  leveldb_1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted(true, "\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  namespace_impl->RemoveStorageKeyData(test_storage_key1_, base::DoNothing());
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Check that the observer was notified.
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, RemoveStorageKeyDataWithoutBinding) {
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  namespace_impl->RemoveStorageKeyData(test_storage_key1_, base::DoNothing());
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplTest, PurgeUnused) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::StorageArea> leveldb_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(namespace_impl->HasAreaForStorageKey(test_storage_key1_));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  leveldb_1.reset();
  EXPECT_TRUE(namespace_impl->HasAreaForStorageKey(test_storage_key1_));

  namespace_impl->FlushAreasForTesting();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(namespace_impl->HasAreaForStorageKey(test_storage_key1_));

  namespaces_.clear();
}

}  // namespace

TEST_F(SessionStorageNamespaceImplTest, ReopenClonedAreaAfterPurge) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImpl* namespace_impl =
      CreateSessionStorageNamespaceImpl(test_namespace_id1_);

  SessionStorageDataMap* data_map;
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .WillOnce(testing::SaveArg<1>(&data_map));

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::StorageArea> leveldb_1;
  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());

  // Save the data map, as if we did a clone:
  data_maps_[data_map->map_data()->MapNumberAsBytes()] = data_map;

  leveldb_1.reset();
  namespace_impl->FlushAreasForTesting();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(namespace_impl->HasAreaForStorageKey(test_storage_key1_));

  namespace_impl->OpenArea(test_storage_key1_,
                           leveldb_1.BindNewPipeAndPassReceiver());
  leveldb_1.FlushForTesting();

  EXPECT_EQ(namespace_impl->storage_key_areas_[test_storage_key1_]->data_map(),
            data_map);

  data_maps_.clear();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);

  namespaces_.clear();
}

}  // namespace storage
