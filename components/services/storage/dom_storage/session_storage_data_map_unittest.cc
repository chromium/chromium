// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_data_map.h"

#include <map>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
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

base::span<const uint8_t> MakeBytes(std::string_view str) {
  return base::as_byte_span(str);
}

mojo::PendingRemote<blink::mojom::StorageAreaObserver> MakeStubObserver() {
  mojo::PendingRemote<blink::mojom::StorageAreaObserver> observer;
  std::ignore = observer.InitWithNewPipeAndPassReceiver();
  return observer;
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(DbStatus status));
};

void GetAllDataCallback(base::OnceClosure callback,
                        std::vector<blink::mojom::KeyValuePtr>* data_out,
                        std::vector<blink::mojom::KeyValuePtr> data) {
  *data_out = std::move(data);
  std::move(callback).Run();
}

blink::mojom::StorageArea::GetAllCallback MakeGetAllCallback(
    base::OnceClosure callback,
    std::vector<blink::mojom::KeyValuePtr>* data_out) {
  return base::BindOnce(&GetAllDataCallback, std::move(callback), data_out);
}

class SessionStorageDataMapTest : public testing::Test {
 public:
  SessionStorageDataMapTest() {
    // Create an in-memory LevelDB.
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage,
        /*directory=*/base::FilePath(), "SessionStorageDataMapTest",
        /*memory_dump_id=*/std::nullopt,
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        base::BindLambdaForTesting([&](DbStatus status) {
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    database_->database().PostTaskWithThisObject(
        base::BindOnce([](DomStorageDatabase* dom_storage_database) {
          DomStorageDatabaseLevelDB* db = &dom_storage_database->GetLevelDB();
          // Should show up in first map.
          DbStatus status =
              db->Put(MakeBytes("map-1-key1"), MakeBytes("data1"));
          ASSERT_TRUE(status.ok());

          // Dummy data to verify we don't delete everything.
          status = db->Put(MakeBytes("map-3-key1"), MakeBytes("data3"));
          ASSERT_TRUE(status.ok());
        }));
  }

  ~SessionStorageDataMapTest() override = default;

  std::map<std::string, std::string> GetDatabaseContents() {
    std::vector<DomStorageDatabase::KeyValuePair> entries;
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(base::BindLambdaForTesting(
        [&](DomStorageDatabase* dom_storage_database) {
          DomStorageDatabaseLevelDB& db = dom_storage_database->GetLevelDB();
          ASSERT_OK_AND_ASSIGN(entries, db.GetPrefixed({}));
          loop.Quit();
        }));
    loop.Run();

    std::map<std::string, std::string> contents;
    for (auto& entry : entries) {
      contents.emplace(std::string(entry.key.begin(), entry.key.end()),
                       std::string(entry.value.begin(), entry.value.end()));
    }

    return contents;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<MockListener> listener_;
  const blink::StorageKey test_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://host1.com:1");
  std::unique_ptr<AsyncDomStorageDatabase> database_;
};

}  // namespace

TEST_F(SessionStorageDataMapTest, BasicEmptyCreation) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map =
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(
              1, test_storage_key_),
          database_.get());

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map->storage_area()->GetAll(MakeStubObserver(),
                              MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;

  // The database must contain 3 entries:
  // (1) This map's key/value pair.
  // (2) The other map's key/value pair
  // (3) The database schema version.
  EXPECT_EQ(3u, GetDatabaseContents().size());
}

TEST_F(SessionStorageDataMapTest, ExplicitlyEmpty) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map = SessionStorageDataMap::CreateEmpty(
      &listener_,
      base::MakeRefCounted<SessionStorageMetadata::MapData>(1,
                                                            test_storage_key_),
      database_.get());

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map->storage_area()->GetAll(MakeStubObserver(),
                              MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(0u, data.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;

  // The database must contain 3 entries:
  // (1) This map's key/value pair.
  // (2) The other map's key/value pair
  // (3) The database schema version.
  EXPECT_EQ(3u, GetDatabaseContents().size());
}

TEST_F(SessionStorageDataMapTest, Clone) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map1 =
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(
              1, test_storage_key_),
          database_.get());

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("2"), testing::_))
      .Times(1);
  // One call on fork.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(1);

  scoped_refptr<SessionStorageDataMap> map2 =
      SessionStorageDataMap::CreateClone(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(
              2, test_storage_key_),
          map1);

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map2->storage_area()->GetAll(MakeStubObserver(),
                               MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  // Test that the data was copied.
  EXPECT_EQ("data1", GetDatabaseContents()["map-2-key1"]);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("2")))
      .Times(1);

  // Test data is not cleared on deletion.
  map1 = nullptr;
  map2 = nullptr;

  // The database must contain 4 entries:
  // (1) This map's key/value pair.
  // (2) The cloned map's key/value pair.
  // (3) The other map's key/value pair
  // (4) The database schema version.
  EXPECT_EQ(4u, GetDatabaseContents().size());
}

}  // namespace storage
