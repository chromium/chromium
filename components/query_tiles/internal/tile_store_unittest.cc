// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/query_tiles/internal/proto_conversion.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;

namespace query_tiles {
namespace {

class TileStoreTest : public testing::Test {
 public:
  using TileGroupProto = query_tiles::proto::TileGroup;
  using EntriesMap = std::map<std::string, std::unique_ptr<TileGroup>>;
  using ProtoMap = std::map<std::string, TileGroupProto>;
  using KeysAndEntries = std::map<std::string, TileGroup>;
  using TestEntries = std::vector<TileGroup>;

  TileStoreTest() : load_result_(false), db_(nullptr) {}
  ~TileStoreTest() override = default;

  TileStoreTest(const TileStoreTest& other) = delete;
  TileStoreTest& operator=(const TileStoreTest& other) = delete;

 protected:
  void Init(TestEntries input, InitStatus status) {
    CreateTestDbEntries(std::move(input));
    auto db = std::make_unique<FakeDB<TileGroupProto, TileGroup>>(&db_entries_);
    db_ = db.get();
    store_ = std::make_unique<TileStore>(std::move(db));
    store_->InitAndLoad(base::BindOnce(&TileStoreTest::OnEntriesLoaded,
                                       base::Unretained(this)));
    db_->InitStatusCallback(status);
  }

  void OnEntriesLoaded(bool success, EntriesMap loaded_entries) {
    load_result_ = success;
    in_memory_entries_ = std::move(loaded_entries);
  }

  void CreateTestDbEntries(TestEntries input) {
    for (auto& entry : input) {
      TileGroupProto proto;
      query_tiles::TileGroupToProto(&entry, &proto);
      db_entries_.emplace(entry.id, proto);
    }
  }

  // Verifies the entries in the db is |expected|.
  void VerifyDataInDb(std::unique_ptr<KeysAndEntries> expected) {
    db_->LoadKeysAndEntries(base::BindOnce(&TileStoreTest::OnVerifyDataInDb,
                                           base::Unretained(this),
                                           std::move(expected)));
    db_->LoadCallback(true);
  }

  void OnVerifyDataInDb(std::unique_ptr<KeysAndEntries> expected,
                        bool success,
                        std::unique_ptr<KeysAndEntries> loaded_entries) {
    EXPECT_TRUE(success);
    DCHECK(expected);
    DCHECK(loaded_entries);
    for (auto it = loaded_entries->begin(); it != loaded_entries->end(); it++) {
      EXPECT_NE(expected->count(it->first), 0u);
      auto& actual_loaded_group = it->second;
      auto& expected_group = expected->at(it->first);
      EXPECT_TRUE(
          test::AreTileGroupsIdentical(actual_loaded_group, expected_group))
          << "\n Actual: " << actual_loaded_group.DebugString()
          << "\n Expected: " << expected_group.DebugString();
    }
  }

  bool load_result() const { return load_result_; }
  const EntriesMap& in_memory_entries() const { return in_memory_entries_; }
  FakeDB<TileGroupProto, TileGroup>* db() { return db_; }
  Store<TileGroup>* store() { return store_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  bool load_result_;
  EntriesMap in_memory_entries_;
  ProtoMap db_entries_;
  raw_ptr<FakeDB<TileGroupProto, TileGroup>> db_;
  std::unique_ptr<Store<TileGroup>> store_;
};

// Test Initializing and loading an empty database .
TEST_F(TileStoreTest, InitSuccessEmptyDb) {
  auto test_data = TestEntries();
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(in_memory_entries().empty());
}

// Test Initializing and loading a non-empty database.
TEST_F(TileStoreTest, InitSuccessWithData) {
  auto test_data = TestEntries();
  TileGroup test_group;
  test::ResetTestGroup(&test_group);
  std::string id = test_group.id;
  test_data.emplace_back(std::move(test_group));
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(in_memory_entries().size(), 1u);
  auto actual = in_memory_entries().begin();
  EXPECT_EQ(actual->first, id);
  EXPECT_EQ(actual->second.get()->id, id);
}

// Test Initializing and loading a non-empty database failed.
TEST_F(TileStoreTest, InitFailedWithData) {
  auto test_data = TestEntries();
  TileGroup test_group;
  test::ResetTestGroup(&test_group);
  std::string id = test_group.id;
  test_data.emplace_back(std::move(test_group));
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(false);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(in_memory_entries().empty());
}

// Test adding and updating.
TEST_F(TileStoreTest, AddAndUpdateDataFailed) {
  auto test_data = TestEntries();
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(in_memory_entries().empty());

  // Add an entry failed.
  TileGroup test_group;
  test_group.id = "test_group_id";
  auto test_entry_1 = std::make_unique<Tile>();
  test_entry_1->id = "test_entry_id_1";
  test_entry_1->display_text = "test_entry_test_display_text";
  test_group.tiles.emplace_back(std::move(test_entry_1));
  store()->Update(test_group.id, test_group,
                  base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->UpdateCallback(false);
}

TEST_F(TileStoreTest, AddAndUpdateDataSuccess) {
  auto test_data = TestEntries();
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(in_memory_entries().empty());

  // Add a group successfully.
  TileGroup test_group;
  test::ResetTestGroup(&test_group);
  store()->Update(test_group.id, test_group,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  auto expected = std::make_unique<KeysAndEntries>();
  expected->emplace(test_group.id, std::move(test_group));
  VerifyDataInDb(std::move(expected));
}

// Test deleting from db.
TEST_F(TileStoreTest, DeleteSuccess) {
  auto test_data = TestEntries();
  TileGroup test_group;
  test::ResetTestGroup(&test_group);
  std::string id = test_group.id;
  test_data.emplace_back(std::move(test_group));
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(in_memory_entries().size(), 1u);
  auto actual = in_memory_entries().begin();
  EXPECT_EQ(actual->first, id);
  EXPECT_EQ(actual->second.get()->id, id);
  store()->Delete(id,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  // No entry is expected in db.
  auto expected = std::make_unique<KeysAndEntries>();
  VerifyDataInDb(std::move(expected));
}

}  // namespace
}  // namespace query_tiles
