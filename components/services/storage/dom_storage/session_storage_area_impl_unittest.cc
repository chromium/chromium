// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_area_impl.h"

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/dom_storage/test_support/storage_area_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

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

class SessionStorageAreaImplTest : public base::test::WithFeatureOverride,
                                   public testing::Test {
 public:
  SessionStorageAreaImplTest()
      : base::test::WithFeatureOverride(kDomStorageSqlite),
        test_namespace_id1_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
        test_namespace_id2_(
            base::Uuid::GenerateRandomV4().AsLowercaseString()) {
    // Create an in-memory database.
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage,
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt, base::DoNothing());

    // Create a map with one key/value pair in the database.
    scoped_refptr<DomStorageDatabase::SharedMapLocator> map_locator =
        metadata_.RegisterNewMap(test_namespace_id1_, test_storage_key1_);
    CHECK_EQ(map_locator->map_id().value(), 0);

    FakeCommitter committer(database_.get(), map_locator->Clone());
    committer.PutMapKeyValueSync(StdStringToUint8Vector("key1"),
                                 StdStringToUint8Vector("data1"));
  }

  ~SessionStorageAreaImplTest() override = default;

  scoped_refptr<DomStorageDatabase::SharedMapLocator> RegisterNewAreaMap(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key) {
    return metadata_.RegisterNewMap(namespace_id, storage_key);
  }

  SessionStorageAreaImpl::RegisterNewAreaMap GetRegisterNewAreaMapCallback() {
    return base::BindRepeating(&SessionStorageAreaImplTest::RegisterNewAreaMap,
                               base::Unretained(this));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const std::string test_namespace_id1_;
  const std::string test_namespace_id2_;
  const blink::StorageKey test_storage_key1_ =
      blink::StorageKey::CreateFromStringForTesting("https://host1.com:1/");
  const blink::StorageKey test_storage_key2_ =
      blink::StorageKey::CreateFromStringForTesting("https://host2.com:2/");
  std::unique_ptr<AsyncDomStorageDatabase> database_;
  SessionStorageMetadata metadata_;

  testing::StrictMock<MockListener> listener_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    SessionStorageAreaImplTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<SessionStorageAreaImplTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

}  // namespace

TEST_P(SessionStorageAreaImplTest, BasicUsage) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::Remote<blink::mojom::StorageArea> storage_area;
  session_storage_area->Bind(storage_area.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area.get());
  ASSERT_EQ(1ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
}

TEST_P(SessionStorageAreaImplTest, ExplicitlyEmptyMap) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateEmpty(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::Remote<blink::mojom::StorageArea> storage_area;
  session_storage_area->Bind(storage_area.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area.get());
  ASSERT_EQ(0ul, data.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
}

TEST_P(SessionStorageAreaImplTest, DoubleBind) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::Remote<blink::mojom::StorageArea> storage_area1;
  base::RunLoop loop;
  session_storage_area->Bind(storage_area1.BindNewPipeAndPassReceiver());

  // Get data from the first binding.
  std::vector<blink::mojom::KeyValuePtr> data1 =
      test::GetAllSync(storage_area1.get());
  ASSERT_EQ(1ul, data1.size());

  // Check that we can bind twice and get data from the second binding.
  mojo::Remote<blink::mojom::StorageArea> storage_area2;
  session_storage_area->Bind(storage_area2.BindNewPipeAndPassReceiver());
  std::vector<blink::mojom::KeyValuePtr> data2 =
      test::GetAllSync(storage_area2.get());
  ASSERT_EQ(1ul, data2.size());

  // Check that we can still get data from the first binding.
  std::vector<blink::mojom::KeyValuePtr> data3 =
      test::GetAllSync(storage_area1.get());
  ASSERT_EQ(1ul, data3.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
}

TEST_P(SessionStorageAreaImplTest, Cloning) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area1 = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  SessionStorageMetadata::NamespaceEntry clone_entry =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_);
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), clone_entry);
  database_->PutMetadata(
      SessionStorageMetadata::ToDomStorageMetadata(clone_entry),
      base::DoNothing());
  auto session_storage_area2 =
      session_storage_area1->Clone(test_namespace_id2_);

  mojo::Remote<blink::mojom::StorageArea> storage_area1;
  session_storage_area1->Bind(storage_area1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::StorageArea> storage_area2;
  session_storage_area2->Bind(storage_area2.BindNewPipeAndPassReceiver());

  // Same maps are used.
  EXPECT_EQ(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  // The |Put| call will fork the maps.
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(testing::AnyNumber());
  EXPECT_TRUE(test::PutSync(storage_area2.get(), StdStringToUint8Vector("key2"),
                            StdStringToUint8Vector("data2"), std::nullopt, ""));

  // The maps were forked on the above put.
  EXPECT_NE(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  // Check map 1 data.
  std::vector<blink::mojom::KeyValuePtr> data =
      test::GetAllSync(storage_area1.get());
  ASSERT_EQ(1ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  // Check map 2 data.
  data.clear();
  data = test::GetAllSync(storage_area2.get());
  ASSERT_EQ(2ul, data.size());
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(std::ranges::contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);

  session_storage_area1 = nullptr;
  session_storage_area2 = nullptr;
}

TEST_P(SessionStorageAreaImplTest, NotifyAllDeleted) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area1 = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::Remote<blink::mojom::StorageArea> storage_area1;
  session_storage_area1->Bind(storage_area1.BindNewPipeAndPassReceiver());

  testing::StrictMock<test::MockStorageAreaObserver> mock_observer;
  storage_area1->AddObserver(mock_observer.Bind());
  storage_area1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted(true, "\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  session_storage_area1->NotifyObserversAllDeleted();
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
}

TEST_P(SessionStorageAreaImplTest, DeleteAllOnShared) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area1 = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  SessionStorageMetadata::NamespaceEntry clone_entry =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_);
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), clone_entry);
  database_->PutMetadata(
      SessionStorageMetadata::ToDomStorageMetadata(clone_entry),
      base::DoNothing());
  auto session_storage_area2 =
      session_storage_area1->Clone(test_namespace_id2_);

  mojo::Remote<blink::mojom::StorageArea> storage_area1;
  session_storage_area1->Bind(storage_area1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::StorageArea> storage_area2;
  session_storage_area2->Bind(storage_area2.BindNewPipeAndPassReceiver());

  // Same maps are used.
  EXPECT_EQ(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  // Create the observer, attach to the first namespace, and verify we don't see
  // any changes (see SessionStorageAreaImpl class comment about when observers
  // are called).
  testing::StrictMock<test::MockStorageAreaObserver> mock_observer;
  storage_area1->AddObserver(mock_observer.Bind());

  // The |DeleteAll| call will fork the maps, and the observer should see a
  // DeleteAll.
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);
  // There should be no commits, as we don't actually have to change any data.
  // |session_storage_area1| should just switch to a new, empty map.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(0);
  test::DeleteAllSync(storage_area1.get(), "source");

  // The maps were forked on the above call.
  EXPECT_NE(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);

  session_storage_area1 = nullptr;
  session_storage_area2 = nullptr;
}

TEST_P(SessionStorageAreaImplTest, DeleteAllWithoutBinding) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area1 = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  test::DeleteAllSync(session_storage_area1.get(), "source");
  session_storage_area1->data_map()->storage_area()->ScheduleImmediateCommit();
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);

  session_storage_area1 = nullptr;
}

TEST_P(SessionStorageAreaImplTest, DeleteAllWithoutBindingOnShared) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/0, testing::_)).Times(1);

  auto session_storage_area1 = std::make_unique<SessionStorageAreaImpl>(
      test_namespace_id1_, test_storage_key1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_storage_key1_],
          database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  SessionStorageMetadata::NamespaceEntry clone_entry =
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_);
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), clone_entry);
  database_->PutMetadata(
      SessionStorageMetadata::ToDomStorageMetadata(clone_entry),
      base::DoNothing());
  auto session_storage_area2 =
      session_storage_area1->Clone(test_namespace_id2_);

  // Same maps are used.
  EXPECT_EQ(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  // The |DeleteAll| call will fork the maps, and the observer should see a
  // DeleteAll.
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);
  // There should be no commits, as we don't actually have to change any data.
  // |session_storage_area1| should just switch to a new, empty map.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(0);
  test::DeleteAllSync(session_storage_area1.get(), "source");

  // The maps were forked on the above call.
  EXPECT_NE(session_storage_area1->data_map(),
            session_storage_area2->data_map());

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/0)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);

  session_storage_area1 = nullptr;
  session_storage_area2 = nullptr;
}

}  // namespace storage
