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
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/power_bookmarks/common/power_test_util.h"
#include "components/power_bookmarks/common/search_params.h"
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

const sync_pb::PowerBookmarkSpecifics::PowerType kMockType =
    sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK;
const sync_pb::PowerBookmarkSpecifics::PowerType kNoteType =
    sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE;

std::string WritePower(PowerBookmarkDatabaseImpl* pbdb,
                       std::unique_ptr<Power> power) {
  std::string guid = power->guid_string();
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));
  return guid;
}

base::Time EpochAndSeconds(int seconds_after_epoch) {
  return base::Time::UnixEpoch() + base::Seconds(seconds_after_epoch);
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

bool ContainsPowerOverview(
    const std::vector<std::unique_ptr<PowerOverview>>& list,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    GURL url,
    size_t count) {
  for (const std::unique_ptr<PowerOverview>& power_overview : list) {
    if (power_overview->power()->power_type() == power_type &&
        power_overview->power()->url() == url &&
        power_overview->count() == count) {
      return true;
    }
  }
  return false;
}

}  // namespace

class PowerBookmarkDatabaseImplTest : public testing::Test {
 public:
  const GURL kGoogleUrl = GURL("https://google.com");
  const GURL kBoogleUrl = GURL("https://boogle.com");
  const GURL kExampleUrl = GURL("https://example.com");
  const GURL kAnotherUrl = GURL("https://another.com");

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
    save_statement.BindString(0, power->guid_string());
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
    blob_statement.BindString(0, power->guid_string());
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

  auto power = MakePower(kGoogleUrl, kMockType);
  power->set_guid(base::Uuid::GenerateRandomV4());
  auto power2 = power->Clone();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  EXPECT_TRUE(pbdb->UpdatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(kBoogleUrl);
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, ShouldNotUpdatePowerIfNotExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  // Update called when no Power can be found will fail and return false.
  EXPECT_FALSE(pbdb->UpdatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdatePowerShouldMergePower) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(kGoogleUrl, kMockType);
  base::Time now = base::Time::Now();
  power->set_time_modified(now);
  auto power2 = power->Clone();
  power2->set_time_modified(now - base::Seconds(1));

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());

  // Make sure time modified does not change after a merge.
  EXPECT_EQ(now, stored_powers[0]->time_modified());
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdateNotesWithMerge) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(kGoogleUrl, kNoteType);
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
      pbdb->GetPowersForURL(kGoogleUrl, kNoteType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(now, stored_powers[0]->time_modified());
  EXPECT_EQ("now",
            stored_powers[0]->power_entity()->note_entity().plain_text());

  // Merge a note with a newer one. Text will change.
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power_updated_after)));
  stored_powers = pbdb->GetPowersForURL(kGoogleUrl, kNoteType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(after_time_modified, stored_powers[0]->time_modified());
  EXPECT_EQ("after",
            stored_powers[0]->power_entity()->note_entity().plain_text());
}

TEST_F(PowerBookmarkDatabaseImplTest, WritePowerIfNotExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(kBoogleUrl);
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, ShouldNotCreatePowerIfExist) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(kGoogleUrl, kMockType);
  power->set_guid(base::Uuid::GenerateRandomV4());
  auto power2 = power->Clone();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  EXPECT_FALSE(pbdb->CreatePower(std::move(power2)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  // Create called when there is already a Power will fail and return false.
  stored_powers[0]->set_url(kBoogleUrl);
  EXPECT_FALSE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      kGoogleUrl, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());
}

// // TODO(crbug.com/40877748): Re-enable this test.
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
        db->GetPowersForURL(kGoogleUrl, kMockType);
    EXPECT_EQ(0u, stored_powers.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsForType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());
  PowerBookmarkDatabaseImpl* db = pbdb.get();

  std::string boogleGuid =
      WritePower(db, MakePower(kBoogleUrl, EpochAndSeconds(4)));
  WritePower(db, MakePower(kBoogleUrl, kNoteType, EpochAndSeconds(5)));

  WritePower(db, MakePower(kGoogleUrl, EpochAndSeconds(1)));
  WritePower(db, MakePower(kGoogleUrl, EpochAndSeconds(2)));
  std::string googleGuid =
      WritePower(db, MakePower(kGoogleUrl, EpochAndSeconds(3)));

  WritePower(db, MakePower(kExampleUrl, EpochAndSeconds(2)));
  WritePower(db, MakePower(kExampleUrl, EpochAndSeconds(7)));
  WritePower(db, MakePower(kExampleUrl, EpochAndSeconds(4)));
  std::string exampleGuid =
      WritePower(db, MakePower(kExampleUrl, EpochAndSeconds(9)));
  WritePower(db, MakePower(kExampleUrl, EpochAndSeconds(6)));

  WritePower(db, MakePower(kAnotherUrl, kNoteType, EpochAndSeconds(7)));
  WritePower(db, MakePower(kAnotherUrl, kNoteType, EpochAndSeconds(6)));
  WritePower(db, MakePower(kAnotherUrl, kNoteType, EpochAndSeconds(3)));
  WritePower(db, MakePower(kAnotherUrl, kNoteType, EpochAndSeconds(4)));

  std::vector<std::unique_ptr<PowerOverview>> power_overviews =
      pbdb->GetPowerOverviewsForType(kMockType);
  EXPECT_EQ(3u, power_overviews.size());

  const PowerOverview* example_power_overview = power_overviews[0].get();
  EXPECT_EQ(5u, example_power_overview->count());
  const Power* example_power = example_power_overview->power();
  EXPECT_EQ(kExampleUrl, example_power->url());
  EXPECT_EQ(exampleGuid, example_power->guid_string());
  EXPECT_EQ(EpochAndSeconds(9), example_power->time_modified());

  const PowerOverview* google_power_overview = power_overviews[1].get();
  EXPECT_EQ(3u, google_power_overview->count());
  const Power* google_power = google_power_overview->power();
  EXPECT_EQ(kGoogleUrl, google_power->url());
  EXPECT_EQ(googleGuid, google_power->guid_string());
  EXPECT_EQ(EpochAndSeconds(3), google_power->time_modified());

  const PowerOverview* boogle_power_overview = power_overviews[2].get();
  EXPECT_EQ(1u, boogle_power_overview->count());
  const Power* boogle_power = boogle_power_overview->power();
  EXPECT_EQ(kBoogleUrl, boogle_power->url());
  EXPECT_EQ(boogleGuid, boogle_power->guid_string());
  EXPECT_EQ(EpochAndSeconds(4), boogle_power->time_modified());
}

// // TODO(crbug.com/40877748): Re-enable this test.
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
        db->GetPowerOverviewsForType(kMockType);
    EXPECT_EQ(0u, overviews.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForSearchParams) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/b1.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a2.html"), kMockType)));

  SearchParams search_params{.query = "/a"};
  std::vector<std::unique_ptr<Power>> search_results =
      pbdb->GetPowersForSearchParams(search_params);

  EXPECT_EQ(2u, search_results.size());
  EXPECT_TRUE(ContainsPower(search_results, kMockType,
                            GURL("https://example.com/a1.html")));
  EXPECT_TRUE(ContainsPower(search_results, kMockType,
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
    EXPECT_TRUE(
        pbdb->CreatePower(MakePower(GURL("https://example.com/a1.html"),
                                    kNoteType, std::move(note_specifics))));
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("not a match");
    EXPECT_TRUE(
        pbdb->CreatePower(MakePower(GURL("https://example.com/a2.html"),
                                    kNoteType, std::move(note_specifics))));
  }

  SearchParams search_params{.query = "lorem"};
  std::vector<std::unique_ptr<Power>> search_results =
      pbdb->GetPowersForSearchParams(search_params);

  EXPECT_EQ(1u, search_results.size());
  EXPECT_TRUE(ContainsPower(search_results, kNoteType,
                            GURL("https://example.com/a1.html")));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForSearchParamsMatchType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));

  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("lorem ipsum");
    EXPECT_TRUE(
        pbdb->CreatePower(MakePower(GURL("https://example.com/a2.html"),
                                    kNoteType, std::move(note_specifics))));
  }

  SearchParams search_params{
      .power_type = sync_pb::PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK};
  std::vector<std::unique_ptr<Power>> search_results =
      pbdb->GetPowersForSearchParams(search_params);

  EXPECT_EQ(1u, search_results.size());
  EXPECT_TRUE(ContainsPower(search_results, kMockType,
                            GURL("https://example.com/a1.html")));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsForSearchParams) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/b1.html"), kMockType)));

  SearchParams search_params{.query = "/a"};
  std::vector<std::unique_ptr<PowerOverview>> search_results =
      pbdb->GetPowerOverviewsForSearchParams(search_params);

  EXPECT_EQ(2u, search_results.size());
  EXPECT_TRUE(ContainsPowerOverview(search_results, kMockType,
                                    GURL("https://example.com/a3.html"), 2));
  EXPECT_TRUE(ContainsPowerOverview(search_results, kMockType,
                                    GURL("https://example.com/a1.html"), 1));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsCaseSensitive) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));

  SearchParams case_sensitive_search_params{.query = "/A",
                                            .case_sensitive = true};
  EXPECT_EQ(0u,
            pbdb->GetPowerOverviewsForSearchParams(case_sensitive_search_params)
                .size());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsIgnoreCase) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a3.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/a1.html"), kMockType)));

  SearchParams case_insensitive_search_params{.query = "/A",
                                              .case_sensitive = false};
  EXPECT_EQ(
      2u, pbdb->GetPowerOverviewsForSearchParams(case_insensitive_search_params)
              .size());
}

TEST_F(PowerBookmarkDatabaseImplTest,
       GetPowerOverviewsForSearchParamsMatchNoteText) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("lorem ipsum 1");
    std::unique_ptr<Power> note_power =
        MakePower(GURL("https://example.com/a1.html"), kNoteType,
                  std::move(note_specifics));
    note_power->set_time_modified(base::Time::FromTimeT(1100000000));
    EXPECT_TRUE(pbdb->CreatePower(std::move(note_power)));
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("lorem ipsum 2");
    std::unique_ptr<Power> note_power =
        MakePower(GURL("https://example.com/a1.html"), kNoteType,
                  std::move(note_specifics));
    note_power->set_time_modified(base::Time::FromTimeT(1200000000));
    EXPECT_TRUE(pbdb->CreatePower(std::move(note_power)));
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("lorem ipsum 3");
    std::unique_ptr<Power> note_power =
        MakePower(GURL("https://example.com/a1.html"), kNoteType,
                  std::move(note_specifics));
    note_power->set_time_modified(base::Time::FromTimeT(1300000000));
    EXPECT_TRUE(pbdb->CreatePower(std::move(note_power)));
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("only ipsum");
    std::unique_ptr<Power> note_power =
        MakePower(GURL("https://example.com/a1.html"), kNoteType,
                  std::move(note_specifics));
    note_power->set_time_modified(base::Time::FromTimeT(1400000000));
    EXPECT_TRUE(pbdb->CreatePower(std::move(note_power)));
  }
  EXPECT_TRUE(pbdb->CreatePower(
      MakePower(GURL("https://example.com/lorem_ipsum.html"), kMockType)));

  // Test matches URL.
  {
    SearchParams search_params{.query = "lorem"};
    std::vector<std::unique_ptr<PowerOverview>> search_results =
        pbdb->GetPowerOverviewsForSearchParams(search_params);

    EXPECT_EQ(2u, search_results.size());
    EXPECT_TRUE(
        ContainsPowerOverview(search_results, kMockType,
                              GURL("https://example.com/lorem_ipsum.html"), 1));
    EXPECT_TRUE(ContainsPowerOverview(search_results, kNoteType,
                                      GURL("https://example.com/a1.html"), 4));
  }
  // Test doesn't match the latest power.
  {
    SearchParams search_params{
        .query = "lorem",
        .power_type = kNoteType,
    };
    std::vector<std::unique_ptr<PowerOverview>> search_results =
        pbdb->GetPowerOverviewsForSearchParams(search_params);

    EXPECT_EQ(1u, search_results.size());
    EXPECT_TRUE(ContainsPowerOverview(search_results, kNoteType,
                                      GURL("https://example.com/a1.html"), 4));
    EXPECT_EQ("lorem ipsum 3", search_results.at(0)
                                   ->power()
                                   ->power_entity()
                                   ->note_entity()
                                   .plain_text());
    EXPECT_EQ(base::Time::FromTimeT(1300000000),
              search_results.at(0)->power()->time_modified());
  }
  // Test matches the latest power.
  {
    SearchParams search_params{
        .query = "ipsum",
        .power_type = kNoteType,
    };
    std::vector<std::unique_ptr<PowerOverview>> search_results =
        pbdb->GetPowerOverviewsForSearchParams(search_params);

    EXPECT_EQ(1u, search_results.size());
    EXPECT_TRUE(ContainsPowerOverview(search_results, kNoteType,
                                      GURL("https://example.com/a1.html"), 4));
    EXPECT_EQ("only ipsum", search_results.at(0)
                                ->power()
                                ->power_entity()
                                ->note_entity()
                                .plain_text());
    EXPECT_EQ(base::Time::FromTimeT(1400000000),
              search_results.at(0)->power()->time_modified());
  }
  // Test matches no powers.
  {
    SearchParams search_params{
        .query = "not found anywhere",
    };
    std::vector<std::unique_ptr<PowerOverview>> search_results =
        pbdb->GetPowerOverviewsForSearchParams(search_params);

    EXPECT_EQ(0u, search_results.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest,
       GetPowerOverviewsForSearchParamsShouldNotMatchNoteUrl) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  {
    std::unique_ptr<sync_pb::PowerEntity> note_specifics =
        std::make_unique<sync_pb::PowerEntity>();
    note_specifics->mutable_note_entity()->set_plain_text("");
    std::unique_ptr<Power> note_power = MakePower(
        GURL("https://example.com/foo"), kNoteType, std::move(note_specifics));
    EXPECT_TRUE(pbdb->CreatePower(std::move(note_power)));
  }
  // Test should not match URL for notes.
  {
    SearchParams search_params{
        .query = "foo",
    };
    std::vector<std::unique_ptr<PowerOverview>> search_results =
        pbdb->GetPowerOverviewsForSearchParams(search_params);

    EXPECT_EQ(0u, search_results.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePower) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePower(stored_powers[0]->guid()));
  stored_powers = pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power = MakePower(kGoogleUrl, kMockType);
  auto guid = power->guid_string();
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  std::vector<std::string> guids;
  EXPECT_TRUE(pbdb->DeletePowersForURL(kGoogleUrl, kMockType, &guids));
  EXPECT_EQ(1u, guids.size());
  EXPECT_EQ(guid, guids[0]);

  stored_powers =
      pbdb->GetPowersForURL(GURL("https://google.com"),
                            sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForURL(kGoogleUrl, kMockType);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kMockType, stored_powers[0]->power_type());

  std::vector<std::string> guids;
  EXPECT_TRUE(pbdb->DeletePowersForURL(
      kGoogleUrl, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      &guids));
  stored_powers = pbdb->GetPowersForURL(
      kGoogleUrl, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetAllPowers) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  EXPECT_TRUE(pbdb->CreatePower(MakePower(kGoogleUrl, kMockType)));

  EXPECT_TRUE(
      pbdb->CreatePower(MakePower(GURL("https://bing.com"), kMockType)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetAllPowers();
  EXPECT_EQ(2u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForGUIDs) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power1 = MakePower(kGoogleUrl, kMockType);
  auto power2 = MakePower(kGoogleUrl, kMockType);
  auto power3 = MakePower(GURL("https://bing.com"), kMockType);
  auto guid1 = power1->guid_string();
  auto guid2 = power2->guid_string();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power1)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power2)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power3)));

  std::vector<std::unique_ptr<Power>> stored_powers =
      pbdb->GetPowersForGUIDs({guid1, guid2});
  EXPECT_EQ(2u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
  EXPECT_EQ(kGoogleUrl, stored_powers[1]->url());

  stored_powers = pbdb->GetPowersForGUIDs({guid1});
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(kGoogleUrl, stored_powers[0]->url());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerForGUID) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  auto power1 = MakePower(kGoogleUrl, kMockType);
  auto power2 = MakePower(GURL("https://bing.com"), kMockType);
  auto guid1 = power1->guid_string();

  EXPECT_TRUE(pbdb->CreatePower(std::move(power1)));
  EXPECT_TRUE(pbdb->CreatePower(std::move(power2)));

  std::unique_ptr<Power> stored_power = pbdb->GetPowerForGUID(guid1);
  EXPECT_EQ(kGoogleUrl, stored_power->url());
}

}  // namespace power_bookmarks
