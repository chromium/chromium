// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;

namespace video_tutorials {
namespace {
constexpr char kDatabaseEntryKey[] = "";
}  // namespace

class TutorialStoreTest : public testing::Test {
 public:
  using KeysAndEntries = std::map<std::string, proto::VideoTutorialGroups>;

  TutorialStoreTest() {
    auto db =
        std::make_unique<FakeDB<proto::VideoTutorialGroups>>(&db_entries_);
    db_ = db.get();
    store_ = std::make_unique<TutorialStore>(std::move(db));
  }
  ~TutorialStoreTest() override = default;

  TutorialStoreTest(const TutorialStoreTest& other) = delete;
  TutorialStoreTest& operator=(const TutorialStoreTest& other) = delete;

 protected:
  void LoadEntriesAndVerify(bool load_only, bool expected_success) {
    store_->InitAndLoad(base::BindOnce(&TutorialStoreTest::OnEntriesLoaded,
                                       base::Unretained(this),
                                       expected_success));
    if (!load_only)
      db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    db_->GetCallback(true);
  }

  void OnEntriesLoaded(
      bool expected_success,
      bool success,
      std::unique_ptr<proto::VideoTutorialGroups> loaded_data) {
    EXPECT_EQ(expected_success, success);
    EXPECT_TRUE(loaded_data);
    last_load_result_ = std::move(loaded_data);
  }

  FakeDB<proto::VideoTutorialGroups>* db() { return db_; }
  TutorialStore* store() { return store_.get(); }

  proto::VideoTutorialGroups* last_load_result() {
    return last_load_result_.get();
  }

  base::test::TaskEnvironment task_environment_;
  std::map<std::string, proto::VideoTutorialGroups> db_entries_;
  std::unique_ptr<proto::VideoTutorialGroups> last_load_result_;
  raw_ptr<FakeDB<proto::VideoTutorialGroups>> db_{nullptr};
  std::unique_ptr<TutorialStore> store_;
};

TEST_F(TutorialStoreTest, InitAndLoad) {
  // Initialize database and call InitAndLoad().
  proto::VideoTutorialGroups test_data;
  test_data.set_preferred_locale("hi");
  test_data.add_tutorial_groups()->add_tutorials()->set_feature(
      proto::FeatureType::DOWNLOAD);
  test_data.add_tutorial_groups()->add_tutorials()->set_feature(
      proto::FeatureType::SEARCH);

  db_entries_[kDatabaseEntryKey] = test_data;
  LoadEntriesAndVerify(false, true);
  EXPECT_TRUE(last_load_result());
  EXPECT_EQ(2, last_load_result()->tutorial_groups_size());
  EXPECT_EQ(proto::FeatureType::DOWNLOAD,
            last_load_result()->tutorial_groups(0).tutorials(0).feature());

  // Modify the database, and call Load().
  test_data.add_tutorial_groups()->add_tutorials()->set_feature(
      proto::FeatureType::VOICE_SEARCH);
  db_entries_[kDatabaseEntryKey] = test_data;
  LoadEntriesAndVerify(true, true);
  EXPECT_TRUE(last_load_result());
  EXPECT_EQ(3, last_load_result()->tutorial_groups_size());
  EXPECT_EQ(proto::FeatureType::VOICE_SEARCH,
            last_load_result()->tutorial_groups(2).tutorials(0).feature());
}

TEST_F(TutorialStoreTest, Update) {
  // Initialize database with no data.
  proto::VideoTutorialGroups test_data;
  db_entries_[kDatabaseEntryKey] = test_data;
  LoadEntriesAndVerify(false, true);
  EXPECT_TRUE(last_load_result());
  EXPECT_EQ(0, last_load_result()->tutorial_groups_size());

  // Add a tutorial group and update.
  test_data.add_tutorial_groups()->add_tutorials()->set_feature(
      proto::FeatureType::SEARCH);
  store()->Update(test_data,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  LoadEntriesAndVerify(true, true);
  EXPECT_TRUE(last_load_result());
  EXPECT_EQ(1, last_load_result()->tutorial_groups_size());
  EXPECT_EQ(proto::FeatureType::SEARCH,
            last_load_result()->tutorial_groups(0).tutorials(0).feature());

  // Clear data and update.
  store()->Update(proto::VideoTutorialGroups(),
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  LoadEntriesAndVerify(true, true);
  EXPECT_TRUE(last_load_result());
  EXPECT_EQ(0, last_load_result()->tutorial_groups_size());
}

}  // namespace video_tutorials
