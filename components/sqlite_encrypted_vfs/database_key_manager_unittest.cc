// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/database_key_manager.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sqlite_encrypted_vfs/database_metadata.pb.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

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

scoped_refptr<os_crypt_async::Encryptor> MakeEncryptor(
    const std::vector<std::string>& key_tags,
    const std::string& provider_for_encryption) {
  os_crypt_async::Encryptor::KeyRing keys;
  for (const std::string& tag : key_tags) {
    std::vector<uint8_t> raw_key(
        os_crypt_async::Encryptor::Key::kAES256GCMKeySize, tag.back());
    keys.emplace(tag,
                 os_crypt_async::Encryptor::Key(
                     raw_key, os_crypt_async::mojom::Algorithm::kAES256GCM));
  }
  return base::MakeRefCounted<CustomTestEncryptor>(std::move(keys),
                                                   provider_for_encryption);
}

}  // namespace

class DatabaseKeyManagerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("test.sqlite");
  }

  std::unique_ptr<DatabaseKeyManager> MakeManager(
      scoped_refptr<os_crypt_async::Encryptor> encryptor) {
    return DatabaseKeyManager::CreateForTesting(std::move(encryptor));
  }

  DatabaseKeyManager::KeyResult GetKey(DatabaseKeyManager& manager,
                                       const base::FilePath& db_path) {
    return manager.GetKeyForDatabase(db_path);
  }

  bool DeleteKey(DatabaseKeyManager& manager, const base::FilePath& db_path) {
    return manager.DeleteKeyForDatabase(db_path);
  }

  static base::FilePath KeyFilePath(const base::FilePath& db_path) {
    return DatabaseKeyManager::GetKeyFilePathForTesting(db_path);
  }

  base::FilePath key_path() const {
    return DatabaseKeyManager::GetKeyFilePathForTesting(db_path_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(DatabaseKeyManagerTest, KeyFileIsDatabaseSibling) {
  EXPECT_EQ(KeyFilePath(temp_dir_.GetPath().AppendASCII("History.db")),
            temp_dir_.GetPath().AppendASCII("History.db-key"));
}

TEST_F(DatabaseKeyManagerTest, CreatesAndPersistsKey) {
  base::HistogramTester histograms;
  EXPECT_FALSE(base::PathExists(key_path()));

  scoped_refptr<os_crypt_async::Encryptor> encryptor =
      os_crypt_async::GetTestEncryptorForTesting();

  std::optional<DatabaseKey> first_key;
  {
    std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor);
    DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
    ASSERT_TRUE(key.has_value());
    first_key = *key;
    EXPECT_TRUE(base::PathExists(key_path()));
  }
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kCreate),
                                true, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kCreate),
                              1);
  histograms.ExpectTotalCount("ImportantFile.WriteDuration.SqliteEncryptedVfs",
                              1);

  // A fresh manager with the same `Encryptor` must unseal the same DEK.
  {
    std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor);
    DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(*key, *first_key);
  }
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                true, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kLoad),
                              1);
}

TEST_F(DatabaseKeyManagerTest,
       ReusesPersistedKeyWithoutRetainingInMemoryCache) {
  base::HistogramTester histograms;
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());

  DatabaseKeyManager::KeyResult first = GetKey(*manager, db_path_);
  DatabaseKeyManager::KeyResult second = GetKey(*manager, db_path_);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  // The manager does not retain a map of loaded keys across completed loads,
  // so the second call reads from disk and unseals the same key.
  EXPECT_EQ(*first, *second);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kCreate),
                                true, 1);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                true, 1);
}

TEST_F(DatabaseKeyManagerTest, DifferentDatabasesGetDifferentKeys) {
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());

  DatabaseKeyManager::KeyResult first = GetKey(*manager, db_path_);
  DatabaseKeyManager::KeyResult second =
      GetKey(*manager, temp_dir_.GetPath().AppendASCII("other.sqlite"));
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(*first, *second);
}

TEST_F(DatabaseKeyManagerTest, ResealsKeyAfterOsKeyRotation) {
  base::HistogramTester histograms;
  // v1 can only decrypt data sealed with KEY_V1.
  scoped_refptr<os_crypt_async::Encryptor> encryptor_v1 =
      MakeEncryptor({"KEY_V1"}, "KEY_V1");

  std::optional<DatabaseKey> first_key;
  {
    std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor_v1);
    DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
    ASSERT_TRUE(key.has_value());
    first_key = *key;
  }

  std::optional<std::vector<uint8_t>> sealed_v1 =
      base::ReadFileToBytes(key_path());
  ASSERT_TRUE(sealed_v1.has_value());
  // `os_crypt_async::Encryptor::EncryptString()` prepends the active key
  // provider's tag (here `"KEY_V1"`) in plaintext before the random AES-256-GCM
  // nonce and ciphertext, so `StartsWith` verifies which key sealed the file
  // while ignoring the randomized ciphertext tail.
  EXPECT_TRUE(base::StartsWith(
      std::string(sealed_v1->begin(), sealed_v1->end()), "KEY_V1"));

  // v2 can still decrypt KEY_V1 data, but seals new data with KEY_V2, which
  // makes `DecryptFlags::should_reencrypt` `true`.
  scoped_refptr<os_crypt_async::Encryptor> encryptor_v2 =
      MakeEncryptor({"KEY_V1", "KEY_V2"}, "KEY_V2");
  {
    std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor_v2);
    DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
    ASSERT_TRUE(key.has_value());
    // The DEK itself, and therefore the database, is unchanged.
    EXPECT_EQ(*key, *first_key);
  }

  std::optional<std::vector<uint8_t>> sealed_v2 =
      base::ReadFileToBytes(key_path());
  ASSERT_TRUE(sealed_v2.has_value());
  // Verify the re-sealed file now starts with the `"KEY_V2"` provider tag.
  EXPECT_TRUE(base::StartsWith(
      std::string(sealed_v2->begin(), sealed_v2->end()), "KEY_V2"));
  EXPECT_NE(*sealed_v1, *sealed_v2);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                true, 1);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kReseal),
                                true, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kReseal),
                              1);
}

TEST_F(DatabaseKeyManagerTest, FailsClosedOnCorruptKeyFile) {
  base::HistogramTester histograms;
  ASSERT_TRUE(base::WriteFile(key_path(), "not a sealed key"));

  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kKeyUnrecoverable);
  // The `-key` file is left for the caller to delete with the database.
  EXPECT_TRUE(base::PathExists(key_path()));
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                false, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kLoad),
                              1);
}

TEST_F(DatabaseKeyManagerTest, ReportsKeyUnrecoverableWhenOsKeyIsLost) {
  base::HistogramTester histograms;
  {
    std::unique_ptr<DatabaseKeyManager> manager =
        MakeManager(MakeEncryptor({"KEY_V1"}, "KEY_V1"));
    ASSERT_TRUE(GetKey(*manager, db_path_).has_value());
  }

  // The OS-level key that sealed the `-key` file is gone (e.g. the OS keychain
  // entry was reset), and only an unrelated key is available.
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(MakeEncryptor({"KEY_V2"}, "KEY_V2"));
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kKeyUnrecoverable);
  EXPECT_TRUE(base::PathExists(key_path()));
}

TEST_F(DatabaseKeyManagerTest, ReportsPermanentErrorWhenKeyFileIsUnreadable) {
  base::HistogramTester histograms;
  // A directory at the `-key` path exists but cannot be read as a file. An I/O
  // failure does not prove the key is lost, so it must not be reported as
  // `kKeyUnrecoverable`.
  ASSERT_TRUE(base::CreateDirectory(key_path()));

  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kPermanentError);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                false, 1);
}

TEST_F(DatabaseKeyManagerTest,
       FailsWithTransientErrorWhenKeyTemporarilyUnavailable) {
  base::HistogramTester histograms;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_v1 =
      MakeEncryptor({"KEY_V1"}, "KEY_V1");
  {
    std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor_v1);
    ASSERT_TRUE(GetKey(*manager, db_path_).has_value());
  }

  // An `Encryptor` whose keyring maps `"KEY_V1"` to `std::nullopt` represents
  // a key provider that failed with `KeyError::kTemporarilyUnavailable`, which
  // causes `DecryptData()` to set `DecryptFlags::temporarily_unavailable`.
  os_crypt_async::Encryptor::KeyRing unavailable_keys;
  unavailable_keys.emplace("KEY_V1", std::nullopt);
  scoped_refptr<os_crypt_async::Encryptor> unavailable_encryptor =
      base::MakeRefCounted<CustomTestEncryptor>(std::move(unavailable_keys),
                                                /*provider_for_encryption=*/"");

  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(unavailable_encryptor);
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kTransientError);
  EXPECT_TRUE(base::PathExists(key_path()));
}

TEST_F(DatabaseKeyManagerTest, FailsClosedOnUnsupportedMetadataVersion) {
  base::HistogramTester histograms;
  scoped_refptr<os_crypt_async::Encryptor> encryptor =
      os_crypt_async::GetTestEncryptorForTesting();

  proto::EncryptedDatabaseMetadata metadata;
  metadata.set_version(proto::EncryptedDatabaseMetadata::VERSION_UNSPECIFIED);
  metadata.set_key(std::string(DatabaseKey::kSize, 'k'));
  std::string serialized;
  ASSERT_TRUE(metadata.SerializeToString(&serialized));
  std::optional<std::vector<uint8_t>> sealed =
      encryptor->EncryptString(serialized);
  ASSERT_TRUE(sealed.has_value());
  ASSERT_TRUE(base::WriteFile(key_path(), *sealed));

  std::unique_ptr<DatabaseKeyManager> manager = MakeManager(encryptor);
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kKeyUnrecoverable);
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kLoad),
                                false, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kLoad),
                              1);
}

TEST_F(DatabaseKeyManagerTest, FailsClosedWithoutUsableEncryptor) {
  base::HistogramTester histograms;
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorWithoutKeysForTesting());
  EXPECT_FALSE(GetKey(*manager, db_path_).has_value());
  EXPECT_FALSE(base::PathExists(key_path()));
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kCreate),
                                false, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kCreate),
                              1);
}

TEST_F(DatabaseKeyManagerTest, FailsClosedOnEmptyPath) {
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  EXPECT_FALSE(GetKey(*manager, base::FilePath()).has_value());
}

TEST_F(DatabaseKeyManagerTest, FailsClosedOnRelativeOrParentRefPath) {
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  EXPECT_FALSE(
      GetKey(*manager, base::FilePath(FILE_PATH_LITERAL("relative.sqlite")))
          .has_value());
  EXPECT_FALSE(GetKey(*manager, temp_dir_.GetPath()
                                    .Append(base::FilePath::kParentDirectory)
                                    .AppendASCII("test.sqlite"))
                   .has_value());
  EXPECT_FALSE(DeleteKey(*manager,
                         base::FilePath(FILE_PATH_LITERAL("relative.sqlite"))));
}

TEST_F(DatabaseKeyManagerTest, WhenReadyDefersUntilEncryptorIsReady) {
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/false);

  std::unique_ptr<DatabaseKeyManager> manager =
      DatabaseKeyManager::CreateForTesting(*os_crypt_async);

  bool first_ready = false;
  bool second_ready = false;
  base::RunLoop run_loop;
  DatabaseKeyManager::WhenReady(
      base::BindLambdaForTesting([&]() { first_ready = true; }));
  DatabaseKeyManager::WhenReady(base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(first_ready);
    second_ready = true;
    run_loop.Quit();
  }));
  // `os_crypt_async` is asynchronous, so callbacks must not run synchronously
  // before the `Encryptor` is vended.
  EXPECT_FALSE(first_ready);
  EXPECT_FALSE(second_ready);

  run_loop.Run();
  EXPECT_TRUE(first_ready);
  EXPECT_TRUE(second_ready);

  DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
  ASSERT_TRUE(key.has_value());
  EXPECT_TRUE(base::PathExists(key_path()));

  // Once the `Encryptor` is already ready, `WhenReady()` runs its callback
  // immediately on the current sequence.
  bool immediate_ready = false;
  DatabaseKeyManager::WhenReady(
      base::BindLambdaForTesting([&]() { immediate_ready = true; }));
  EXPECT_TRUE(immediate_ready);
}

TEST_F(DatabaseKeyManagerTest, DeleteKeyForDatabase) {
  base::HistogramTester histograms;
  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  ASSERT_TRUE(GetKey(*manager, db_path_).has_value());
  ASSERT_TRUE(base::PathExists(key_path()));

  EXPECT_TRUE(DeleteKey(*manager, db_path_));
  EXPECT_FALSE(base::PathExists(key_path()));
  histograms.ExpectUniqueSample(GetKeyActionHistogramName(KeyAction::kDelete),
                                true, 1);
  histograms.ExpectTotalCount(GetKeyActionTimeHistogramName(KeyAction::kDelete),
                              1);

  // The next request must mint a brand new key.
  DatabaseKeyManager::KeyResult key = GetKey(*manager, db_path_);
  ASSERT_TRUE(key.has_value());
  EXPECT_TRUE(base::PathExists(key_path()));
}

// If no key can be used to seal a new database key, creating the database
// fails closed with `kPermanentError` and no `-key` file is written.
TEST_F(DatabaseKeyManagerTest,
       FailsWithPermanentErrorOnNewDatabaseWhenNoKeyCanBecomeAvailable) {
  // No keys at all (every provider failed with `kPermanentlyUnavailable`).
  {
    std::unique_ptr<DatabaseKeyManager> manager =
        MakeManager(os_crypt_async::GetTestEncryptorWithoutKeysForTesting());
    DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kPermanentError);
    EXPECT_FALSE(base::PathExists(key_path()));
  }

  // A usable decrypt-only key and no provider for encryption. This `Encryptor`
  // reports `IsDecryptionAvailable() == true` and
  // `IsEncryptionAvailable() == false`, and failing to seal a new key with it
  // is still a permanent error.
  {
    os_crypt_async::Encryptor::KeyRing decrypt_only_keys;
    decrypt_only_keys.emplace(
        "KEY_V1",
        os_crypt_async::Encryptor::Key(
            std::vector<uint8_t>(
                os_crypt_async::Encryptor::Key::kAES256GCMKeySize, '1'),
            os_crypt_async::mojom::Algorithm::kAES256GCM));
    std::unique_ptr<DatabaseKeyManager> manager =
        MakeManager(base::MakeRefCounted<CustomTestEncryptor>(
            std::move(decrypt_only_keys), /*provider_for_encryption=*/""));
    DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kPermanentError);
    EXPECT_FALSE(base::PathExists(key_path()));
  }
}

TEST_F(DatabaseKeyManagerTest, FailsClosedWhenDatabaseExistsWithoutKeyFile) {
  // If a non-empty encrypted database file already exists on disk but its
  // sibling `-key` file is missing, `GetKeyForDatabase()` must fail closed with
  // `kPermanentError` rather than silently minting a new random key for the
  // existing database.
  ASSERT_TRUE(base::WriteFile(db_path_, "existing encrypted database bytes"));
  ASSERT_FALSE(base::PathExists(key_path()));

  std::unique_ptr<DatabaseKeyManager> manager =
      MakeManager(os_crypt_async::GetTestEncryptorForTesting());
  DatabaseKeyManager::KeyResult result = GetKey(*manager, db_path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DatabaseKeyManager::KeyError::kPermanentError);
  EXPECT_FALSE(base::PathExists(key_path()));
}

#if GTEST_HAS_DEATH_TEST
namespace {

// Calls `DatabaseKeyManager::Init()` twice and returns whether `WhenReady()`
// fires for the second `os_crypt_async::OSCryptAsync`. Leaks the process-wide
// singleton, so it must only run in an isolated child process.
bool SecondInitBecomesReady() {
  // The first `OSCryptAsync` is asynchronous and the task queue is never
  // pumped, so the first instance never becomes ready.
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_1 =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/false);
  DatabaseKeyManager::Init(*os_crypt_1);

  // The second `OSCryptAsync` vends its `Encryptor` synchronously, so if this
  // `Init()` takes effect, `WhenReady()` runs its callback immediately.
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_2 =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  DatabaseKeyManager::Init(*os_crypt_2);

  bool ready = false;
  DatabaseKeyManager::WhenReady(
      base::BindLambdaForTesting([&]() { ready = true; }));
  return ready;
}

}  // namespace

// A second `Init()` (e.g. a test or embedder re-creating `OSCryptAsync` within
// one process) must replace the active instance rather than being silently
// ignored by the function-local static in `Init()`.
//
// `Init()` creates a process-lifetime singleton that cannot be reset, so the
// scenario runs in a death-test child process (`threadsafe` style re-executes
// only this test), keeping the leaked singleton out of other tests in the same
// batch and guaranteeing no earlier test already called `Init()`.
TEST_F(DatabaseKeyManagerTest, SecondInitReplacesActiveInstance) {
  EXPECT_EXIT(std::_Exit(SecondInitBecomesReady() ? 0 : 1),
              testing::ExitedWithCode(0), "")
      << "WhenReady() did not fire after the second Init(): it was ignored.";
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace sqlite_encrypted_vfs
