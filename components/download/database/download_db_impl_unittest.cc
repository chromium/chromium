// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db_impl.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/download/database/download_db_conversions.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/database/proto/download_entry.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {

namespace {

DownloadDBEntry CreateDownloadDBEntry() {
  DownloadDBEntry entry;
  DownloadInfo download_info;
  download_info.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.download_info = download_info;
  return entry;
}

std::string GetKey(const std::string& guid) {
  return DownloadNamespaceToString(
             DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD) +
         "," + guid;
}

}  // namespace

class DownloadDBTest : public testing::Test {
 public:
  DownloadDBTest() : db_(nullptr), init_success_(false) {}

  DownloadDBTest(const DownloadDBTest&) = delete;
  DownloadDBTest& operator=(const DownloadDBTest&) = delete;

  ~DownloadDBTest() override = default;

  void CreateDatabase() {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<download_pb::DownloadDBEntry>>(
        &db_entries_);
    db_ = db.get();
    download_db_ = std::make_unique<DownloadDBImpl>(
        DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD, std::move(db));
  }

  void InitCallback(bool success) { init_success_ = success; }

  void LoadCallback(std::vector<DownloadDBEntry>* loaded_entries,
                    bool success,
                    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
    loaded_entries->swap(*entries);
  }

  bool IsInitialized() { return download_db_->IsInitialized(); }

  void PrepopulateSampleEntries() {
    DownloadDBEntry first = CreateDownloadDBEntry();
    DownloadDBEntry second = CreateDownloadDBEntry();
    DownloadDBEntry third = CreateDownloadDBEntry();
    db_entries_.insert(
        std::make_pair("unknown," + first.GetGuid(),
                       DownloadDBConversions::DownloadDBEntryToProto(first)));
    db_entries_.insert(
        std::make_pair(GetKey(second.GetGuid()),
                       DownloadDBConversions::DownloadDBEntryToProto(second)));
    db_entries_.insert(
        std::make_pair(GetKey(third.GetGuid()),
                       DownloadDBConversions::DownloadDBEntryToProto(third)));
  }

  void DestroyAndReinitialize() {
    download_db_->DestroyAndReinitialize(
        base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
    ASSERT_FALSE(download_db_->IsInitialized());
  }

 protected:
  std::map<std::string, download_pb::DownloadDBEntry> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<download_pb::DownloadDBEntry>,
          DanglingUntriaged>
      db_;
  std::unique_ptr<DownloadDBImpl> download_db_;
  bool init_success_;
};

TEST_F(DownloadDBTest, InitializeSucceeded) {
  CreateDatabase();
  ASSERT_FALSE(IsInitialized());

  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  ASSERT_TRUE(IsInitialized());
  ASSERT_TRUE(init_success_);
}

TEST_F(DownloadDBTest, InitializeFailed) {
  CreateDatabase();
  ASSERT_FALSE(IsInitialized());

  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);

  ASSERT_FALSE(IsInitialized());
  ASSERT_FALSE(init_success_);
}

TEST_F(DownloadDBTest, LoadEntries) {
  PrepopulateSampleEntries();
  CreateDatabase();
  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(IsInitialized());

  std::vector<DownloadDBEntry> loaded_entries;
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(2u, loaded_entries.size());
  for (auto db_entry : loaded_entries) {
    ASSERT_EQ(db_entry,
              DownloadDBConversions::DownloadDBEntryFromProto(
                  db_entries_.find(GetKey(db_entry.GetGuid()))->second));
  }
}

TEST_F(DownloadDBTest, AddEntry) {
  PrepopulateSampleEntries();
  CreateDatabase();
  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(IsInitialized());

  DownloadDBEntry entry = CreateDownloadDBEntry();
  download_db_->AddOrReplace(entry);
  db_->UpdateCallback(true);

  std::vector<DownloadDBEntry> loaded_entries;
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(3u, loaded_entries.size());
  for (auto db_entry : loaded_entries) {
    ASSERT_EQ(db_entry,
              DownloadDBConversions::DownloadDBEntryFromProto(
                  db_entries_.find(GetKey(db_entry.GetGuid()))->second));
  }
}

TEST_F(DownloadDBTest, ReplaceEntry) {
  DownloadDBEntry first = CreateDownloadDBEntry();
  DownloadDBEntry second = CreateDownloadDBEntry();
  db_entries_.insert(
      std::make_pair(GetKey(first.GetGuid()),
                     DownloadDBConversions::DownloadDBEntryToProto(first)));
  db_entries_.insert(
      std::make_pair(GetKey(second.GetGuid()),
                     DownloadDBConversions::DownloadDBEntryToProto(second)));
  CreateDatabase();
  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(IsInitialized());

  InProgressInfo in_progress_info;
  in_progress_info.current_path =
      base::FilePath(FILE_PATH_LITERAL("/tmp.crdownload"));
  in_progress_info.target_path = base::FilePath(FILE_PATH_LITERAL("/tmp"));
  in_progress_info.url_chain.emplace_back("http://foo");
  in_progress_info.url_chain.emplace_back("http://foo2");
  first.download_info->in_progress_info = in_progress_info;
  download_db_->AddOrReplace(first);
  db_->UpdateCallback(true);

  std::vector<DownloadDBEntry> loaded_entries;
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(2u, loaded_entries.size());
  for (auto db_entry : loaded_entries) {
    ASSERT_EQ(db_entry,
              DownloadDBConversions::DownloadDBEntryFromProto(
                  db_entries_.find(GetKey(db_entry.GetGuid()))->second));
  }
}

TEST_F(DownloadDBTest, Remove) {
  DownloadDBEntry first = CreateDownloadDBEntry();
  DownloadDBEntry second = CreateDownloadDBEntry();
  db_entries_.insert(
      std::make_pair(GetKey(first.GetGuid()),
                     DownloadDBConversions::DownloadDBEntryToProto(first)));
  db_entries_.insert(
      std::make_pair(GetKey(second.GetGuid()),
                     DownloadDBConversions::DownloadDBEntryToProto(second)));
  CreateDatabase();
  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(IsInitialized());

  download_db_->Remove(first.GetGuid());
  db_->UpdateCallback(true);

  std::vector<DownloadDBEntry> loaded_entries;
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(1u, loaded_entries.size());
  ASSERT_EQ(loaded_entries[0],
            DownloadDBConversions::DownloadDBEntryFromProto(
                db_entries_.find(GetKey(loaded_entries[0].GetGuid()))->second));
}

TEST_F(DownloadDBTest, DestroyAndReinitialize) {
  PrepopulateSampleEntries();
  CreateDatabase();
  download_db_->Initialize(
      base::BindOnce(&DownloadDBTest::InitCallback, base::Unretained(this)));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(IsInitialized());

  std::vector<DownloadDBEntry> loaded_entries;
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(2u, loaded_entries.size());

  DestroyAndReinitialize();

  db_->DestroyCallback(true);
  download_db_->LoadEntries(base::BindOnce(
      &DownloadDBTest::LoadCallback, base::Unretained(this), &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(0u, loaded_entries.size());
}

}  // namespace download
