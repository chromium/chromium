# SQLite Encrypted VFS

`sqlite_encrypted_vfs` provides a custom SQLite Virtual File System (VFS)
wrapper for Chromium that integrates with `os_crypt_async` to provide envelope
encryption and key lifecycle management for [`sql::Database`](/sql/database.h).

## Exported Public API

1. **One-Time Browser Process Initialization (`DatabaseKeyManager::Init`)**:
   Because `//components/sqlite_encrypted_vfs` cannot depend on `//chrome`'s
   `g_browser_process->os_crypt_async()` directly and
   `os_crypt_async::OSCryptAsync::GetInstance()` must be called on the UI
   sequence where `OSCryptAsync` lives, the embedder initializes
   `DatabaseKeyManager` once at browser-process startup on the UI sequence:
   ```cpp
   sqlite_encrypted_vfs::DatabaseKeyManager::Init(os_crypt_async);
   ```
   `DatabaseKeyManager` acquires the thread-safe `os_crypt_async::Encryptor`
   asynchronously once for the process.

2. **Deferring Consumer Database Sequences (`DatabaseKeyManager::WhenReady`)**:
   Database consumers whose backend runs on a `base::MayBlock()` worker
   sequence wrap that sequence in a `base::DeferredSequencedTaskRunner` on the
   UI sequence and start it via `DatabaseKeyManager::WhenReady()` (matching
   `WebDatabaseService::LoadDatabase` in `/components/webdata/common/`):
   ```cpp
   auto db_task_runner =
       base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
           base::ThreadPool::CreateSequencedTaskRunner(
               {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
   sqlite_encrypted_vfs::DatabaseKeyManager::WhenReady(base::BindOnce(
       &base::DeferredSequencedTaskRunner::Start, db_task_runner));
   ```
   Tasks posted to `db_task_runner` stay queued in order until the `Encryptor`
   is ready, so backend code on the database sequence never blocks on or waits
   for key readiness.

3. **Per-Database Consumer Usage (`EncryptedDatabase`)**:
   On the `base::MayBlock()` database sequence, feature storage backends
   interact with `EncryptedDatabase` synchronously, just like `sql::Database`.
   Store the `EncryptedDatabase` as a member so that it outlives all operations
   on `DB()`:
   ```cpp
   // Member declaration (on the feature's MayBlock() backend sequence):
   // std::unique_ptr<sqlite_encrypted_vfs::EncryptedDatabase> encrypted_db_;

   encrypted_db_ = std::make_unique<sqlite_encrypted_vfs::EncryptedDatabase>(
       sql::DatabaseOptions(), sql::Database::Tag("MyFeature"));
   sqlite_encrypted_vfs::EncryptedDatabase::OpenResult result =
       encrypted_db_->Open(db_path);
   if (result != sqlite_encrypted_vfs::EncryptedDatabase::OpenResult::kOk) {
     // Fail closed: there is no plaintext fallback.
     // - `kTransientError`: do not delete the database, as a future attempt
     //   may succeed.
     // - `kKeyUnrecoverable`: the existing contents can never be decrypted
     //   (e.g. the OS-level key was lost or reset). The feature may call
     //   `encrypted_db_->Delete()` and start over with a fresh
     //   `EncryptedDatabase`.
     // - `kPermanentError`: do not delete the database, as the failure (e.g.
     //   I/O) does not imply its contents are unreadable.
     encrypted_db_.reset();
     return;
   }
   sql::Database& sql_db = encrypted_db_->DB();
   // Use `sql_db` on the sequence that called Open()...
   ```
   To remove an encrypted database and its sibling key file:
   ```cpp
   bool deleted = encrypted_db_->Delete();
   ```

## Internal Ownership & Data Flow

* **`DatabaseKeyManager` & `DatabaseKey`**:
  Manages envelope encryption for per-database 32-byte keys (`DatabaseKey`). For
  a database at `/path/to/db.sqlite`, the key is wrapped in a versioned
  `EncryptedDatabaseMetadata` protobuf, sealed with `os_crypt_async::Encryptor`,
  and stored atomically in `/path/to/db.sqlite-key` on the caller's
  `base::MayBlock()` sequence. On OS key rotation, only the sibling `-key` file
  is re-sealed in place. Loaded keys are not cached in the manager across calls;
  `DatabaseKeyManager::Get()` is restricted to `EncryptedDatabase` via
  `base::PassKey<EncryptedDatabase>`.
* **`SqliteEncryptedVfs`**:
  Each open `EncryptedDatabase` owns a dedicated `SqliteEncryptedVfs` instance
  bound to that database's normalized path and `DatabaseKey`. The instance
  registers a uniquely named `sqlite3_vfs` (`sqlite_encrypted_vfs_<path_hash>`)
  with `pAppData` pointing back to itself, disables memory-mapped I/O (`xFetch`
  returns a null buffer so reads never bypass `xRead`), and `CHECK`s in `xOpen`
  that no other main database file is opened through it.
* **Teardown Ordering**:
  `EncryptedDatabase` owns both the `SqliteEncryptedVfs` and the underlying
  `sql::Database`, and guarantees that `sql::Database` is closed and destroyed
  before `SqliteEncryptedVfs` unregisters from SQLite.
