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
#include "base/test/with_feature_override.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

constexpr const char kFakeNamespaceId[] =
    "ce8c7dc5_73b4_4320_a506_ce1f4fd3356f";
constexpr const char kClonedFakeNamespaceId[] =
    "5fe0e896_c6d8_4d2b_8b3c_d26f47832125";
constexpr const char kOtherFakeNamespaceId[] =
    "36356e0b_1627_4492_a474_db76a8996bed";

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

MATCHER(OKStatus, "Equality matcher for type OK DbStatus") {
  return arg.ok();
}

std::vector<uint8_t> MakeBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
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
               void(int64_t map_id, SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(int64_t map_id));
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

class SessionStorageDataMapTest : public base::test::WithFeatureOverride,
                                  public testing::Test {
 public:
  SessionStorageDataMapTest()
      : base::test::WithFeatureOverride(kDomStorageSqlite) {
    // Create an in-memory database.
    base::RunLoop loop;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage,
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt,
        base::BindLambdaForTesting([&](DbStatus status) {
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    // Store a key/value pair in the first map.
    FakeCommitter first_map_committer(database_.get(),
                                      first_map_locator_->Clone());
    first_map_committer.PutMapKeyValueSync(kKey1, kValue1);

    // Create another map with a key/value pair to verify the test does not
    // delete everything.
    FakeCommitter other_map_committer(database_.get(),
                                      other_map_locator_->Clone());
    other_map_committer.PutMapKeyValueSync(kKey1, kValue3);
  }

  ~SessionStorageDataMapTest() override = default;

  // Verifies a map in the database contains `expected_entries`.
  void ExpectMapEquals(const DomStorageDatabase::MapLocator& map_locator,
                       std::map<DomStorageDatabase::Key,
                                DomStorageDatabase::Value> expected_entries) {
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
    ASSERT_NO_FATAL_FAILURE(
        ReadMapKeyValuesSync(*database_, map_locator.Clone(), &actual_entries));
    EXPECT_EQ(actual_entries, expected_entries);
  }

 protected:
  const DomStorageDatabase::Key kKey1 = MakeBytes("key1");
  const DomStorageDatabase::Value kValue1 = MakeBytes("data1");
  const DomStorageDatabase::Value kValue3 = MakeBytes("data3");

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://host1.com:1");

  scoped_refptr<DomStorageDatabase::SharedMapLocator> first_map_locator_ =
      base::MakeRefCounted<DomStorageDatabase::SharedMapLocator>(
          DomStorageDatabase::MapLocator(kFakeNamespaceId,
                                         kTestStorageKey,
                                         /*map_id=*/1));

  scoped_refptr<DomStorageDatabase::SharedMapLocator> other_map_locator_ =
      base::MakeRefCounted<DomStorageDatabase::SharedMapLocator>(
          DomStorageDatabase::MapLocator(kOtherFakeNamespaceId,
                                         kTestStorageKey,
                                         /*map_id=*/3));

  scoped_refptr<DomStorageDatabase::SharedMapLocator> cloned_map_locator_ =
      base::MakeRefCounted<DomStorageDatabase::SharedMapLocator>(
          DomStorageDatabase::MapLocator(kClonedFakeNamespaceId,
                                         kTestStorageKey,
                                         /*map_id=*/2));

  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<MockListener> listener_;
  std::unique_ptr<AsyncDomStorageDatabase> database_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    SessionStorageDataMapTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<SessionStorageDataMapTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

}  // namespace

TEST_P(SessionStorageDataMapTest, BasicEmptyCreation) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);

  scoped_refptr<SessionStorageDataMap> map =
      SessionStorageDataMap::CreateFromDisk(&listener_, first_map_locator_,
                                            database_.get());

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map->storage_area()->GetAll(MakeStubObserver(),
                              MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;

  // The database must contain 2 map entries:
  // (1) This map's key/value pair.
  // (2) The other map's key/value pair
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*first_map_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*other_map_locator_, {{kKey1, kValue3}}));
}

TEST_P(SessionStorageDataMapTest, ExplicitlyEmpty) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);

  scoped_refptr<SessionStorageDataMap> map = SessionStorageDataMap::CreateEmpty(
      &listener_, first_map_locator_, database_.get());

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map->storage_area()->GetAll(MakeStubObserver(),
                              MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(0u, data.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;

  // The database must contain 2 map entries:
  // (1) This map's key/value pair.
  // (2) The other map's key/value pair
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*first_map_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*other_map_locator_, {{kKey1, kValue3}}));
}

TEST_P(SessionStorageDataMapTest, Clone) {
  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/1, testing::_)).Times(1);

  scoped_refptr<SessionStorageDataMap> map1 =
      SessionStorageDataMap::CreateFromDisk(&listener_, first_map_locator_,
                                            database_.get());

  EXPECT_CALL(listener_, OnDataMapCreation(/*map_id=*/2, testing::_)).Times(1);
  // One call on fork.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(1);

  scoped_refptr<SessionStorageDataMap> map2 =
      SessionStorageDataMap::CreateClone(&listener_, cloned_map_locator_, map1);

  std::vector<blink::mojom::KeyValuePtr> data;
  base::RunLoop loop;
  map2->storage_area()->GetAll(MakeStubObserver(),
                               MakeGetAllCallback(loop.QuitClosure(), &data));
  loop.Run();

  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  // Test that the data was copied.
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*cloned_map_locator_, {{kKey1, kValue1}}));
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/1)).Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(/*map_id=*/2)).Times(1);

  // Test data is not cleared on deletion.
  map1 = nullptr;
  map2 = nullptr;

  // The database must contain 3 map entries:
  // (1) This map's key/value pair.
  // (2) The cloned map's key/value pair.
  // (3) The other map's key/value pair
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*first_map_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*cloned_map_locator_, {{kKey1, kValue1}}));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMapEquals(*first_map_locator_, {{kKey1, kValue1}}));
}

}  // namespace storage
