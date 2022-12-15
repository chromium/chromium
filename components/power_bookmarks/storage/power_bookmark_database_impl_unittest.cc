// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/power_bookmarks/core/powers/search_params.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace power_bookmarks {

namespace {

std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    std::unique_ptr<sync_pb::PowerEntity> power_specifics) {
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_guid(base::GUID::GenerateRandomV4());
  power->set_url(url);
  power->set_power_type(power_type);
  return power;
}

std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type) {
  return MakePower(url, power_type, std::make_unique<sync_pb::PowerEntity>());
}

bool ContainsPower(const std::vector<std::unique_ptr<Power>>& list,
                   sync_pb::PowerBookmarkSpecifics::PowerType power_type,
                   GURL url) {
  for (const std::unique_ptr<Power>& power : list) {
    if (power->power_type() == power_type && power->url() == url)
      return true;
  }
  return false;
}

}  // namespace

class PowerBookmarkDatabaseImplTest : public testing::Test {
 public:
  PowerBookmarkDatabaseImplTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }
  void TearDown() override { EXPECT_TRUE(temp_directory_.Delete()); }

  base::FilePath db_dir() { return temp_directory_.GetPath(); }

  base::FilePath db_file_path() {
    return temp_directory_.GetPath().Append(kDatabaseName);
  }

  base::HistogramTester* histogram() { return &histogram_; }

  void InsertBadlyFormattedProtoToDB() {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    static constexpr char kCreatePowerSaveSql[] =
        // clang-format off
      "INSERT INTO saves("
          "id, url, origin, power_type, "
          "time_added, time_modified)"
      "VALUES(?,?,?,?,?,?)";
    // clang-format on
    DCHECK(db.IsSQLValid(kCreatePowerSaveSql));

    sql::Statement save_statement(
        db.GetCachedStatement(SQL_FROM_HERE, kCreatePowerSaveSql));

    std::unique_ptr<sync_pb::PowerEntity> power_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    std::unique_ptr<Power> power =
        std::make_unique<Power>(std::move(power_specifics));
    save_statement.BindString(0, power->guid().AsLowercaseString());
    save_statement.BindString(1, power->url().spec());
    save_statement.BindString(2, url::Origin::Create(power->url()).Serialize());
    save_statement.BindInt(3, power->power_type());
    save_statement.BindTime(4, power->time_added());
    save_statement.BindTime(5, power->time_modified());
    EXPECT_TRUE(save_statement.Run());

    static constexpr char kCreatePowerBlobSql[] =
        // clang-format off
      "INSERT INTO blobs("
          "id, specifics)"
      "VALUES(?,?)";
    // clang-format on
    DCHECK(db.IsSQLValid(kCreatePowerBlobSql));

    sql::Statement blob_statement(
        db.GetCachedStatement(SQL_FROM_HERE, kCreatePowerBlobSql));
    blob_statement.BindString(0, power->guid().AsLowercaseString());
    std::string data = "badprotofortesting";
    blob_statement.BindBlob(1, data);
    EXPECT_TRUE(blob_statement.Run());
  }

 private:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  base::HistogramTester histogram_;
};

TEST_F(PowerBookmarkDatabaseImplTest, InitDatabaseWithErrorCallback) {
  EXPECT_FALSE(base::PathExists(db_file_path()));

  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());

  EXPECT_FALSE(base::PathExists(db_file_path()));

  EXPECT_TRUE(pbdb->Init());
  EXPECT_TRUE(base::PathExists(db_file_path()));

  pbdb->DatabaseErrorCallback(
      static_cast<int>(sql::SqliteResultCode::kCantOpen), nullptr);
  EXPECT_FALSE(pbdb->IsOpen());

  histogram()->ExpectTotalCount("PowerBookmarks.Storage.DatabaseError", 1);
  histogram()->ExpectBucketCount("PowerBookmarks.Storage.DatabaseError",
                                 sql::SqliteResultCode::kCantOpen, 1);
}

TEST_F(PowerBookmarkDatabaseImplTest, InitDatabase) {
  EXPECT_FALSE(base::PathExists(db_file_path()));
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());

    EXPECT_FALSE(base::PathExists(db_file_path()));

    EXPECT_TRUE(pbdb->Init());
    EXPECT_TRUE(base::PathExists(db_file_path()));

    histogram()->ExpectTotalCount(
        "PowerBookmarks.Storage.DatabaseDirSizeAtStartup", 1);
  }

  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    // Database should have 4 tables: meta, saves, blobs and sync_meta.
    EXPECT_EQ(4u, sql::test::CountSQLTables(&db));
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, InitDatabaseTwice) {
  EXPECT_FALSE(base::PathExists(db_file_path()));

  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());

  EXPECT_FALSE(base::PathExists(db_file_path()));

  EXPECT_TRUE(pbdb->Init());
  EXPECT_TRUE(base::PathExists(db_file_path()));

  // The 2nd Init should return true since the db is already open.
  EXPECT_TRUE(pbdb->Init());

  histogram()->ExpectTotalCount(
      "PowerBookmarks.Storage.DatabaseDirSizeAtStartup", 1);
}

TEST_F(PowerBookmarkDatabaseImplTest, DatabaseNewVersion) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Create an empty database with a newer schema version (version=1000000).
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    sql::MetaTable meta_table;
    constexpr int kFutureVersionNumber = 1000000;
    EXPECT_TRUE(meta_table.Init(&db, /*version=*/kFutureVersionNumber,
                                /*compatible_version=*/kFutureVersionNumber));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&db)) << db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_file_path()));
  // Calling Init DB with existing DB ahead of current version should fail.
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_FALSE(db->Init());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, DatabaseHasSchemaNoMeta) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Init DB with all tables including meta.
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());
  }

  // Drop meta table.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));
    sql::MetaTable::DeleteTableForTesting(&db);
  }

  // Init again with no meta should raze the DB and recreate again successfully.
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdatePowerIfExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  power->set_guid(base::GUID::GenerateRandomV4());
  auto power2 = power->Clone();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  EXPECT_TRUE(pbdb->UpdatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(GURL("https://boogle.com"));
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, ShouldNotUpdatePowerIfNotExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  // Update called when no Power can be found will fail and return false.
  EXPECT_FALSE(pbdb->UpdatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdatePowerShouldMergePower) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  base::Time now = base::Time::Now();
  power->set_time_modified(now);
  auto power2 = power->Clone();
  power2->set_time_modified(now - base::Seconds(1));

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());

  // Make sure time modified does not change after a merge.
  EXPECT_EQ(now, stored_powers[0]->time_modified());
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdateNotesWithMerge) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  power->power_entity()->mutable_note_entity()->set_plain_text("now");

  base::Time now = base::Time::Now();
  power->set_time_modified(now);
  auto power_updated_before = power->Clone();
  power_updated_before->set_time_modified(now - base::Seconds(1));
  power_updated_before->power_entity()->mutable_note_entity()->set_plain_text(
      "before");

  auto power_updated_after = power->Clone();
  auto after_time_modified = now + base::Seconds(1);
  power_updated_after->set_time_modified(after_time_modified);
  power_updated_after->power_entity()->mutable_note_entity()->set_plain_text(
      "after");

  // Merge a note with an older one. Text will not change.
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power_updated_before)));
  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(now, stored_powers[0]->time_modified());
  EXPECT_EQ("now",
            stored_powers[0]->power_entity()->note_entity().plain_text());

  // Merge a note with a newer one. Text will change.
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power_updated_after)));
  stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(after_time_modified, stored_powers[0]->time_modified());
  EXPECT_EQ("after",
            stored_powers[0]->power_entity()->note_entity().plain_text());
}

TEST_F(PowerBookmarkDatabaseImplTest, CreatePowerIfNotExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(GURL("https://boogle.com"));
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, ShouldNotCreatePowerIfExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  power->set_guid(base::GUID::GenerateRandomV4());
  auto power2 = power->Clone();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  EXPECT_FALSE(pbdb->CreatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(GURL("https://boogle.com"));
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());
}

// // TODO(crbug.com/1383289): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetPowersForURLDeserializingProtoFails \
  DISABLED_GetPowersForURLDeserializingProtoFails
#else
#define MAYBE_GetPowersForURLDeserializingProtoFails \
  GetPowersForURLDeserializingProtoFails
#endif
TEST_F(PowerBookmarkDatabaseImplTest,
       MAYBE_GetPowersForURLDeserializingProtoFails) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Init DB
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());
  }

  InsertBadlyFormattedProtoToDB();

  // Init again and try to retrieve the badly serialized proto.
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());

    std::vector<std::unique_ptr<Power>> stored_powers =
        db->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
    EXPECT_EQ(0u, stored_powers.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsForType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://boogle.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<PowerOverview>> power_overviews =
      pbdb->GetPowerOverviewsForType(
          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(2u, power_overviews.size());
  EXPECT_EQ(GURL("https://google.com"), power_overviews[0]->power()->url());
  EXPECT_EQ(2u, power_overviews[0]->count());
  EXPECT_EQ(GURL("https://boogle.com"), power_overviews[1]->power()->url());
  EXPECT_EQ(1u, power_overviews[1]->count());
}
// // TODO(crbug.com/1383289): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetPowerOverviewsForTypeDeserializingProtoFails \
  DISABLED_GetPowerOverviewsForTypeDeserializingProtoFails
#else
#define MAYBE_GetPowerOverviewsForTypeDeserializingProtoFails \
  GetPowerOverviewsForTypeDeserializingProtoFails
#endif
TEST_F(PowerBookmarkDatabaseImplTest,
       MAYBE_GetPowerOverviewsForTypeDeserializingProtoFails) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Init DB
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());
  }

  InsertBadlyFormattedProtoToDB();

  // Init again and try to retrieve the badly serialized proto.
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> db =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
    EXPECT_TRUE(db->Init());

    std::vector<std::unique_ptr<PowerOverview>> overviews =
        db->GetPowerOverviewsForType(
            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
    EXPECT_EQ(0u, overviews.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForSearchParams) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/b1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a2.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  SearchParams search_params{.query = "/a"};
  std::vector<std::unique_ptr<Power>> search_results =
      pbdb->GetPowersForSearchParams(search_params);

  EXPECT_EQ(2u, search_results.size());
  EXPECT_TRUE(ContainsPower(search_results,
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
                            GURL("https://example.com/a1.html")));
  EXPECT_TRUE(ContainsPower(search_results,
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
                            GURL("https://example.com/a2.html")));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForSearchParamsMatchNoteText) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("lorem ipsum");
    EXPECT_TRUE(pbdb->CreatePower(
        MakePower(GURL("https://example.com/a1.html"),
                  sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
                  std::move(note_specifics))));
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("not a match");
    EXPECT_TRUE(pbdb->CreatePower(
        MakePower(GURL("https://example.com/a2.html"),
                  sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
                  std::move(note_specifics))));
  }

  SearchParams search_params{.query = "lorem"};
  std::vector<std::unique_ptr<Power>> search_results =
      pbdb->GetPowersForSearchParams(search_params);

  EXPECT_EQ(1u, search_results.size());
  EXPECT_TRUE(ContainsPower(search_results,
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
                            GURL("https://example.com/a1.html")));
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePower) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePower(stored_powers[0]->guid()));
  stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK));
  stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
            stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED));
  stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetAllPowers) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://bing.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetAllPowers();
  EXPECT_EQ(2u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForGUIDs) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto power2 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto power3 = MakePower(GURL("https://bing.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto guid1 = power1->guid().AsLowercaseString();
  auto guid2 = power2->guid().AsLowercaseString();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power1)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power2)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power3)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForGUIDs({guid1, guid2});
  EXPECT_EQ(2u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[1]->url());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerForGUID) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto power2 = MakePower(GURL("https://bing.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto guid1 = power1->guid().AsLowercaseString();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power1)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power2)));

  std::unique_ptr<Power> stored_power = pbdb->GetPowerForGUID(guid1);
  EXPECT_EQ(GURL("https://google.com"), stored_power->url());
}

}  // namespace power_bookmarks
