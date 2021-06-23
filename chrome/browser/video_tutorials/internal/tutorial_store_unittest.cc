// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/video_tutorials/test/test_utils.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;

namespace video_tutorials {

class TutorialStoreTest : public testing::Test {
 public:
  using TutorialGroupProto = video_tutorials::proto::VideoTutorialGroup;
  using EntriesMap = std::map<std::string, std::unique_ptr<TutorialGroup>>;
  using ProtoMap = std::map<std::string, TutorialGroupProto>;
  using KeysAndEntries = std::map<std::string, TutorialGroup>;

  TutorialStoreTest() : db_(nullptr) {}
  ~TutorialStoreTest() override = default;

  TutorialStoreTest(const TutorialStoreTest& other) = delete;
  TutorialStoreTest& operator=(const TutorialStoreTest& other) = delete;

 protected:
  void Init(std::vector<TutorialGroup> input,
            InitStatus status,
            bool expected_success) {
    CreateTestDbEntries(std::move(input));
    auto db = std::make_unique<FakeDB<TutorialGroupProto, TutorialGroup>>(
        &db_entries_);
    db_ = db.get();
    store_ = std::make_unique<TutorialStore>(std::move(db));
    store_->Initialize(base::BindOnce(&TutorialStoreTest::OnInitCompleted,
                                      base::Unretained(this),
                                      expected_success));
    db_->InitStatusCallback(status);
  }

  void OnInitCompleted(bool expected_success, bool success) {
    EXPECT_EQ(expected_success, success);
  }

  void CreateTestDbEntries(std::vector<TutorialGroup> input) {
    for (auto& entry : input) {
      TutorialGroupProto proto;
      TutorialGroupToProto(&entry, &proto);
      db_entries_.emplace(entry.language, proto);
    }
  }

  void LoadEntriesAndVerify(const std::vector<std::string>& keys,
                            bool expected_success,
                            std::vector<TutorialGroup> expected_entries) {
    store_->LoadEntries(keys,
                        base::BindOnce(&TutorialStoreTest::OnEntriesLoaded,
                                       base::Unretained(this), expected_success,
                                       expected_entries));
    db_->LoadCallback(true);
  }

  void OnEntriesLoaded(
      bool expected_success,
      std::vector<TutorialGroup> expected_entries,
      bool success,
      std::unique_ptr<std::vector<TutorialGroup>> loaded_entries) {
    EXPECT_EQ(expected_success, success);
    EXPECT_EQ(loaded_entries->size(), expected_entries.size());
    std::vector<TutorialGroup> actual_loaded_entries;
    for (auto& loaded_entry : *loaded_entries.get()) {
      actual_loaded_entries.emplace_back(loaded_entry);
    }
    EXPECT_EQ(expected_entries, actual_loaded_entries);
  }

  // Verifies the entries in the db is |expected|.
  void VerifyDataInDb(std::unique_ptr<KeysAndEntries> expected) {
    db_->LoadKeysAndEntries(base::BindOnce(&TutorialStoreTest::OnVerifyDataInDb,
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
      EXPECT_EQ(actual_loaded_group, expected_group);
    }
  }

  const EntriesMap& loaded_keys_and_entries() const {
    return loaded_keys_and_entries_;
  }
  FakeDB<TutorialGroupProto, TutorialGroup>* db() { return db_; }
  Store<TutorialGroup>* store() { return store_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  EntriesMap loaded_keys_and_entries_;
  ProtoMap db_entries_;
  FakeDB<TutorialGroupProto, TutorialGroup>* db_{nullptr};
  std::unique_ptr<Store<TutorialGroup>> store_;
};

// Test loading keys from a non-empty database in initialization successfully.
TEST_F(TutorialStoreTest, InitSuccess) {
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::vector<TutorialGroup> test_data;
  test_data.emplace_back(std::move(test_group));

  Init(std::move(test_data), InitStatus::kOK, true /* expected */);
}

// Test loading all entries from a non-empty database in initialization
// successfully.
TEST_F(TutorialStoreTest, LoadAllEntries) {
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::vector<TutorialGroup> test_data;
  test_data.emplace_back(std::move(test_group));
  auto expected_test_data = test_data;

  Init(std::move(test_data), InitStatus::kOK, true /* expected */);
  LoadEntriesAndVerify(std::vector<std::string>(), true, expected_test_data);
}

// Test loading keys from a non-empty database in initialization successfully.
TEST_F(TutorialStoreTest, LoadSpecificEntries) {
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::vector<TutorialGroup> test_data;
  test_data.emplace_back(std::move(test_group));
  auto expected_test_data = test_data;

  Init(std::move(test_data), InitStatus::kOK, true /* expected */);
  std::vector<std::string> keys;
  keys.emplace_back("en");
  LoadEntriesAndVerify(keys, true, expected_test_data);
}

TEST_F(TutorialStoreTest, LoadEntryThatDoesntExist) {
  std::vector<TutorialGroup> test_data;
  auto expected_test_data = test_data;

  Init(std::move(test_data), InitStatus::kOK, true /* expected */);
  std::vector<std::string> keys;
  keys.emplace_back("en");
  LoadEntriesAndVerify(keys, true, expected_test_data);
}

// Test adding and updating data successfully.
TEST_F(TutorialStoreTest, AddAndUpdateDataSuccess) {
  std::vector<TutorialGroup> test_data;
  Init(std::move(test_data), InitStatus::kOK, true /* expected */);

  // Add a group successfully.
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::vector<std::pair<std::string, TutorialGroup>> entries_to_save;
  entries_to_save.emplace_back(std::make_pair(test_group.language, test_group));
  std::vector<std::string> keys_to_delete;
  store()->UpdateAll(
      entries_to_save, keys_to_delete,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  auto expected = std::make_unique<KeysAndEntries>();
  expected->emplace(test_group.language, std::move(test_group));
  VerifyDataInDb(std::move(expected));
}

// Test deleting entries with keys .
TEST_F(TutorialStoreTest, Delete) {
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::string locale = test_group.language;
  std::vector<TutorialGroup> test_data;
  test_data.emplace_back(std::move(test_group));
  Init(test_data, InitStatus::kOK, true /* expected */);

  std::vector<std::string> keys{locale};
  std::vector<std::pair<std::string, TutorialGroup>> entries_to_save;

  store()->UpdateAll(
      entries_to_save, std::move(keys),
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  // No entry is expected in db.
  auto expected = std::make_unique<KeysAndEntries>();
  VerifyDataInDb(std::move(expected));
}

}  // namespace video_tutorials
