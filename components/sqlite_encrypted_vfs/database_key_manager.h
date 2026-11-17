// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_ENCRYPTED_VFS_DATABASE_KEY_MANAGER_H_
#define COMPONENTS_SQLITE_ENCRYPTED_VFS_DATABASE_KEY_MANAGER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace sqlite_encrypted_vfs {

class EncryptedDatabase;

// The 32-byte (256-bit) data encrypting key (DEK) that protects a single
// SQLite database (sized for AES-256 page encryption and HMAC-SHA256 IV
// derivation). Instances are immutable value types and safe to copy across
// sequences.
//
// This class is internal to the component: consumers never obtain one directly.
// `EncryptedDatabase` acquires it through `DatabaseKeyManager` and hands it to
// the `SqliteEncryptedVfs` instance serving that database.
class DatabaseKey {
 public:
  static constexpr size_t kSize = 32;

  enum class UnsealStatus {
    kOk,
    kOkShouldReseal,
    kFailedTransient,
    kFailedPermanent,
  };

  // Creates a new random 32-byte database key.
  static DatabaseKey Create();

  // Decrypts `sealed_bytes` with `encryptor` and parses the versioned
  // `EncryptedDatabaseMetadata` payload to reconstruct a `DatabaseKey`. Returns
  // the unseal status (including whether OS key rotation requires re-sealing)
  // alongside the key on success.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static std::pair<UnsealStatus, std::optional<DatabaseKey>> Unseal(
      const os_crypt_async::Encryptor& encryptor,
      base::span<const uint8_t> sealed_bytes);

  DatabaseKey(const DatabaseKey&) = default;
  DatabaseKey& operator=(const DatabaseKey&) = default;

  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS) ~DatabaseKey();

  // Serializes this key into a versioned `EncryptedDatabaseMetadata` payload
  // and encrypts it with `encryptor` for on-disk storage in the `-key` file.
  // Returns `std::nullopt` if serialization or encryption fails.
  std::optional<std::vector<uint8_t>> Seal(
      const os_crypt_async::Encryptor& encryptor) const;

  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  bool operator==(const DatabaseKey& other) const;

 private:
  using KeyBytes = std::array<uint8_t, kSize>;

  explicit DatabaseKey(base::span<const uint8_t, kSize> bytes);

  KeyBytes bytes_;
};

// Owns the envelope encryption scheme used by the encrypted SQLite VFS.
//
// For a database at `/path/to/db.sqlite`, the DEK is wrapped in a versioned
// metadata protobuf, sealed with `os_crypt_async::Encryptor`, and stored in a
// sibling file at `/path/to/db.sqlite-key`. If the OS-level key has rotated,
// only the (tiny) `-key` file is re-sealed in place with the new OS key; the
// DEK itself does not change, so the database is never rewritten and all data
// encrypted before the rotation remains available.
//
// Embedders initialize the process-wide instance alongside
// `os_crypt_async::OSCryptAsync` at browser-process startup via
// `DatabaseKeyManager::Init()`. Consumer services on the UI sequence can use
// `DatabaseKeyManager::WhenReady()` to start their
// `base::DeferredSequencedTaskRunner` once the `os_crypt_async::Encryptor` has
// been acquired. `EncryptedDatabase` then retrieves the manager via `Get()`
// using its `base::PassKey` and loads or deletes keys synchronously on the
// database's `base::MayBlock()` worker sequence. Loaded keys are not retained
// in memory by this manager: each open `SqliteEncryptedVfs` instance holds its
// own key for the lifetime of the connection, while the manager only holds the
// thread-safe `os_crypt_async::Encryptor`.
//
// TODO(crbug.com/476343961): No production code initializes the singleton yet -
// this CL only establishes the class and its tests. Wiring `Init()` up at
// browser-process startup, next to `os_crypt_async::OSCryptAsync`, belongs to
// the consumer integration CL along with the feature flag. If/when a second
// consumer is onboarded, revisit whether to keep the process-wide `Init()`
// singleton or refactor to an explicitly owned instance/factory.
//
// Threading: `Init()`, `WhenReady()`, and destruction in tests must happen on
// the sequence where `os_crypt_async::OSCryptAsync` lives (the UI thread in the
// browser process). Once `WhenReady()` has signaled readiness,
// `GetKeyForDatabase()` and `DeleteKeyForDatabase()` perform synchronous
// blocking file I/O and may be called from any `base::MayBlock()` sequence
// (such as `EncryptedDatabase`'s background worker sequence).
class DatabaseKeyManager {
 public:
  enum class KeyError {
    // Key decryption failed because the OS key provider is temporarily
    // unavailable
    // (`os_crypt_async::Encryptor::DecryptFlags::temporarily_unavailable`).
    // Callers must fail closed without deleting the database or `-key` file, as
    // a subsequent attempt may succeed.
    kTransientError,
    // The `-key` file was read but its sealed key cannot be recovered with the
    // current `os_crypt_async::Encryptor`: the OS-level key that sealed it is
    // permanently gone (e.g. the OS key changed or was reset), or the `-key`
    // file contents are corrupted or fail metadata validation. The database
    // contents are unreadable, so callers may recover by deleting the database
    // and its `-key` file together via `EncryptedDatabase::Delete()` and
    // starting over with a fresh database.
    kKeyUnrecoverable,
    // The key could not be loaded or created for any other non-transient
    // reason (e.g. I/O failure reading or writing the `-key` file, or an
    // invalid path). This does not imply the existing database is unreadable,
    // so callers must not delete it in response.
    kPermanentError,
  };

  using KeyResult = base::expected<DatabaseKey, KeyError>;

  // Initializes the process-wide `DatabaseKeyManager` singleton. Must be called
  // at browser-process startup on the UI sequence before
  // `WhenReady()` or `EncryptedDatabase` is used. Acquires an
  // `os_crypt_async::Encryptor` from `os_crypt_async` asynchronously. Calling
  // it again replaces the previous singleton (pending `WhenReady()` callbacks
  // are carried over), so the active instance never refers to a stale
  // `os_crypt_async`.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static void Init(os_crypt_async::OSCryptAsync& os_crypt_async);

  // Invokes `callback` on the calling (UI) sequence once the
  // `os_crypt_async::Encryptor` has been acquired. If the `Encryptor` is
  // already available, `callback` is run synchronously before `WhenReady()`
  // returns.
  //
  // Consumer services that post database tasks from the UI sequence to a
  // background database sequence should wrap that sequence in a
  // `base::DeferredSequencedTaskRunner` and pass its `Start()` closure to
  // `WhenReady()`, ensuring `EncryptedDatabase::Open()` only runs after the
  // `Encryptor` is ready while preserving FIFO task ordering.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static void WhenReady(base::OnceClosure callback);

  // Returns the process-wide `DatabaseKeyManager` created by `Init()`. Does not
  // lazily initialize the singleton: `Init()` must have been called first
  // (enforced by a `CHECK`). Restricted to `EncryptedDatabase` and not exported
  // outside the component.
  static DatabaseKeyManager& Get(base::PassKey<EncryptedDatabase>);

  DatabaseKeyManager(const DatabaseKeyManager&) = delete;
  DatabaseKeyManager& operator=(const DatabaseKeyManager&) = delete;

  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS) ~DatabaseKeyManager();

  // Synchronously reads (or creates and persists) the key for the database at
  // `db_path`, which must be an absolute path without parent (`..`) references.
  // Must be called on a `base::MayBlock()` sequence after `WhenReady()` has
  // signaled readiness (`CHECK`s that the `os_crypt_async::Encryptor` is
  // available). Callers must fail closed on error: there is no plaintext
  // fallback.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  KeyResult GetKeyForDatabase(const base::FilePath& db_path);

  // Synchronously deletes the on-disk `-key` file for `db_path` (which must be
  // an absolute path without parent references) and returns whether the
  // deletion succeeded. Must be called on a `base::MayBlock()` sequence. Called
  // by `EncryptedDatabase::Delete()` after the database files have been
  // removed.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  bool DeleteKeyForDatabase(const base::FilePath& db_path);

  // Test-only entry points exported for unit tests across the component
  // shared-library boundary.
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static std::unique_ptr<DatabaseKeyManager> CreateForTesting(
      scoped_refptr<os_crypt_async::Encryptor> encryptor);
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static std::unique_ptr<DatabaseKeyManager> CreateForTesting(
      os_crypt_async::OSCryptAsync& os_crypt_async);
  COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
  static base::FilePath GetKeyFilePathForTesting(const base::FilePath& db_path);

 private:
  explicit DatabaseKeyManager(os_crypt_async::OSCryptAsync& os_crypt_async);
  explicit DatabaseKeyManager(
      scoped_refptr<os_crypt_async::Encryptor> encryptor);

  void OnEncryptorReady(scoped_refptr<os_crypt_async::Encryptor> encryptor);
  scoped_refptr<os_crypt_async::Encryptor> GetEncryptor() const;

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<DatabaseKeyManager> previous_active_instance_ = nullptr;
  raw_ptr<DatabaseKeyManager> next_active_instance_ = nullptr;

  mutable base::Lock lock_;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_ GUARDED_BY(lock_);

  // Callbacks waiting on `OnEncryptorReady()`, accessed only on the UI sequence
  // where `this` was constructed.
  std::vector<base::OnceClosure> ready_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DatabaseKeyManager> weak_factory_{this};
};

}  // namespace sqlite_encrypted_vfs

#endif  // COMPONENTS_SQLITE_ENCRYPTED_VFS_DATABASE_KEY_MANAGER_H_
