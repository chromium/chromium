// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sqlite_encrypted_vfs/database_key_manager.h"
#include "components/sqlite_encrypted_vfs/encrypted_database.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.h"
#include "sql/database.h"
#include "sql/initialization.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_encrypted_vfs {

namespace {

class CustomTestEncryptor : public os_crypt_async::Encryptor {
 public:
  CustomTestEncryptor(KeyRing keys, const std::string& provider_for_encryption)
      : Encryptor(std::move(keys), provider_for_encryption) {}

 private:
  friend class base::RefCountedThreadSafe<Encryptor>;
  ~CustomTestEncryptor() override = default;
};

os_crypt_async::Encryptor::Key MakeTestOsKey(uint8_t fill) {
  return os_crypt_async::Encryptor::Key(
      std::vector<uint8_t>(os_crypt_async::Encryptor::Key::kAES256GCMKeySize,
                           fill),
      os_crypt_async::mojom::Algorithm::kAES256GCM);
}

// State for `ScopedFailingVfsDelete`. Only set while one is alive.
int (*g_original_xdelete)(sqlite3_vfs*, const char*, int) = nullptr;
const std::string* g_undeletable_path = nullptr;

int FailingXDelete(sqlite3_vfs* vfs, const char* path, int sync_dir) {
  if (g_undeletable_path && path && *g_undeletable_path == path) {
    return SQLITE_IOERR_DELETE;
  }
  return g_original_xdelete(vfs, path, sync_dir);
}

// Patches SQLite's default OS VFS so that deleting `path` fails (as with a
// file lock or I/O error), and restores the VFS on destruction. `path` must be
// spelled exactly as SQLite receives it, i.e. normalized.
class ScopedFailingVfsDelete {
 public:
  explicit ScopedFailingVfsDelete(const base::FilePath& path)
      : path_(path.AsUTF8Unsafe()) {
    sql::EnsureSqliteInitialized();
    vfs_ = sqlite3_vfs_find(nullptr);
    CHECK(vfs_);
    CHECK(!g_original_xdelete);
    g_original_xdelete = std::exchange(vfs_->xDelete, &FailingXDelete);
    g_undeletable_path = &path_;
  }

  ScopedFailingVfsDelete(const ScopedFailingVfsDelete&) = delete;
  ScopedFailingVfsDelete& operator=(const ScopedFailingVfsDelete&) = delete;

  ~ScopedFailingVfsDelete() {
    vfs_->xDelete = std::exchange(g_original_xdelete, nullptr);
    g_undeletable_path = nullptr;
  }

 private:
  const std::string path_;
  raw_ptr<sqlite3_vfs> vfs_ = nullptr;
};

// Changes the current working directory for the lifetime of the object.
class ScopedCurrentDirectory {
 public:
  explicit ScopedCurrentDirectory(const base::FilePath& path) {
    CHECK(base::GetCurrentDirectory(&original_));
    CHECK(base::SetCurrentDirectory(path));
  }

  ScopedCurrentDirectory(const ScopedCurrentDirectory&) = delete;
  ScopedCurrentDirectory& operator=(const ScopedCurrentDirectory&) = delete;

  ~ScopedCurrentDirectory() { CHECK(base::SetCurrentDirectory(original_)); }

 private:
  base::FilePath original_;
};

}  // namespace

class SqliteEncryptedVfsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("encrypted_vfs_test.sqlite");
    key_manager_ = DatabaseKeyManager::CreateForTesting(
        os_crypt_async::GetTestEncryptorForTesting());
  }

  void TearDown() override { key_manager_.reset(); }

  // Constructs an `EncryptedDatabase` and opens it synchronously. Returns
  // `nullptr` if `Open()` fails.
  std::unique_ptr<EncryptedDatabase> OpenDatabase(
      const base::FilePath& db_path,
      sql::DatabaseOptions options = sql::DatabaseOptions()) {
    auto database = std::make_unique<EncryptedDatabase>(std::move(options),
                                                        sql::test::kTestTag);
    return database->Open(db_path) == EncryptedDatabase::OpenResult::kOk
               ? std::move(database)
               : nullptr;
  }

  base::FilePath KeyPath(const base::FilePath& db_path) const {
    return DatabaseKeyManager::GetKeyFilePathForTesting(db_path);
  }

  // Resolves `db_path` through SQLite's default OS VFS the same way
  // `EncryptedDatabase` does (e.g. `/var/...` -> `/private/var/...` on macOS).
  base::FilePath NormalizedPath(const base::FilePath& db_path) const {
    sql::EnsureSqliteInitialized();
    sqlite3_vfs* base_vfs = sqlite3_vfs_find(nullptr);
    CHECK(base_vfs);
    std::vector<char> full_path(std::max(base_vfs->mxPathname + 1, 1024), '\0');
    std::string utf8 = db_path.AsUTF8Unsafe();
    int rc = base_vfs->xFullPathname(base_vfs, utf8.c_str(),
                                     static_cast<int>(full_path.size()) - 1,
                                     full_path.data());
    return (rc == SQLITE_OK || rc == SQLITE_OK_SYMLINK)
               ? base::FilePath::FromUTF8Unsafe(full_path.data())
               : db_path;
  }

  // Mirrors `GetVfsName()` in sqlite_encrypted_vfs.cc, which documents how
  // (rare) hash collisions between database paths fail closed.
  std::string ExpectedVfsName(const base::FilePath& db_path) const {
    uint32_t path_hash =
        base::PersistentHash(NormalizedPath(db_path).AsUTF8Unsafe());
    return base::StringPrintf("sqlite_encrypted_vfs_%08x", path_hash);
  }

  std::vector<base::FilePath> GetFilesInTempDir() const {
    std::vector<base::FilePath> files;
    base::FileEnumerator enumerator(temp_dir_.GetPath(), /*recursive=*/false,
                                    base::FileEnumerator::FILES);
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      files.push_back(name.BaseName());
    }
    return files;
  }

  void VerifyDatabaseFiles() const {
    EXPECT_TRUE(base::PathExists(db_path_));
    EXPECT_THAT(GetFilesInTempDir(),
                testing::UnorderedElementsAre(
                    db_path_.BaseName(),
                    sql::Database::JournalPath(db_path_).BaseName(),
                    KeyPath(db_path_).BaseName()));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<DatabaseKeyManager> key_manager_;
};

TEST_F(SqliteEncryptedVfsTest, BasicOperations) {
  base::HistogramTester histograms;
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    sql::Database& db = encrypted_db->DB();

    EXPECT_TRUE(
        db.Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, title TEXT)"))
        << db.GetErrorMessage();
    EXPECT_TRUE(
        db.Execute("INSERT INTO foo (id, title) VALUES (1, 'hello world')"))
        << db.GetErrorMessage();

    sql::Statement s(
        db.GetUniqueStatement("SELECT title FROM foo WHERE id = 1"));
    EXPECT_TRUE(s.is_valid()) << db.GetErrorMessage();
    EXPECT_TRUE(s.Step()) << db.GetErrorMessage();
    EXPECT_EQ(s.ColumnString(0), "hello world");
  }

  VerifyDatabaseFiles();
  histograms.ExpectUniqueSample(
      GetDatabaseActionHistogramName(DatabaseAction::kOpen), true, 1);
  histograms.ExpectTotalCount(
      GetDatabaseActionTimeHistogramName(DatabaseAction::kOpen), 1);
}

TEST_F(SqliteEncryptedVfsTest, TransactionAndRollback) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    sql::Database& db = encrypted_db->DB();
    ASSERT_TRUE(
        db.Execute("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)"));

    // Rolled back transaction.
    {
      sql::Transaction transaction(&db);
      ASSERT_TRUE(transaction.Begin());
      ASSERT_TRUE(
          db.Execute("INSERT INTO items (id, name) VALUES (1, 'apple')"));
      transaction.Rollback();
    }

    {
      sql::Statement s(
          db.GetUniqueStatement("SELECT COUNT(*) FROM items WHERE id = 1"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnInt(0), 0);
    }

    // Committed transaction.
    {
      sql::Transaction transaction(&db);
      ASSERT_TRUE(transaction.Begin());
      ASSERT_TRUE(
          db.Execute("INSERT INTO items (id, name) VALUES (2, 'banana')"));
      ASSERT_TRUE(transaction.Commit());
    }

    {
      sql::Statement s(
          db.GetUniqueStatement("SELECT name FROM items WHERE id = 2"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnString(0), "banana");
    }
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, ReopenDatabase) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(
        encrypted_db->DB().Execute("CREATE TABLE kv (k TEXT PRIMARY KEY, v "
                                   "TEXT)"));
    ASSERT_TRUE(encrypted_db->DB().Execute(
        "INSERT INTO kv (k, v) VALUES ('key1', 'val1')"));
  }

  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    sql::Statement s(encrypted_db->DB().GetUniqueStatement(
        "SELECT v FROM kv WHERE k = 'key1'"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "val1");
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, MultiPageOperations) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    sql::Database& db = encrypted_db->DB();
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE large (id INTEGER PRIMARY KEY, payload TEXT)"));

    std::string large_string(1024, 'x');
    sql::Statement s(
        db.GetUniqueStatement("INSERT INTO large (payload) VALUES (?)"));
    for (int i = 0; i < 50; ++i) {
      s.Reset(/*clear_bound_vars=*/true);
      s.BindString(0, large_string);
      ASSERT_TRUE(s.Run());
    }
  }

  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    sql::Statement count_stmt(
        encrypted_db->DB().GetUniqueStatement("SELECT COUNT(*) FROM large"));
    ASSERT_TRUE(count_stmt.Step());
    EXPECT_EQ(count_stmt.ColumnInt(0), 50);
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, MmapRequestIsIgnored) {
  // Even if a consumer asks for mmap, it must be disabled: mmap I/O bypasses
  // `xRead` and would hand raw ciphertext to SQLite.
  std::unique_ptr<EncryptedDatabase> encrypted_db =
      OpenDatabase(db_path_, sql::DatabaseOptions().set_mmap_enabled(true));
  ASSERT_TRUE(encrypted_db);
  sql::Database& db = encrypted_db->DB();

  EXPECT_TRUE(
      db.Execute("CREATE TABLE data (id INTEGER PRIMARY KEY, content TEXT)"))
      << db.GetErrorMessage();
  EXPECT_TRUE(
      db.Execute("INSERT INTO data (id, content) VALUES (1, 'no mmap')"))
      << db.GetErrorMessage();

  sql::Statement s(
      db.GetUniqueStatement("SELECT content FROM data WHERE id = 1"));
  ASSERT_TRUE(s.Step()) << db.GetErrorMessage();
  EXPECT_EQ(s.ColumnString(0), "no mmap");
}

TEST_F(SqliteEncryptedVfsTest, EachDatabaseGetsItsOwnRegisteredVfs) {
  std::unique_ptr<EncryptedDatabase> first = OpenDatabase(db_path_);
  base::FilePath other_path = temp_dir_.GetPath().AppendASCII("other.sqlite");
  std::unique_ptr<EncryptedDatabase> second = OpenDatabase(other_path);

  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  std::string first_name = ExpectedVfsName(db_path_);
  std::string second_name = ExpectedVfsName(other_path);
  EXPECT_NE(first_name, second_name);

  sqlite3_vfs* vfs = sqlite3_vfs_find(first_name.c_str());
  ASSERT_NE(vfs, nullptr);
  EXPECT_GE(vfs->iVersion, 3);
  EXPECT_GT(vfs->szOsFile, 0);
  EXPECT_EQ(vfs->xDlOpen, nullptr);  // Dynamic extensions disabled.

  // Registration is scoped to the instance.
  first.reset();
  EXPECT_EQ(sqlite3_vfs_find(first_name.c_str()), nullptr);
}

TEST_F(SqliteEncryptedVfsTest, RejectsDuplicateVfsRegistration) {
  std::unique_ptr<EncryptedDatabase> first = OpenDatabase(db_path_);
  ASSERT_TRUE(first);

  std::string vfs_name = ExpectedVfsName(db_path_);
  sqlite3_vfs* original_vfs = sqlite3_vfs_find(vfs_name.c_str());
  ASSERT_NE(original_vfs, nullptr);

  // Attempting to register a second VFS with the same name while `first` is
  // still alive must fail closed rather than replacing the existing VFS.
  EXPECT_FALSE(OpenDatabase(db_path_));
  EXPECT_EQ(sqlite3_vfs_find(vfs_name.c_str()), original_vfs);
}

TEST_F(SqliteEncryptedVfsTest, RefusesToOpenADifferentDatabase) {
  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
  ASSERT_TRUE(encrypted_db);

  std::string vfs_name = ExpectedVfsName(db_path_);
  ASSERT_NE(sqlite3_vfs_find(vfs_name.c_str()), nullptr);

  // The VFS holds the key for `db_path_` only. Opening anything else through
  // it is a programming error and must `CHECK`-fail rather than encrypt it with
  // the wrong key.
  sql::Database intruder_db(
      sql::DatabaseOptions().set_vfs_name_discouraged(vfs_name.c_str()),
      sql::test::kTestTag);
  EXPECT_CHECK_DEATH(std::ignore = intruder_db.Open(
                         temp_dir_.GetPath().AppendASCII("intruder.sqlite")));
}

TEST_F(SqliteEncryptedVfsTest, JournalDeletionPreservesKeyFile) {
  // Regression test: SQLite deletes the rollback journal routinely (on every
  // commit in DELETE journal mode). That must never take the key file with it.
  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
  ASSERT_TRUE(encrypted_db);
  ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE del (id INT)"));
  ASSERT_TRUE(base::PathExists(KeyPath(db_path_)));

  // Delete the journal, the WAL and the shm through the registered VFS
  // directly.
  std::string vfs_name = ExpectedVfsName(db_path_);
  sqlite3_vfs* encrypted_vfs = sqlite3_vfs_find(vfs_name.c_str());
  ASSERT_NE(encrypted_vfs, nullptr);

  for (const base::FilePath& path :
       {sql::Database::JournalPath(db_path_),
        sql::Database::WriteAheadLogPath(db_path_),
        sql::Database::SharedMemoryFilePath(db_path_)}) {
    encrypted_vfs->xDelete(encrypted_vfs, path.AsUTF8Unsafe().c_str(),
                           /*syncDir=*/0);
  }

  EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));
  EXPECT_TRUE(base::PathExists(db_path_));

  // The open database is unaffected.
  EXPECT_TRUE(encrypted_db->DB().Execute("INSERT INTO del VALUES (1)"));
}

TEST_F(SqliteEncryptedVfsTest, FailsClosedWithoutAKey) {
  base::HistogramTester histograms;
  // A key manager that cannot produce a key must not yield a database, and
  // must never fall back to an unencrypted one.
  key_manager_.reset();
  key_manager_ = DatabaseKeyManager::CreateForTesting(
      os_crypt_async::GetTestEncryptorWithoutKeysForTesting());

  EXPECT_FALSE(OpenDatabase(db_path_));
  EXPECT_FALSE(base::PathExists(db_path_));
  histograms.ExpectUniqueSample(
      GetDatabaseActionHistogramName(DatabaseAction::kOpen), false, 1);
  histograms.ExpectTotalCount(
      GetDatabaseActionTimeHistogramName(DatabaseAction::kOpen), 1);
}

TEST_F(SqliteEncryptedVfsTest, FailsClosedOnEmptyPath) {
  EXPECT_FALSE(OpenDatabase(base::FilePath()));
}

TEST_F(SqliteEncryptedVfsTest, FailsClosedOnRelativeOrParentRefPath) {
  EXPECT_FALSE(
      OpenDatabase(base::FilePath(FILE_PATH_LITERAL("relative.sqlite"))));
  EXPECT_FALSE(OpenDatabase(temp_dir_.GetPath()
                                .Append(base::FilePath::kParentDirectory)
                                .AppendASCII("test.sqlite")));
}

TEST_F(SqliteEncryptedVfsTest, FailsClosedOnCorruptedKeyFile) {
  base::FilePath bad_db_path =
      temp_dir_.GetPath().AppendASCII("corrupted_key.sqlite");
  ASSERT_TRUE(base::WriteFile(KeyPath(bad_db_path), "corrupted_garbage_bytes"));

  EXPECT_FALSE(OpenDatabase(bad_db_path));
  EXPECT_FALSE(base::PathExists(bad_db_path));
}

TEST_F(SqliteEncryptedVfsTest, DeleteRemovesDatabaseAndKeyFile) {
  base::HistogramTester histograms;
  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
  ASSERT_TRUE(encrypted_db);
  ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE gone (id INT)"));
  ASSERT_TRUE(base::PathExists(db_path_));
  ASSERT_TRUE(base::PathExists(KeyPath(db_path_)));

  EXPECT_TRUE(encrypted_db->Delete());

  EXPECT_FALSE(base::PathExists(db_path_));
  EXPECT_FALSE(base::PathExists(KeyPath(db_path_)));
  EXPECT_THAT(GetFilesInTempDir(), testing::IsEmpty());
  histograms.ExpectUniqueSample(
      GetDatabaseActionHistogramName(DatabaseAction::kDelete), true, 1);
  histograms.ExpectTotalCount(
      GetDatabaseActionTimeHistogramName(DatabaseAction::kDelete), 1);
}

TEST_F(SqliteEncryptedVfsTest, DeleteThenReopenMintsAFreshKey) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
    EXPECT_TRUE(encrypted_db->Delete());
  }

  // The key file was removed along with the database, so this opens a brand
  // new database and mints a fresh key.
  std::unique_ptr<EncryptedDatabase> reopened = OpenDatabase(db_path_);
  ASSERT_TRUE(reopened);
  EXPECT_TRUE(reopened->DB().Execute("CREATE TABLE t (id INT)"));
  EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));
}

TEST_F(SqliteEncryptedVfsTest, DBAfterPoison) {
  EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
  ASSERT_EQ(db.Open(db_path_), EncryptedDatabase::OpenResult::kOk);
  db.DB().Poison();

  EXPECT_FALSE(db.DB().Execute("SELECT 1"));
}

TEST_F(SqliteEncryptedVfsTest, ErrorCallbackAndIsOpen) {
  EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
  EXPECT_FALSE(db.is_open());

  int error_count = 0;
  db.set_error_callback(base::BindLambdaForTesting(
      [&](int error, sql::Statement* stmt) { ++error_count; }));

  ASSERT_EQ(db.Open(db_path_), EncryptedDatabase::OpenResult::kOk);
  EXPECT_TRUE(db.is_open());

  EXPECT_FALSE(db.DB().Execute("SELECT * FROM nonexistent_table"));
  EXPECT_EQ(error_count, 1);

  db.reset_error_callback();
  EXPECT_FALSE(db.DB().Execute("SELECT * FROM nonexistent_table"));
  EXPECT_EQ(error_count, 1);
}

TEST_F(SqliteEncryptedVfsTest, ReportsTransientVsPermanentKeyError) {
  os_crypt_async::Encryptor::KeyRing active_keys;
  active_keys.emplace(
      "KEY_V1", os_crypt_async::Encryptor::Key(
                    std::vector<uint8_t>(
                        os_crypt_async::Encryptor::Key::kAES256GCMKeySize, '1'),
                    os_crypt_async::mojom::Algorithm::kAES256GCM));
  key_manager_ = DatabaseKeyManager::CreateForTesting(
      base::MakeRefCounted<CustomTestEncryptor>(std::move(active_keys),
                                                "KEY_V1"));

  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
  }

  // Simulate `"KEY_V1"` being temporarily unavailable.
  os_crypt_async::Encryptor::KeyRing unavailable_keys;
  unavailable_keys.emplace("KEY_V1", std::nullopt);
  key_manager_ = DatabaseKeyManager::CreateForTesting(
      base::MakeRefCounted<CustomTestEncryptor>(
          std::move(unavailable_keys), /*provider_for_encryption=*/""));

  {
    EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
    EXPECT_EQ(db.Open(db_path_),
              EncryptedDatabase::OpenResult::kTransientError);
  }

  // Corrupted `-key` file must report `kKeyUnrecoverable`.
  ASSERT_TRUE(base::WriteFile(KeyPath(db_path_), "corrupted_key_bytes"));
  {
    EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
    EXPECT_EQ(db.Open(db_path_),
              EncryptedDatabase::OpenResult::kKeyUnrecoverable);
  }
}

// Verifies the recovery path for a lost OS-level key: `Open()` reports
// `kKeyUnrecoverable` without touching the files, and the caller can then
// `Delete()` the database and open a fresh one under a new key.
TEST_F(SqliteEncryptedVfsTest, KeyUnrecoverableThenDeleteAndReopen) {
  auto make_encryptor = [](const std::string& tag, char key_byte) {
    os_crypt_async::Encryptor::KeyRing keys;
    keys.emplace(tag, os_crypt_async::Encryptor::Key(
                          std::vector<uint8_t>(
                              os_crypt_async::Encryptor::Key::kAES256GCMKeySize,
                              key_byte),
                          os_crypt_async::mojom::Algorithm::kAES256GCM));
    return base::MakeRefCounted<CustomTestEncryptor>(std::move(keys), tag);
  };

  key_manager_ =
      DatabaseKeyManager::CreateForTesting(make_encryptor("KEY_V1", '1'));
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE old (id INT)"));
  }

  // Simulate the OS-level key being reset: only an unrelated key is available.
  key_manager_ =
      DatabaseKeyManager::CreateForTesting(make_encryptor("KEY_V2", '2'));
  {
    EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
    EXPECT_EQ(db.Open(db_path_),
              EncryptedDatabase::OpenResult::kKeyUnrecoverable);
    // Nothing is deleted until the caller asks for it.
    EXPECT_TRUE(base::PathExists(db_path_));
    EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));
    EXPECT_TRUE(db.Delete());
  }
  EXPECT_FALSE(base::PathExists(db_path_));
  EXPECT_FALSE(base::PathExists(KeyPath(db_path_)));

  std::unique_ptr<EncryptedDatabase> reopened = OpenDatabase(db_path_);
  ASSERT_TRUE(reopened);
  EXPECT_FALSE(reopened->DB().DoesTableExist("old"));
  EXPECT_TRUE(reopened->DB().Execute("CREATE TABLE fresh (id INT)"));
}

// Verifies the consumer pattern: `DatabaseKeyManager` is initialized with an
// asynchronous `OSCryptAsync` on the UI/main sequence, a
// `base::DeferredSequencedTaskRunner` wrapping a `base::MayBlock()` worker
// sequence is started via `DatabaseKeyManager::WhenReady()`, and synchronous
// `EncryptedDatabase::Open()` / `Delete()` tasks posted before the `Encryptor`
// is ready run on the worker sequence once `WhenReady()` fires.
TEST_F(SqliteEncryptedVfsTest, OpenAndDeleteOnBackgroundWorkerSequence) {
  key_manager_.reset();
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/false);
  key_manager_ = DatabaseKeyManager::CreateForTesting(*os_crypt_async);

  auto worker_task_runner =
      base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  DatabaseKeyManager::WhenReady(base::BindOnce(
      &base::DeferredSequencedTaskRunner::Start, worker_task_runner));
  EXPECT_FALSE(worker_task_runner->Started());

  base::RunLoop done;
  worker_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(worker_task_runner->RunsTasksInCurrentSequence());
        EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
        ASSERT_EQ(db.Open(db_path_), EncryptedDatabase::OpenResult::kOk);
        EXPECT_TRUE(db.DB().Execute("CREATE TABLE t (id INT)"));
        EXPECT_TRUE(db.DB().Execute("INSERT INTO t VALUES (7)"));
        EXPECT_TRUE(db.Delete());
        done.Quit();
      }));
  done.Run();

  EXPECT_TRUE(worker_task_runner->Started());
  EXPECT_FALSE(base::PathExists(db_path_));
  EXPECT_FALSE(base::PathExists(KeyPath(db_path_)));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(SqliteEncryptedVfsTest, SymlinkPathResolution) {
  base::FilePath real_dir = temp_dir_.GetPath().AppendASCII("real_dir");
  ASSERT_TRUE(base::CreateDirectory(real_dir));
  base::FilePath symlink_dir = temp_dir_.GetPath().AppendASCII("symlink_dir");
  ASSERT_TRUE(base::CreateSymbolicLink(real_dir, symlink_dir));

  base::FilePath symlink_db_path = symlink_dir.AppendASCII("symlink.sqlite");

  {
    std::unique_ptr<EncryptedDatabase> encrypted_db =
        OpenDatabase(symlink_db_path);
    ASSERT_TRUE(encrypted_db);
    EXPECT_TRUE(
        encrypted_db->DB().Execute("CREATE TABLE s (id INT, val TEXT)"));
    EXPECT_TRUE(encrypted_db->DB().Execute(
        "INSERT INTO s VALUES (42, 'symlink_success')"));
  }

  // The same database reached through the resolved path is recognized as the
  // VFS' own database.
  base::FilePath real_db_path = real_dir.AppendASCII("symlink.sqlite");
  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(real_db_path);
  ASSERT_TRUE(encrypted_db);
  sql::Statement s(
      encrypted_db->DB().GetUniqueStatement("SELECT val FROM s WHERE id = 42"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(s.ColumnString(0), "symlink_success");
}

TEST_F(SqliteEncryptedVfsTest, SymlinkedDatabaseFile) {
  base::FilePath real_path = temp_dir_.GetPath().AppendASCII("real.sqlite");
  base::FilePath link_path = temp_dir_.GetPath().AppendASCII("link.sqlite");
  ASSERT_TRUE(base::WriteFile(real_path, ""));
  ASSERT_TRUE(base::CreateSymbolicLink(real_path, link_path));
  for (const base::FilePath& path : {link_path, real_path}) {
    EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
    ASSERT_EQ(db.Open(path), EncryptedDatabase::OpenResult::kOk);
  }

  // One database, one key file (`real.sqlite-key`).
  EXPECT_FALSE(base::PathExists(
      DatabaseKeyManager::GetKeyFilePathForTesting(link_path)));
  EXPECT_TRUE(base::PathExists(
      DatabaseKeyManager::GetKeyFilePathForTesting(real_path)));
}

TEST_F(SqliteEncryptedVfsTest, FailsClosedOnSymlinkLoopWithoutCreatingKeyFile) {
  // A self-referential symlink causes `xFullPathname` to fail with
  // `SQLITE_CANTOPEN` (`ELOOP`). `NormalizeDatabasePath` must fail closed
  // rather than falling back to the un-normalized path and creating an
  // orphaned `-key` file.
  base::FilePath loop_path = temp_dir_.GetPath().AppendASCII("loop.sqlite");
  ASSERT_TRUE(base::CreateSymbolicLink(loop_path, loop_path));

  EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
  EXPECT_EQ(db.Open(loop_path), EncryptedDatabase::OpenResult::kPermanentError);
  EXPECT_FALSE(base::PathExists(KeyPath(loop_path)));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(SqliteEncryptedVfsTest,
       DeletePreservesKeyFileWhenDatabaseFileDeletionFails) {
  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
  ASSERT_TRUE(encrypted_db);
  ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
  ASSERT_TRUE(base::PathExists(db_path_));
  ASSERT_TRUE(base::PathExists(KeyPath(db_path_)));

  // Simulate `sql::Database::Delete()` failing to remove the main database
  // file. `EncryptedDatabase` hands SQLite the normalized path, so that is the
  // spelling the failing `xDelete` must match.
  bool delete_succeeded;
  {
    ScopedFailingVfsDelete failing_delete(NormalizedPath(db_path_));
    delete_succeeded = encrypted_db->Delete();
  }

  EXPECT_FALSE(delete_succeeded);
  ASSERT_TRUE(base::PathExists(db_path_));
  // Because the encrypted database file is still on disk, its `-key` file must
  // NOT have been deleted; otherwise the database is permanently unopenable.
  EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));
}

TEST_F(SqliteEncryptedVfsTest,
       DeleteAfterFailedDuplicateOpenDoesNotDeleteActiveDatabase) {
  std::unique_ptr<EncryptedDatabase> first = OpenDatabase(db_path_);
  ASSERT_TRUE(first);
  ASSERT_TRUE(first->DB().Execute("CREATE TABLE t (id INT)"));
  ASSERT_TRUE(first->DB().Execute("INSERT INTO t VALUES (1)"));

  EncryptedDatabase second(sql::DatabaseOptions(), sql::test::kTestTag);
  ASSERT_EQ(second.Open(db_path_),
            EncryptedDatabase::OpenResult::kPermanentError);

  // `second` never owned the open database, so calling `Delete()` on `second`
  // after its `Open()` failed must not wipe `first`'s live database and key
  // file. Nothing was deleted, so `Delete()` must not report success either.
  EXPECT_FALSE(second.Delete());
  EXPECT_TRUE(base::PathExists(db_path_));
  EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));

  // `first` keeps working, and the database survives a close / reopen cycle.
  // The reopen also catches a deleted `-key` file on platforms where the open
  // database file itself cannot be unlinked (e.g. Windows).
  EXPECT_TRUE(first->DB().Execute("INSERT INTO t VALUES (2)"));
  first.reset();
  std::unique_ptr<EncryptedDatabase> reopened = OpenDatabase(db_path_);
  ASSERT_TRUE(reopened);
  ASSERT_TRUE(reopened->DB().DoesTableExist("t"));
  sql::Statement count(
      reopened->DB().GetUniqueStatement("SELECT COUNT(*) FROM t"));
  ASSERT_TRUE(count.Step());
  EXPECT_EQ(count.ColumnInt(0), 2);
}

// Companion to `DeleteAfterFailedDuplicateOpenDoesNotDeleteActiveDatabase`:
// `Delete()` after a failed `Open()` is the documented way to recover from an
// unusable database (see `LoadOrCreateKey()`), so it must keep wiping the
// database and `-key` file when no other connection owns them.
TEST_F(SqliteEncryptedVfsTest, DeleteAfterPermanentOpenFailureWipesDatabase) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
  }
  ASSERT_TRUE(base::WriteFile(KeyPath(db_path_), "corrupted_key_bytes"));

  {
    EncryptedDatabase broken(sql::DatabaseOptions(), sql::test::kTestTag);
    ASSERT_EQ(broken.Open(db_path_),
              EncryptedDatabase::OpenResult::kKeyUnrecoverable);
    EXPECT_TRUE(broken.Delete());
  }
  EXPECT_FALSE(base::PathExists(db_path_));
  EXPECT_FALSE(base::PathExists(KeyPath(db_path_)));

  // The database can now be recreated from scratch.
  std::unique_ptr<EncryptedDatabase> recreated = OpenDatabase(db_path_);
  ASSERT_TRUE(recreated);
  EXPECT_FALSE(recreated->DB().DoesTableExist("t"));
}

TEST_F(SqliteEncryptedVfsTest, FailedDuplicateOpenDoesNotRewriteKeyFile) {
  // Use a known OS key so that a rotated keyring can still unseal the DEK.
  {
    os_crypt_async::Encryptor::KeyRing v1_keys;
    v1_keys.emplace("KEY_V1", MakeTestOsKey('1'));
    key_manager_ = DatabaseKeyManager::CreateForTesting(
        base::MakeRefCounted<CustomTestEncryptor>(std::move(v1_keys),
                                                  "KEY_V1"));
  }
  std::unique_ptr<EncryptedDatabase> first = OpenDatabase(db_path_);
  ASSERT_TRUE(first);
  ASSERT_TRUE(first->DB().Execute("CREATE TABLE t (id INT)"));

  std::optional<std::vector<uint8_t>> original_key_file =
      base::ReadFileToBytes(KeyPath(db_path_));
  ASSERT_TRUE(original_key_file.has_value());

  // Rotate the OS key while `first` is still open: `KEY_V1` is retained for
  // decryption and `KEY_V2` is now active, so loading the `-key` file would
  // re-seal it under `KEY_V2` and rewrite it on disk.
  {
    os_crypt_async::Encryptor::KeyRing rotated_keys;
    rotated_keys.emplace("KEY_V1", MakeTestOsKey('1'));
    rotated_keys.emplace("KEY_V2", MakeTestOsKey('2'));
    key_manager_ = DatabaseKeyManager::CreateForTesting(
        base::MakeRefCounted<CustomTestEncryptor>(std::move(rotated_keys),
                                                  "KEY_V2"));
  }

  // `first` owns the registered VFS for `db_path_`, so a duplicate
  // `Open(db_path_)` must be rejected before doing any `-key` file I/O on
  // behalf of a connection that will never exist.
  EncryptedDatabase second(sql::DatabaseOptions(), sql::test::kTestTag);
  EXPECT_EQ(second.Open(db_path_),
            EncryptedDatabase::OpenResult::kPermanentError);
  EXPECT_EQ(base::ReadFileToBytes(KeyPath(db_path_)), original_key_file);
}

TEST_F(SqliteEncryptedVfsTest,
       MissingKeyFileForExistingDatabaseIsPermanentError) {
  {
    std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
    ASSERT_TRUE(encrypted_db);
    ASSERT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
  }
  ASSERT_TRUE(base::PathExists(db_path_));
  ASSERT_TRUE(base::DeleteFile(KeyPath(db_path_)));

  // Opening an existing database whose `-key` file is missing must fail closed
  // rather than minting a brand new key.
  EncryptedDatabase reopened(sql::DatabaseOptions(), sql::test::kTestTag);
  EXPECT_EQ(reopened.Open(db_path_),
            EncryptedDatabase::OpenResult::kPermanentError);
  EXPECT_FALSE(base::PathExists(KeyPath(db_path_)));
}

// Companion to `MissingKeyFileForExistingDatabaseIsPermanentError`, pinning
// down its boundary: SQLite discards a leftover `-journal` or `-wal` when the
// main database file is missing or empty (a zero-page database has no hot
// journal), so there is no existing ciphertext to protect and creating the
// database with a fresh key must still succeed.
TEST_F(SqliteEncryptedVfsTest,
       StaleSiblingFilesWithoutDatabaseContentDoNotBlockCreation) {
  ASSERT_TRUE(
      base::WriteFile(sql::Database::JournalPath(db_path_), "stale journal"));
  ASSERT_TRUE(
      base::WriteFile(sql::Database::WriteAheadLogPath(db_path_), "stale wal"));
  ASSERT_TRUE(base::WriteFile(db_path_, ""));
  ASSERT_FALSE(base::PathExists(KeyPath(db_path_)));

  std::unique_ptr<EncryptedDatabase> encrypted_db = OpenDatabase(db_path_);
  ASSERT_TRUE(encrypted_db);
  EXPECT_TRUE(encrypted_db->DB().Execute("CREATE TABLE t (id INT)"));
  EXPECT_TRUE(base::PathExists(KeyPath(db_path_)));
  EXPECT_FALSE(base::PathExists(sql::Database::WriteAheadLogPath(db_path_)));
}

TEST_F(SqliteEncryptedVfsTest, OpenAfterDeleteIsRejected) {
  // The `EncryptedDatabase::Delete()` contract states that afterwards "the only
  // valid remaining operation is destruction", and `Open()` `CHECK`s that it is
  // not called more than once. However, `Delete()` resets `db_` to `nullptr`,
  // so a subsequent `Open()` bypasses `CHECK(!db_)`.
  EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
  ASSERT_EQ(db.Open(db_path_), EncryptedDatabase::OpenResult::kOk);
  ASSERT_TRUE(db.Delete());

  EXPECT_CHECK_DEATH(std::ignore = db.Open(db_path_));
}

TEST_F(SqliteEncryptedVfsTest, DeleteWithoutOpenDoesNotTouchCurrentDirectory) {
  // Before `Open()`, `EncryptedDatabase` has no database path. `Delete()` must
  // not derive `-journal` / `-wal` siblings from the empty path, which would
  // resolve relative to the current working directory and delete unrelated
  // files there.
  ScopedCurrentDirectory scoped_cwd(temp_dir_.GetPath());
  const base::FilePath bystander_journal =
      temp_dir_.GetPath().AppendASCII("-journal");
  const base::FilePath bystander_wal = temp_dir_.GetPath().AppendASCII("-wal");
  ASSERT_TRUE(base::WriteFile(bystander_journal, "unrelated"));
  ASSERT_TRUE(base::WriteFile(bystander_wal, "unrelated"));

  EncryptedDatabase db(sql::DatabaseOptions(), sql::test::kTestTag);
  db.Delete();

  EXPECT_TRUE(base::PathExists(bystander_journal));
  EXPECT_TRUE(base::PathExists(bystander_wal));
}

}  // namespace sqlite_encrypted_vfs
