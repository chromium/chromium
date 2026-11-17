// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/database_key_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sqlite_encrypted_vfs/database_metadata.pb.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs.h"
#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"

namespace sqlite_encrypted_vfs {

// Envelope Encryption & Key Loading Cases:
//
// Each encrypted SQLite database has a 32-byte data encrypting key (DEK,
// represented by `DatabaseKey`) that encrypts the database pages. Once created,
// a database's DEK never changes for the lifetime of that database.
//
// We use "Seal" / "Unseal" to refer to the envelope operation of wrapping or
// unwrapping the `DatabaseKey` inside a binary `EncryptedDatabaseMetadata`
// protobuf and encrypting or decrypting that protobuf with the OS-level key
// managed by `os_crypt_async::Encryptor`, stored in `<db_path>-key`:
//
// 1. Creation (no `-key` file on disk):
//    `DatabaseKey::Create()` mints a random 32-byte DEK, `DatabaseKey::Seal()`
//    serializes and encrypts it with `os_crypt_async::Encryptor`, and
//    `LoadOrCreateKey()` atomically writes `<db_path>-key`.
//
// 2. Steady-state load (`UnsealStatus::kOk`):
//    `DatabaseKey::Unseal()` decrypts `<db_path>-key` with the current OS key
//    (`DecryptFlags::should_reencrypt == false`) and returns the `DatabaseKey`.
//
// 3. OS key rotation (`UnsealStatus::kOkShouldReseal`):
//    When the OS-level key in `os_crypt_async::Encryptor` rotates, the
//    `Encryptor` retains older OS keys in its keyring for decryption while
//    marking the new OS key as active for encryption. `DatabaseKey::Unseal()`
//    decrypts `<db_path>-key` using the old OS key, recovers the exact same
//    `DatabaseKey` (DEK), and reports `UnsealStatus::kOkShouldReseal`.
//    `LoadOrCreateKey()` then re-seals that same DEK under the new OS key and
//    atomically overwrites `<db_path>-key`. Because the DEK itself is
//    unchanged, re-sealing never invalidates existing database pages or
//    requires rewriting the SQLite database.
//
// 4. Unseal failure (`UnsealStatus::kFailedTransient` /
//    `UnsealStatus::kFailedPermanent`):
//    If `<db_path>-key` cannot be decrypted (e.g. a temporarily unavailable OS
//    keyring, a permanently lost OS key, or a corrupted file) or fails
//    metadata validation, `LoadOrCreateKey()` returns a `KeyError`
//    (`kTransientError` or `kKeyUnrecoverable`) without deleting
//    `<db_path>-key` so `EncryptedDatabase::Open()` fails closed and the caller
//    can decide how to recover. Failing to read `<db_path>-key` at all is
//    reported as `kPermanentError`, since it does not prove the key is lost.

namespace {

// Sibling suffix for the file holding the sealed DEK. This mirrors SQLite's
// own "-journal" / "-wal" sibling naming.
constexpr base::FilePath::CharType kKeyFileSuffix[] = FILE_PATH_LITERAL("-key");

// Histogram suffix reported to `base::ImportantFileWriter` for key file writes.
constexpr std::string_view kImportantFileHistogramSuffix = "SqliteEncryptedVfs";

// Tracks the active `DatabaseKeyManager` instance (either the process singleton
// created via `DatabaseKeyManager::Init()` or a test-scoped instance).
DatabaseKeyManager* g_active_instance = nullptr;

// RAII helper that securely zeroes a sensitive byte container or
// `proto::EncryptedDatabaseMetadata` key field on scope exit so early returns
// cannot skip scrubbing.
template <typename T>
class ScopedZeroMemory {
 public:
  explicit ScopedZeroMemory(T& target) : target_(target) {}
  ScopedZeroMemory(const ScopedZeroMemory&) = delete;
  ScopedZeroMemory& operator=(const ScopedZeroMemory&) = delete;

  ~ScopedZeroMemory() {
    if constexpr (std::is_same_v<T, proto::EncryptedDatabaseMetadata>) {
      if (target_->has_key()) {
        crypto::SecureZeroBuffer(
            base::as_writable_byte_span(*target_->mutable_key()));
      }
    } else {
      crypto::SecureZeroBuffer(base::as_writable_byte_span(*target_));
    }
  }

 private:
  const base::raw_ref<T> target_;
};

// Returns the path of the sibling file holding the sealed key for `db_path`,
// i.e. `/path/to/db.sqlite` -> `/path/to/db.sqlite-key`.
base::FilePath GetKeyFilePath(const base::FilePath& db_path) {
  CHECK(IsValidDatabasePath(db_path));
  return base::FilePath(db_path.value() + kKeyFileSuffix);
}

// Validates and normalizes `db_path` and returns its sibling `-key` file path,
// or `std::nullopt` if `db_path` is invalid.
std::optional<base::FilePath> ResolveKeyFilePath(
    const base::FilePath& db_path) {
  base::FilePath normalized_path = NormalizeDatabasePath(db_path);
  if (normalized_path.empty()) {
    return std::nullopt;
  }
  return GetKeyFilePath(normalized_path);
}

// Reads the sealed key at `key_path`, or creates and persists a new one for the
// database at `db_path` (the normalized path `key_path` belongs to).
DatabaseKeyManager::KeyResult LoadOrCreateKey(
    const base::FilePath& db_path,
    const base::FilePath& key_path,
    const os_crypt_async::Encryptor& encryptor) {
  if (!base::PathExists(key_path)) {
    base::ElapsedTimer create_timer;
    // A non-empty database without its `-key` file holds content that no new
    // key could ever decrypt. Fail closed rather than minting a key for it.
    // An empty or missing database file has no content to protect (SQLite
    // discards stale `-journal` / `-wal` siblings in that case), so creation
    // proceeds.
    if (base::GetFileSize(db_path).value_or(0) > 0) {
      RecordKeyAction(KeyAction::kCreate, false, create_timer.Elapsed());
      return base::unexpected(DatabaseKeyManager::KeyError::kPermanentError);
    }
    DatabaseKey key = DatabaseKey::Create();
    std::optional<std::vector<uint8_t>> sealed = key.Seal(encryptor);
    const bool created =
        sealed.has_value() && base::ImportantFileWriter::WriteFileAtomically(
                                  key_path, base::as_string_view(*sealed),
                                  kImportantFileHistogramSuffix);
    RecordKeyAction(KeyAction::kCreate, created, create_timer.Elapsed());
    if (!created) {
      return base::unexpected(DatabaseKeyManager::KeyError::kPermanentError);
    }
    return key;
  }

  // If `Unseal()` fails, fail closed by returning a `KeyError` without
  // deleting `key_path`: `os_crypt_async::Encryptor::DecryptData()` failures
  // can be transient (e.g. when an OS key provider reports
  // `kTemporarilyUnavailable`), so deleting the `-key` file here would
  // permanently destroy the DEK. Even on permanent corruption, wiping and
  // recreating the database must be coordinated by the caller via
  // `EncryptedDatabase::Delete()` so the database files and `-key` file are
  // removed together rather than minting a new key for an existing encrypted
  // database.
  base::ElapsedTimer load_timer;
  std::optional<std::vector<uint8_t>> sealed = base::ReadFileToBytes(key_path);
  if (!sealed.has_value()) {
    // An I/O failure says nothing about whether the sealed key is still
    // recoverable, so report a generic permanent error rather than
    // `kKeyUnrecoverable`, which would invite the caller to delete the
    // database.
    RecordKeyAction(KeyAction::kLoad, false, load_timer.Elapsed());
    return base::unexpected(DatabaseKeyManager::KeyError::kPermanentError);
  }
  auto [status, key] = DatabaseKey::Unseal(encryptor, *sealed);
  RecordKeyAction(KeyAction::kLoad, key.has_value(), load_timer.Elapsed());
  if (!key.has_value()) {
    return base::unexpected(
        status == DatabaseKey::UnsealStatus::kFailedTransient
            ? DatabaseKeyManager::KeyError::kTransientError
            : DatabaseKeyManager::KeyError::kKeyUnrecoverable);
  }

  // The OS-level key rotated: re-seal the DEK in place. `DatabaseKey::Unseal()`
  // has already recovered the valid `DatabaseKey` in memory, so `key` is
  // returned and the database remains available for the current session
  // regardless of whether rewriting `key_path` succeeds. A write failure here
  // does not return an error because the existing `-key` file on disk is
  // still valid (sealed under the previous OS key retained in `encryptor`'s
  // keyring); the next load will see `should_reencrypt` again and retry
  // re-sealing.
  if (status == DatabaseKey::UnsealStatus::kOkShouldReseal) {
    base::ElapsedTimer reseal_timer;
    std::optional<std::vector<uint8_t>> resealed = key->Seal(encryptor);
    const bool resealed_on_disk =
        resealed.has_value() && base::ImportantFileWriter::WriteFileAtomically(
                                    key_path, base::as_string_view(*resealed),
                                    kImportantFileHistogramSuffix);
    RecordKeyAction(KeyAction::kReseal, resealed_on_disk,
                    reseal_timer.Elapsed());
  }

  return *key;
}

}  // namespace

// static
DatabaseKey DatabaseKey::Create() {
  KeyBytes bytes;
  ScopedZeroMemory zero_bytes(bytes);
  crypto::RandBytes(bytes);
  return DatabaseKey(bytes);
}

// static
std::pair<DatabaseKey::UnsealStatus, std::optional<DatabaseKey>>
DatabaseKey::Unseal(const os_crypt_async::Encryptor& encryptor,
                    base::span<const uint8_t> sealed_bytes) {
  if (sealed_bytes.empty()) {
    return {UnsealStatus::kFailedPermanent, std::nullopt};
  }
  os_crypt_async::Encryptor::DecryptFlags flags;
  std::optional<std::string> decrypted =
      encryptor.DecryptData(sealed_bytes, &flags);
  if (!decrypted.has_value()) {
    return {flags.temporarily_unavailable ? UnsealStatus::kFailedTransient
                                          : UnsealStatus::kFailedPermanent,
            std::nullopt};
  }
  ScopedZeroMemory zero_decrypted(*decrypted);

  // `ParseFromString()` parses Protobuf's binary wire format; `std::string` is
  // used as the byte container to match `Encryptor::DecryptData()`.
  proto::EncryptedDatabaseMetadata metadata;
  ScopedZeroMemory zero_metadata(metadata);
  if (!metadata.ParseFromString(*decrypted) ||
      metadata.version() != proto::EncryptedDatabaseMetadata::VERSION_1 ||
      !metadata.has_key() || metadata.key().size() != kSize) {
    return {UnsealStatus::kFailedPermanent, std::nullopt};
  }

  UnsealStatus status = flags.should_reencrypt ? UnsealStatus::kOkShouldReseal
                                               : UnsealStatus::kOk;
  return {status,
          DatabaseKey(base::as_byte_span(metadata.key()).first<kSize>())};
}

DatabaseKey::DatabaseKey(base::span<const uint8_t, kSize> bytes) {
  // `std::array` cannot be initialized from a `base::span` in the member
  // initializer list; copying directly into `bytes_` avoids an un-zeroed
  // temporary `KeyBytes` on the stack.
  std::ranges::copy(bytes, bytes_.begin());
}

DatabaseKey::~DatabaseKey() {
  crypto::SecureZeroBuffer(bytes_);
}

std::optional<std::vector<uint8_t>> DatabaseKey::Seal(
    const os_crypt_async::Encryptor& encryptor) const {
  proto::EncryptedDatabaseMetadata metadata;
  ScopedZeroMemory zero_metadata(metadata);
  metadata.set_version(proto::EncryptedDatabaseMetadata::VERSION_1);
  metadata.set_key(bytes_.data(), bytes_.size());

  // `SerializeToString()` writes Protobuf's binary wire format; `std::string`
  // is used as the byte container to match `Encryptor::EncryptString()`.
  std::string serialized;
  ScopedZeroMemory zero_serialized(serialized);
  if (!metadata.SerializeToString(&serialized)) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> sealed =
      encryptor.EncryptString(serialized);
  if (!sealed.has_value() || sealed->empty()) {
    return std::nullopt;
  }
  return sealed;
}

bool DatabaseKey::operator==(const DatabaseKey& other) const {
  return crypto::SecureMemEqual(bytes_, other.bytes_);
}

// static
void DatabaseKeyManager::Init(os_crypt_async::OSCryptAsync& os_crypt_async) {
  static base::NoDestructor<std::unique_ptr<DatabaseKeyManager>> instance;
  // A repeated `Init()` (e.g. an embedder or test re-creating `OSCryptAsync`
  // within one process) replaces the previous instance, which may still
  // reference a destroyed `OSCryptAsync`. Callbacks still waiting on the
  // previous instance are carried over so they fire once the new one is ready.
  std::vector<base::OnceClosure> pending_callbacks;
  if (*instance) {
    DCHECK_CALLED_ON_VALID_SEQUENCE((*instance)->sequence_checker_);
    pending_callbacks = std::move((*instance)->ready_callbacks_);
    instance->reset();
  }
  *instance = base::WrapUnique(new DatabaseKeyManager(os_crypt_async));
  for (base::OnceClosure& callback : pending_callbacks) {
    WhenReady(std::move(callback));
  }
}

// static
void DatabaseKeyManager::WhenReady(base::OnceClosure callback) {
  CHECK(g_active_instance) << "DatabaseKeyManager::WhenReady() called before "
                              "DatabaseKeyManager::Init().";
  DCHECK_CALLED_ON_VALID_SEQUENCE(g_active_instance->sequence_checker_);
  CHECK(callback);

  if (g_active_instance->GetEncryptor()) {
    std::move(callback).Run();
    return;
  }
  g_active_instance->ready_callbacks_.push_back(std::move(callback));
}

// static
DatabaseKeyManager& DatabaseKeyManager::Get(base::PassKey<EncryptedDatabase>) {
  CHECK(g_active_instance)
      << "DatabaseKeyManager::Get() called before DatabaseKeyManager::Init().";
  return *g_active_instance;
}

DatabaseKeyManager::DatabaseKeyManager(
    os_crypt_async::OSCryptAsync& os_crypt_async)
    : previous_active_instance_(std::exchange(g_active_instance, this)) {
  if (previous_active_instance_) {
    previous_active_instance_->next_active_instance_ = this;
  }
  // May run the callback synchronously if the `os_crypt_async::Encryptor` is
  // already available.
  os_crypt_async.GetInstance(base::BindOnce(
      &DatabaseKeyManager::OnEncryptorReady, weak_factory_.GetWeakPtr()));
}

DatabaseKeyManager::DatabaseKeyManager(
    scoped_refptr<os_crypt_async::Encryptor> encryptor)
    : previous_active_instance_(std::exchange(g_active_instance, this)),
      encryptor_(std::move(encryptor)) {
  CHECK(encryptor_);
  if (previous_active_instance_) {
    previous_active_instance_->next_active_instance_ = this;
  }
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DatabaseKeyManager::~DatabaseKeyManager() {
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  if (previous_active_instance_) {
    previous_active_instance_->next_active_instance_ = next_active_instance_;
  }
  if (next_active_instance_) {
    next_active_instance_->previous_active_instance_ =
        previous_active_instance_;
  }
  if (g_active_instance == this) {
    g_active_instance = previous_active_instance_;
  }
  previous_active_instance_ = nullptr;
  next_active_instance_ = nullptr;
}

// static
std::unique_ptr<DatabaseKeyManager> DatabaseKeyManager::CreateForTesting(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  return base::WrapUnique(new DatabaseKeyManager(std::move(encryptor)));
}

// static
std::unique_ptr<DatabaseKeyManager> DatabaseKeyManager::CreateForTesting(
    os_crypt_async::OSCryptAsync& os_crypt_async) {
  return base::WrapUnique(new DatabaseKeyManager(os_crypt_async));
}

// static
base::FilePath DatabaseKeyManager::GetKeyFilePathForTesting(
    const base::FilePath& db_path) {
  return GetKeyFilePath(db_path);
}

DatabaseKeyManager::KeyResult DatabaseKeyManager::GetKeyForDatabase(
    const base::FilePath& db_path) {
  scoped_refptr<os_crypt_async::Encryptor> encryptor = GetEncryptor();
  CHECK(encryptor)
      << "GetKeyForDatabase() called before DatabaseKeyManager is ready.";

  base::FilePath normalized_path = NormalizeDatabasePath(db_path);
  if (normalized_path.empty()) {
    return base::unexpected(KeyError::kPermanentError);
  }
  return LoadOrCreateKey(normalized_path, GetKeyFilePath(normalized_path),
                         *encryptor);
}

bool DatabaseKeyManager::DeleteKeyForDatabase(const base::FilePath& db_path) {
  std::optional<base::FilePath> key_path = ResolveKeyFilePath(db_path);
  if (!key_path.has_value()) {
    return false;
  }
  base::ElapsedTimer delete_timer;
  bool success = base::DeleteFile(*key_path);
  RecordKeyAction(KeyAction::kDelete, success, delete_timer.Elapsed());
  return success;
}

void DatabaseKeyManager::OnEncryptorReady(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encryptor);

  {
    base::AutoLock auto_lock(lock_);
    encryptor_ = std::move(encryptor);
  }

  for (base::OnceClosure& callback : std::exchange(ready_callbacks_, {})) {
    std::move(callback).Run();
  }
}

scoped_refptr<os_crypt_async::Encryptor> DatabaseKeyManager::GetEncryptor()
    const {
  base::AutoLock auto_lock(lock_);
  return encryptor_;
}

}  // namespace sqlite_encrypted_vfs
