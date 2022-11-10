// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "components/power_bookmarks/core/proto/power_bookmark_specifics.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

class PowerBookmarkDatabaseImplTest : public testing::Test {
 public:
  PowerBookmarkDatabaseImplTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }
  void TearDown() override { EXPECT_TRUE(temp_directory_.Delete()); }

  base::FilePath db_dir() { return temp_directory_.GetPath(); }

  base::FilePath db_file_path() {
    return temp_directory_.GetPath().Append(kDatabaseName);
  }

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

    std::unique_ptr<PowerSpecifics> power_specifics =
        std::make_unique<PowerSpecifics>();
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
}

TEST_F(PowerBookmarkDatabaseImplTest, InitDatabaseError) {
  EXPECT_FALSE(base::PathExists(db_file_path()));
  {
    std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
        std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());

    EXPECT_FALSE(base::PathExists(db_file_path()));

    EXPECT_TRUE(pbdb->Init());
    EXPECT_TRUE(base::PathExists(db_file_path()));
  }

  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    // Database should have 2 tables: meta, saves.
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
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

TEST_F(PowerBookmarkDatabaseImplTest, CreatePowerWhenUpdateCalled) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);

  // Update called when no Power can be found will result in a create.
  EXPECT_TRUE(pbdb->UpdatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());
}

TEST_F(PowerBookmarkDatabaseImplTest, UpdatePowerWhenCreateCalled) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());

  // Create called when there is already a Power will perform an update.
  stored_powers[0]->set_url(GURL("https://boogle.com"));
  EXPECT_TRUE(pbdb->CreatePower(std::move(stored_powers[0])));
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_UNSPECIFIED);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowersForURLDeserializingProtoFails) {
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

    std::vector<std::unique_ptr<Power>> stored_powers = db->GetPowersForURL(
        GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
    EXPECT_EQ(0u, stored_powers.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, GetPowerOverviewsForType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  power_specifics = std::make_unique<PowerSpecifics>();
  power = std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  power_specifics = std::make_unique<PowerSpecifics>();
  power = std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://boogle.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<PowerOverview>> power_overviews =
      pbdb->GetPowerOverviewsForType(PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(2u, power_overviews.size());
  EXPECT_EQ(GURL("https://google.com"), power_overviews[0]->power()->url());
  EXPECT_EQ(2u, power_overviews[0]->count());
  EXPECT_EQ(GURL("https://boogle.com"), power_overviews[1]->power()->url());
  EXPECT_EQ(1u, power_overviews[1]->count());
}

TEST_F(PowerBookmarkDatabaseImplTest,
       GetPowerOverviewsForTypeDeserializingProtoFails) {
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
        db->GetPowerOverviewsForType(PowerType::POWER_TYPE_MOCK);
    EXPECT_EQ(0u, overviews.size());
  }
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePower) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePower(stored_powers[0]->guid()));
  stored_powers = pbdb->GetPowersForURL(GURL("https://google.com"),
                                        PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURL) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePowersForURL(GURL("https://google.com"),
                                       PowerType::POWER_TYPE_MOCK));
  stored_powers = pbdb->GetPowersForURL(GURL("https://google.com"),
                                        PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

TEST_F(PowerBookmarkDatabaseImplTest, DeletePowersForURLUnspecifiedType) {
  std::unique_ptr<PowerBookmarkDatabaseImpl> pbdb =
      std::make_unique<PowerBookmarkDatabaseImpl>(db_dir());
  EXPECT_TRUE(pbdb->Init());

  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_url(GURL("https://google.com"));
  power->set_power_type(PowerType::POWER_TYPE_MOCK);
  EXPECT_TRUE(pbdb->CreatePower(std::move(power)));

  std::vector<std::unique_ptr<Power>> stored_powers = pbdb->GetPowersForURL(
      GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(1u, stored_powers.size());
  EXPECT_EQ(GURL("https://google.com"), stored_powers[0]->url());
  EXPECT_EQ(PowerType::POWER_TYPE_MOCK, stored_powers[0]->power_type());

  EXPECT_TRUE(pbdb->DeletePowersForURL(GURL("https://google.com"),
                                       PowerType::POWER_TYPE_UNSPECIFIED));
  stored_powers = pbdb->GetPowersForURL(GURL("https://google.com"),
                                        PowerType::POWER_TYPE_MOCK);
  EXPECT_EQ(0u, stored_powers.size());
}

}  // namespace power_bookmarks