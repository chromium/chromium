// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace leveldb_proto {

class ProtoLevelDBWrapper;

class Enums {
 public:
  enum InitStatus {
    // Failed to migrate from shared to unique or unique to shared, or failed to
    // create a non-existent database.
    kError = -1,
    // Internal state, never returned to clients. TODO: This should be removed.
    kNotInitialized = 0,
    // Leveldb initialization successful.
    kOK = 1,
    // In case of unique database, this status is never returned. The legacy
    // behavior is to delete the database on corruption and return a clean one
    // (if possible). In case of shared database, the default behavior is to
    // delete on corruption and not return this flag. In the future we will have
    // a delete_on_corruption flag that clients can set to false and handle
    // corruption with partial data. TODO(salg): Expose delete_on_corruption
    // flag.
    kCorrupt = 2,
    // Invalid arguments were passed (like database doesn't exist and
    // create_if_missing was false), or the current platform does not support
    // leveldb.
    kInvalidOperation = 3,
  };
};

class Callbacks {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  using InitStatusCallback = base::OnceCallback<void(Enums::InitStatus)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;
  using LoadKeysCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::vector<std::string>>)>;
  using DestroyCallback = base::OnceCallback<void(bool)>;
  using OnCreateCallback = base::OnceCallback<void(ProtoLevelDBWrapper*)>;

  // TODO(ssid): This should be moved to internal folder.
  using LoadCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::vector<std::string>>)>;
  using GetCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::string>)>;
  using LoadKeysAndEntriesCallback = base::OnceCallback<
      void(bool, std::unique_ptr<std::map<std::string, std::string>>)>;

  template <typename T>
  class Internal {
   public:
    using LoadCallback =
        base::OnceCallback<void(bool, std::unique_ptr<std::vector<T>>)>;
    using GetCallback = base::OnceCallback<void(bool, std::unique_ptr<T>)>;
    using LoadKeysAndEntriesCallback =
        base::OnceCallback<void(bool,
                                std::unique_ptr<std::map<std::string, T>>)>;
  };
};

class Util {
 public:
  template <typename T>
  class Internal {
   public:
    // A list of key-value (string, T) tuples.
    using KeyEntryVector = std::vector<std::pair<std::string, T>>;
  };
};

using KeyFilter = base::RepeatingCallback<bool(const std::string& key)>;

// Interface for classes providing persistent storage of Protocol Buffer
// entries. P must be a proto type extending MessageLite. T is optional and
// defaults to P and there is then no additional requirements for clients.
// If T is set to something else, the client must provide these functions
// (note the namespace requirement):
// namespace leveldb_proto {
// void DataToProto(const T& data, P* proto);
// void ProtoToData(const P& proto, T* data);
// }  // namespace leveldb_proto
// The P type will be stored in the database, and the T type will be required
// as input and will be provided as output for all API calls. The backend will
// invoke the methods above for all conversions between the two types.
// For retrieving a database of proto type ClientProto, use:
// auto db = ProtoDatabaseProviderFactory::GetForBrowserContext(...)
//               -> GetDB<ClientProto>(...);
// For automatically converting to a different data type, use:
// auto db = ProtoDatabaseProviderFactory::GetForBrowserContext(...)
//               -> GetDB<ClientProto, ClientStruct>(...);
template <typename P, typename T = P>
class ProtoDatabase {
 public:
  // For compatibility:
  using KeyEntryVector = typename Util::Internal<T>::KeyEntryVector;

  virtual ~ProtoDatabase() = default;

  // Asynchronously initializes the object, which must have been created by the
  // ProtoDatabaseProvider::GetDB<T> function. |callback| will be invoked on the
  // calling thread when complete.
  //
  // DEPRECATED: |unique_db_options| is used only when a unique DB is loaded,
  // once migration to shared DB is done, this parameter will be ignored.
  virtual void Init(Callbacks::InitStatusCallback callback) = 0;
  virtual void Init(const leveldb_env::Options& unique_db_options,
                    Callbacks::InitStatusCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete.
  virtual void UpdateEntries(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      Callbacks::UpdateCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries that satisfies
  // the |delete_key_filter| from the database. |callback| will be invoked on
  // the calling thread when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const leveldb_proto::KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;

  // Asynchronously loads entries that satisfies the |filter| from the database
  // and invokes |callback| when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void LoadEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;
  virtual void LoadEntriesWithFilter(
      const leveldb_proto::KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;

  virtual void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;

  virtual void LoadKeysAndEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;
  virtual void LoadKeysAndEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;
  // Asynchronously loads entries and their keys for keys in range [start, end]
  // (both inclusive) and invokes |callback| when complete.
  // Range is defined as |start| <= returned keys <= |end|.
  // When |start| = 'bar' and |end| = 'foo' then the keys within brackets are
  // returned: baa, [bar, bara, barb, foa, foo], fooa, fooz, fop.
  virtual void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;

  // Asynchronously loads all keys from the database and invokes |callback| with
  // those keys when complete.
  virtual void LoadKeys(typename Callbacks::LoadKeysCallback callback) = 0;

  // Asynchronously loads a single entry, identified by |key|, from the database
  // and invokes |callback| when complete. If no entry with |key| is found,
  // a nullptr is passed to the callback, but the success flag is still true.
  virtual void GetEntry(
      const std::string& key,
      typename Callbacks::Internal<T>::GetCallback callback) = 0;

  // Asynchronously destroys the database. Use this call only if the database
  // needs to be destroyed for this particular profile. If the database is no
  // longer useful for everyone, the client name must be added to
  // |kObsoleteSharedProtoDatabaseClients| to ensure automatic clean up of the
  // database from all users.
  virtual void Destroy(Callbacks::DestroyCallback callback) = 0;

 protected:
  ProtoDatabase() = default;
};

// Return a new instance of Options, but with two additions:
// 1) create_if_missing = true
// 2) max_open_files = 0
leveldb_env::Options COMPONENT_EXPORT(LEVELDB_PROTO) CreateSimpleOptions();

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_