// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/value_store/value_store_change.h"

namespace value_store {

// Interface for a storage area for Value objects.
class ValueStore {
 public:
  // Status codes returned from storage methods.
  enum StatusCode {
    OK,

    // The failure was due to some kind of database corruption. Depending on
    // what is corrupted, some part of the database may be recoverable.
    //
    // For example, if the on-disk representation of leveldb is corrupted, it's
    // likely the whole database will need to be wiped and started again.
    //
    // If a single key has been committed with an invalid JSON representation,
    // just that key can be deleted without affecting the rest of the database.
    CORRUPTION,

    // The failure was due to the store being read-only (for example, policy).
    READ_ONLY,

    // The failure was due to the store running out of space.
    QUOTA_EXCEEDED,

    // Any other error.
    OTHER_ERROR,
  };

  enum BackingStoreRestoreStatus {
    // No restore attempted.
    RESTORE_NONE,
    // Corrupted backing store successfully deleted.
    DB_RESTORE_DELETE_SUCCESS,
    // Corrupted backing store cannot be deleted.
    DB_RESTORE_DELETE_FAILURE,
    // Corrupted backing store successfully repaired.
    DB_RESTORE_REPAIR_SUCCESS,
    // Corrupted value successfully deleted.
    VALUE_RESTORE_DELETE_SUCCESS,
    // Corrupted value cannot be deleted.
    VALUE_RESTORE_DELETE_FAILURE,
  };

  // The status (result) of an operation on a ValueStore.
  struct Status {
    Status();
    Status(StatusCode code,
           BackingStoreRestoreStatus restore_status,
           const std::string& message);
    Status(StatusCode code, const std::string& message);
    Status(Status&& other);
    ~Status();
    Status& operator=(Status&& rhs);

    bool ok() const { return code == OK; }

    bool IsCorrupted() const { return code == CORRUPTION; }

    // Merge |status| into this object. Any members (either |code|,
    // |restore_status|, or |message| in |status| will be used, but only if this
    // object's members are at their default value.
    void Merge(const Status& status);

    // The status code.
    StatusCode code = OK;

    BackingStoreRestoreStatus restore_status = RESTORE_NONE;

    // Message associated with the status (error) if there is one.
    std::string message;
  };

  // The result of a read operation (Get).
  class ReadResult {
   public:
    ReadResult(base::Value::Dict settings, Status status);
    explicit ReadResult(Status status);
    ReadResult(ReadResult&& other);
    ~ReadResult();
    ReadResult& operator=(ReadResult&& rhs);
    ReadResult(const ReadResult&) = delete;
    ReadResult& operator=(const ReadResult&) = delete;

    // Gets the settings read from the storage. Note that this represents
    // the root object. If you request the value for key "foo", that value will
    // be in |settings|.|foo|.
    //
    // Must only be called if there is no error.
    base::Value::Dict& settings() { return settings_; }
    base::Value::Dict PassSettings() { return std::move(settings_); }
    Status PassStatus() { return std::move(status_); }

    const Status& status() const { return status_; }

   private:
    base::Value::Dict settings_;
    Status status_;
  };

  // The result of a write operation (Set/Remove/Clear).
  class WriteResult {
   public:
    WriteResult(ValueStoreChangeList changes, Status status);
    explicit WriteResult(Status status);
    WriteResult(WriteResult&& other);
    ~WriteResult();
    WriteResult& operator=(WriteResult&& rhs);
    WriteResult(const WriteResult&) = delete;
    WriteResult& operator=(const WriteResult&) = delete;

    // Gets the list of changes to the settings which resulted from the write.
    // Won't be present if the NO_GENERATE_CHANGES WriteOptions was given.
    // Only call if no error.
    const ValueStoreChangeList& changes() const { return changes_; }
    ValueStoreChangeList PassChanges() { return std::move(changes_); }

    const Status& status() const { return status_; }

   private:
    ValueStoreChangeList changes_;
    Status status_;
  };

  // Options for write operations.
  enum WriteOptionsValues {
    // Callers should usually use this.
    DEFAULTS = 0,

    // Ignore any quota restrictions.
    IGNORE_QUOTA = 1 << 1,

    // Don't generate the changes for a WriteResult.
    NO_GENERATE_CHANGES = 1 << 2,
  };
  typedef int WriteOptions;

  virtual ~ValueStore() = default;

  // Gets the amount of space being used by a single value, in bytes.
  // Note: The GetBytesInUse methods are only used by extension settings at the
  // moment. If these become more generally useful, the
  // SettingsStorageQuotaEnforcer and WeakUnlimitedSettingsStorage classes
  // should be moved to the value_store directory.
  virtual size_t GetBytesInUse(const std::string& key) = 0;

  // Gets the total amount of space being used by multiple values, in bytes.
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) = 0;

  // Gets the total amount of space being used by this storage area, in bytes.
  virtual size_t GetBytesInUse() = 0;

  // Gets a single value from storage.
  virtual ReadResult Get(const std::string& key) = 0;

  // Gets multiple values from storage.
  virtual ReadResult Get(const std::vector<std::string>& keys) = 0;

  // Gets all values from storage.
  virtual ReadResult Get() = 0;

  // Sets a single key to a new value.
  virtual WriteResult Set(WriteOptions options,
                          const std::string& key,
                          const base::Value& value) = 0;

  // Sets multiple keys to new values.
  virtual WriteResult Set(WriteOptions options,
                          const base::Value::Dict& values) = 0;

  // Removes a key from the storage.
  virtual WriteResult Remove(const std::string& key) = 0;

  // Removes multiple keys from the storage.
  virtual WriteResult Remove(const std::vector<std::string>& keys) = 0;

  // Clears the storage.
  virtual WriteResult Clear() = 0;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_H_
