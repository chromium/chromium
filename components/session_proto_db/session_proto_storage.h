// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_STORAGE_H_
#define COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_STORAGE_H_

#include <string>
#include <utility>
#include <vector>

#include "components/leveldb_proto/public/proto_database.h"

// General purpose per session (BrowserContext/BrowserState), per proto key ->
// proto database where the template is the proto which is being stored. A
// SessionProtoStorage should be acquired using SessionProtoDBFactory.
template <typename T>
class SessionProtoStorage {
 public:
  using KeyAndValue = std::pair<std::string, T>;

  // Callback which is used when content is acquired. Users are recommended to
  // check the bool value which indicates whether the operation has succeeded,
  // because when the operation fails, the callback could be posted to the
  // thread pool to execute instead of the original thread, which might lead to
  // use-after-free.
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete). This will be invoked on a different SequenceRunner
  // to SessionProtoDB.
  using OperationCallback = base::OnceCallback<void(bool)>;

  // Represents an entry in the database.
  using ContentEntry = typename leveldb_proto::ProtoDatabase<T>::KeyEntryVector;

  SessionProtoStorage() = default;
  SessionProtoStorage(const SessionProtoStorage&) = delete;
  SessionProtoStorage& operator=(const SessionProtoStorage&) = delete;
  virtual ~SessionProtoStorage() = default;

  // Loads the entry for the key and passes it to the callback.
  virtual void LoadOneEntry(const std::string& key, LoadCallback callback) = 0;

  // Loads all entries within the database and passes them to the callback.
  virtual void LoadAllEntries(LoadCallback callback) = 0;

  // Loads the content data matching a prefix for the key and passes them to the
  // callback.
  virtual void LoadContentWithPrefix(const std::string& key_prefix,
                                     LoadCallback callback) = 0;

  // Clean up data in the database which is no longer required by
  // 1) Matching all keys against a substring
  // 2) Deleting all keys matched against a susbstring, except for
  // the keys specified in keys_to_keep
  virtual void PerformMaintenance(const std::vector<std::string>& keys_to_keep,
                                  const std::string& key_substring_to_match,
                                  OperationCallback callback) = 0;

  // Inserts a value for a given key and passes the result (success/failure) to
  // OperationCallback.
  virtual void InsertContent(const std::string& key,
                             const T& value,
                             OperationCallback callback) = 0;

  // Deletes the entry with certain key in the database.
  virtual void DeleteOneEntry(const std::string& key,
                              OperationCallback callback) = 0;

  // Updates the value of multiple entries in the database.
  virtual void UpdateEntries(
      std::unique_ptr<std::vector<KeyAndValue>> entries_to_update,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      OperationCallback callback) = 0;

  // Deletes content in the database, matching all keys which have a prefix
  // that matches the key.
  virtual void DeleteContentWithPrefix(const std::string& key_prefix,
                                       OperationCallback callback) = 0;

  // Delete all content in the database.
  virtual void DeleteAllContent(OperationCallback callback) = 0;

  // Destroy the cached instance of the database (databases are cached per
  // session).
  virtual void Destroy() const = 0;
};

#endif  // COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_STORAGE_H_