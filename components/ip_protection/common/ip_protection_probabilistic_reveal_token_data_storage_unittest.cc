// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_data_storage.h"

#include <cstdint>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

namespace {

constexpr size_t kPRTPointSize = 33;

int VersionFromMetaTable(sql::Database& db) {
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step()) {
    NOTREACHED();
  }
  return s.ColumnInt(0);
}

}  // namespace

class ProbabilisticRevealTokenDataStorageTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath DbPath() const {
    return temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ProbabilisticRevealTokens"));
  }

  base::FilePath GetSqlFilePath(std::string_view sql_filename) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    file_path = file_path.AppendASCII("components");
    file_path = file_path.AppendASCII("test");
    file_path = file_path.AppendASCII("data");
    file_path = file_path.AppendASCII("ip_protection");
    file_path = file_path.AppendASCII(sql_filename);

    EXPECT_TRUE(base::PathExists(file_path));
    return file_path;
  }

  size_t CountTokenEntries(sql::Database& db) {
    static const char kCountSQL[] = "SELECT COUNT(*) FROM tokens";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  size_t CountTokenEntriesOnPublicKey(sql::Database& db,
                                      std::string_view public_key) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM tokens WHERE public_key = ?";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    s.BindString(0, public_key);
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  size_t CountTokenEntriesOnEpoch(sql::Database& db, std::string_view epoch) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM tokens WHERE epoch_id = ?";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    s.BindString(0, epoch);
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  size_t CountTokenEntriesWithBatchSize(sql::Database& db, int64_t batch_size) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM tokens WHERE batch_size = ?";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    s.BindInt64(0, batch_size);
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  void OpenDatabase() {
    storage_.reset();
    storage_ =
        std::make_unique<IpProtectionProbabilisticRevealTokenDataStorage>(
            DbPath());
  }

  void CloseDatabase() { storage_.reset(); }

  IpProtectionProbabilisticRevealTokenDataStorage* storage() {
    return storage_.get();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<IpProtectionProbabilisticRevealTokenDataStorage> storage_;
};

TEST_F(ProbabilisticRevealTokenDataStorageTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  OpenDatabase();
  CloseDatabase();

  // An unused ProbabilisticRevealTokenDataStorage instance should not create
  // the database.
  EXPECT_FALSE(base::PathExists(DbPath()));

  OpenDatabase();
  // Trigger the lazy-initialization. No tokens are stored, as the outcome is
  // empty.
  TryGetProbabilisticRevealTokensOutcome outcome;
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  EXPECT_TRUE(base::PathExists(DbPath()));

  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));

  // [tokens], [meta].
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));

  EXPECT_EQ(5, VersionFromMetaTable(db));

  // `version`, `u`, `e`, `epoch_id`, `expiration`, `num_tokens_with_signal`,
  // `public_key`, and `batch_size`.
  EXPECT_EQ(8u, sql::test::CountTableColumns(&db, "tokens"));

  EXPECT_EQ(0u, CountTokenEntries(db));
}

TEST_F(ProbabilisticRevealTokenDataStorageTest,
       LoadFromFile_CurrentVersion_Success) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("probabilistic_reveal_tokens_v5.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  TryGetProbabilisticRevealTokensOutcome outcome;
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  // Expect that the original database was loaded with one token.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(5, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountTokenEntries(db));
}

TEST_F(ProbabilisticRevealTokenDataStorageTest,
       LoadFromFile_VersionTooOld_Failure) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("probabilistic_reveal_tokens_v4.too_old.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  TryGetProbabilisticRevealTokensOutcome outcome;
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  // Expect that the original database was razed and re-initialized.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(5, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountTokenEntries(db));
}

TEST_F(ProbabilisticRevealTokenDataStorageTest,
       LoadFromFile_VersionTooNew_Failure) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("probabilistic_reveal_tokens_v6.too_new.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  TryGetProbabilisticRevealTokensOutcome outcome;
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  // Expect that the original database was razed and re-initialized.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(5, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountTokenEntries(db));
}

TEST_F(ProbabilisticRevealTokenDataStorageTest, StoreTokenOutcome) {
  // Trigger the lazy-initialization with an empty token outcome.
  OpenDatabase();
  TryGetProbabilisticRevealTokensOutcome outcome;
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  // Expect that the database is empty.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(0u, CountTokenEntries(db));
  db.Close();

  // Store 3 tokens across two calls.
  OpenDatabase();
  outcome.tokens.emplace_back(/*version=*/1, std::string(kPRTPointSize, 'u'),
                              std::string(kPRTPointSize, 'e'));
  outcome.epoch_id = std::string(8, '0');
  outcome.expiration_time_seconds = 123;
  outcome.next_epoch_start_time_seconds = 456;
  outcome.num_tokens_with_signal = 100;
  outcome.public_key = "public_key";
  storage()->StoreTokenOutcome(outcome);

  TryGetProbabilisticRevealTokensOutcome outcome2;
  outcome2.tokens.emplace_back(/*version=*/1, std::string(kPRTPointSize, 'u'),
                               std::string(kPRTPointSize, 'e'));
  outcome2.tokens.emplace_back(/*version=*/1, std::string(kPRTPointSize, 'u'),
                               std::string(kPRTPointSize, 'e'));
  outcome2.epoch_id = std::string(8, '1');
  outcome2.expiration_time_seconds = 234;
  outcome2.next_epoch_start_time_seconds = 567;
  outcome2.num_tokens_with_signal = 200;
  outcome2.public_key = "public_key_2";
  storage()->StoreTokenOutcome(outcome2);
  CloseDatabase();

  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(3u, CountTokenEntries(db));
  EXPECT_EQ(1u, CountTokenEntriesOnPublicKey(
                    db, "cHVibGljX2tleQ"));  // base64url_encode(public_key)
  EXPECT_EQ(2u, CountTokenEntriesOnPublicKey(
                    db, "cHVibGljX2tleV8y"));  // base64url_encode(public_key_2)
  EXPECT_EQ(1u, CountTokenEntriesWithBatchSize(db, outcome.tokens.size()));
  EXPECT_EQ(2u, CountTokenEntriesWithBatchSize(db, outcome2.tokens.size()));
  EXPECT_EQ(1u, CountTokenEntriesOnEpoch(
                    db, "MDAwMDAwMDA"));  // base64url_encode(00000000)
  EXPECT_EQ(2u, CountTokenEntriesOnEpoch(
                    db, "MTExMTExMTE"));  // base64url_encode(11111111)
  CloseDatabase();
}

TEST_F(ProbabilisticRevealTokenDataStorageTest, OpenDatabaseThatIsAlreadyOpen) {
  // Open the database directly.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));

  // Trigger the lazy-initialization with an empty token outcome.
  OpenDatabase();
  TryGetProbabilisticRevealTokensOutcome outcome;
  EXPECT_DCHECK_DEATH_WITH(storage()->StoreTokenOutcome(outcome),
                           "Unexpected Sqlite error");
}

TEST_F(ProbabilisticRevealTokenDataStorageTest, OpenCorruptedDatabase) {
  // Create a corrupted database file.
  base::FilePath db_path = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("CorruptedProbabilisticRevealTokens"));
  base::File corrupted_db_file(db_path, base::File::FLAG_CREATE_ALWAYS |
                                            base::File::FLAG_WRITE |
                                            base::File::FLAG_READ);
  ASSERT_TRUE(corrupted_db_file.IsValid());
  constexpr char kCorruptedDatabase[] = "corrupted_database";
  corrupted_db_file.Write(0, base::as_byte_span(kCorruptedDatabase));
  corrupted_db_file.Close();

  // Expect that the corrupted database cannot be opened directly.
  sql::Database db(sql::test::kTestTag);
  EXPECT_FALSE(db.Open(db_path));
  db.Close();

  // Create the Token Data Storage with the corrupted database file.
  storage_.reset();
  storage_ = std::make_unique<IpProtectionProbabilisticRevealTokenDataStorage>(
      db_path);

  // Trigger the lazy-initialization by attempting to store a token.
  TryGetProbabilisticRevealTokensOutcome outcome;
  outcome.tokens.emplace_back(/*version=*/1, std::string(kPRTPointSize, 'u'),
                              std::string(kPRTPointSize, 'e'));
  outcome.epoch_id = std::string(8, '0');
  outcome.expiration_time_seconds = 123;
  outcome.next_epoch_start_time_seconds = 456;
  outcome.num_tokens_with_signal = 100;
  outcome.public_key = "public_key";
  storage()->StoreTokenOutcome(outcome);
  CloseDatabase();

  // Expect that the corrupted database was razed and re-initialized and the new
  // database has one token.
  EXPECT_TRUE(db.Open(db_path));
  EXPECT_EQ(1u, CountTokenEntries(db));
}

}  // namespace ip_protection
