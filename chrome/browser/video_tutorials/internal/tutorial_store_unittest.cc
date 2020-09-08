// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

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
  using TestEntries = std::vector<TutorialGroup>;
  using TestKeys = std::vector<std::string>;

  TutorialStoreTest() : load_result_(false), db_(nullptr) {}
  ~TutorialStoreTest() override = default;

  TutorialStoreTest(const TutorialStoreTest& other) = delete;
  TutorialStoreTest& operator=(const TutorialStoreTest& other) = delete;

 protected:
  void Init(TestEntries input, InitStatus status) {
    CreateTestDbEntries(std::move(input));
    auto db = std::make_unique<FakeDB<TutorialGroupProto, TutorialGroup>>(
        &db_entries_);
    db_ = db.get();
    store_ = std::make_unique<TutorialStore>(std::move(db));
    store_->InitAndLoadKeys(base::BindOnce(&TutorialStoreTest::OnKeysLoaded,
                                           base::Unretained(this)));
    db_->InitStatusCallback(status);
  }

  void OnKeysLoaded(bool success,
                    std::unique_ptr<std::vector<std::string>> loaded_keys) {
    load_result_ = success;
    loaded_keys_.clear();
    if (success && loaded_keys)
      loaded_keys_ = *loaded_keys;
  }

  void CreateTestDbEntries(TestEntries input) {
    for (auto& entry : input) {
      TutorialGroupProto proto;
      TutorialGroupToProto(&entry, &proto);
      db_entries_.emplace(entry.locale, proto);
    }
  }

  void VerifyLoadEntries(const TestKeys& keys,
                         bool expected_success,
                         TestEntries expected_entries) {
    store_->LoadEntries(keys,
                        base::BindOnce(&TutorialStoreTest::OnEntriesLoaded,
                                       base::Unretained(this), expected_success,
                                       expected_entries));
    db_->LoadCallback(true);
  }

  void OnEntriesLoaded(
      bool expected_success,
      TestEntries expected_entries,
      bool success,
      std::vector<std::unique_ptr<TutorialGroup>> loaded_entries) {
    EXPECT_EQ(expected_success, success);
    EXPECT_EQ(loaded_entries.size(), expected_entries.size());
    TestEntries actual_loaded_entries;
    for (auto& loaded_entry : loaded_entries) {
      actual_loaded_entries.emplace_back(*loaded_entry.get());
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

  bool load_result() const { return load_result_; }
  const EntriesMap& loaded_keys_and_entries() const {
    return loaded_keys_and_entries_;
  }
  const std::vector<std::string>& loaded_keys() const { return loaded_keys_; }
  FakeDB<TutorialGroupProto, TutorialGroup>* db() { return db_; }
  Store<TutorialGroup>* store() { return store_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  bool load_result_{false};
  TestKeys loaded_keys_;
  EntriesMap loaded_keys_and_entries_;
  ProtoMap db_entries_;
  FakeDB<TutorialGroupProto, TutorialGroup>* db_{nullptr};
  std::unique_ptr<Store<TutorialGroup>> store_;
};

// Test loading keys from a non-empty database in initialization successfully.
TEST_F(TutorialStoreTest, LoadedKeysSuccess) {
  auto test_data = TestEntries();
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::string locale = test_group.locale;
  test_data.emplace_back(std::move(test_group));
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadKeysCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(loaded_keys().size(), 1u);
  EXPECT_EQ(loaded_keys().front(), locale);
}

// Test loading keys from a non-empty database failed.
TEST_F(TutorialStoreTest, LoadKeysFailed) {
  auto test_data = TestEntries();
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::string locale = test_group.locale;
  test_data.emplace_back(std::move(test_group));
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadKeysCallback(false);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(loaded_keys().empty());
}

// Test loading entries with loaded keys successfully.
TEST_F(TutorialStoreTest, LoadedEntriesSuccess) {
  auto test_data = TestEntries();
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::string locale = test_group.locale;
  test_data.emplace_back(std::move(test_group));
  Init(test_data, InitStatus::kOK);
  db()->LoadKeysCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(loaded_keys().size(), 1u);
  EXPECT_EQ(loaded_keys().front(), locale);

  VerifyLoadEntries(loaded_keys() /*keys*/, true /*expected_success*/,
                    test_data /*expected_loaded_entries*/);
}

// Test adding and updating data successfully.
TEST_F(TutorialStoreTest, AddAndUpdateDataSuccess) {
  auto test_data = TestEntries();
  Init(std::move(test_data), InitStatus::kOK);
  db()->LoadKeysCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_keys().empty());

  // Add a group successfully.
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  store()->Update(test_group.locale, test_group,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  auto expected = std::make_unique<KeysAndEntries>();
  expected->emplace(test_group.locale, std::move(test_group));
  VerifyDataInDb(std::move(expected));
}

// Test deleting entries with keys .
TEST_F(TutorialStoreTest, Delete) {
  auto test_data = TestEntries();
  TutorialGroup test_group;
  test::BuildTestGroup(&test_group);
  std::string locale = test_group.locale;
  test_data.emplace_back(std::move(test_group));
  Init(test_data, InitStatus::kOK);
  db()->LoadKeysCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(loaded_keys().size(), 1u);
  EXPECT_EQ(loaded_keys().front(), locale);
  std::vector<std::string> keys{locale};
  store()->Delete(std::move(keys),
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  // No entry is expected in db.
  auto expected = std::make_unique<KeysAndEntries>();
  VerifyDataInDb(std::move(expected));
}

}  // namespace video_tutorials
