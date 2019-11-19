// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/internal/proto_database_selector.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace leveldb_proto {

// Update transactions happen on background task runner and callback runs on the
// client task runner.
void COMPONENT_EXPORT(LEVELDB_PROTO) RunUpdateCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::UpdateCallback callback,
    bool success);

// Load transactions happen on background task runner. The loaded keys need to
// be given to clients on client task runner.
void COMPONENT_EXPORT(LEVELDB_PROTO) RunLoadKeysCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::LoadKeysCallback callback,
    bool success,
    std::unique_ptr<KeyVector> keys);

// Helper to run destroy callback on the client task runner.
void COMPONENT_EXPORT(LEVELDB_PROTO) RunDestroyCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::DestroyCallback callback,
    bool success);

// The ProtoDatabaseImpl<T> implements a ProtoDatabase<T> instance, and allows
// the underlying ProtoDatabase<T> implementation to change without users of the
// wrapper needing to know.
// This allows clients to request a DB instance without knowing whether or not
// it's a UniqueProtoDatabase or a SharedProtoDatabaseClient.
template <typename P, typename T = P>
class ProtoDatabaseImpl : public ProtoDatabase<P, T> {
 public:
  // Force usage of unique db.
  ProtoDatabaseImpl(
      ProtoDbType db_type,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Internal implementation is free to choose between unique and shared
  // database to use here (transparently).
  ProtoDatabaseImpl(ProtoDbType db_type,
                    const base::FilePath& db_dir,
                    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                    std::unique_ptr<SharedProtoDatabaseProvider> db_provider);

  virtual ~ProtoDatabaseImpl() = default;

  void Init(Callbacks::InitStatusCallback callback) override;
  void Init(const leveldb_env::Options& unique_db_options,
            Callbacks::InitStatusCallback callback) override;

  // Internal only api.
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        Callbacks::InitStatusCallback callback);

  void UpdateEntries(std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
                         entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback) override;

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override;

  void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;

  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  void RemoveKeysForTesting(const KeyFilter& key_filter,
                            const std::string& target_prefix,
                            Callbacks::UpdateCallback callback);

  // Not thread safe.
  ProtoDatabaseSelector* db_wrapper_for_testing() { return db_wrapper_.get(); }

 private:
  template <typename T_>
  friend class ProtoDatabaseImplTest;

  void InitInternal(const std::string& client_name,
                    const leveldb_env::Options& options,
                    bool use_shared_db,
                    Callbacks::InitStatusCallback callback);

  void PostTransaction(base::OnceClosure task);

  ProtoDbType db_type_;
  scoped_refptr<ProtoDatabaseSelector> db_wrapper_;
  const bool force_unique_db_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath db_dir_;
};

namespace {

template <typename P,
          typename T,
          std::enable_if_t<std::is_base_of<google::protobuf::MessageLite,
                                           T>::value>* = nullptr>
std::string SerializeAsString(T* entry) {
  return entry->SerializeAsString();
}

template <typename P,
          typename T,
          std::enable_if_t<!std::is_base_of<google::protobuf::MessageLite,
                                            T>::value>* = nullptr>
std::string SerializeAsString(T* entry) {
  P proto;
  DataToProto(entry, &proto);
  return proto.SerializeAsString();
}

template <typename P>
bool ParseToProto(const std::string& serialized_entry, P* proto) {
  if (!proto->ParseFromString(serialized_entry)) {
    DLOG(WARNING) << "Unable to parse leveldb_proto entry";
    *proto = P();
    return false;
  }
  return true;
}

template <typename P,
          typename T,
          std::enable_if_t<std::is_base_of<google::protobuf::MessageLite,
                                           T>::value>* = nullptr>
bool ParseToClientType(const std::string& serialized_entry, T* output) {
  return ParseToProto<T>(serialized_entry, output);
}

template <typename P,
          typename T,
          std::enable_if_t<!std::is_base_of<google::protobuf::MessageLite,
                                            T>::value>* = nullptr>
bool ParseToClientType(const std::string& serialized_entry, T* entry) {
  P proto;
  if (!ParseToProto<P>(serialized_entry, &proto))
    return false;

  ProtoToData(&proto, entry);
  return true;
}

// Update transactions need to serialize the entries to be updated on background
// task runner. The database can be accessed on same task runner. The caller
// must wrap the callback using RunUpdateCallback() to ensure the callback runs
// in client task runner.
template <typename P, typename T>
void UpdateEntriesFromTaskRunner(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    scoped_refptr<ProtoDatabaseSelector> db,
    Callbacks::UpdateCallback callback) {
  // Serialize the values from Proto to string before passing on to database.
  auto pairs_to_save = std::make_unique<KeyValueVector>();
  for (auto& pair : *entries_to_save) {
    auto serialized = SerializeAsString<P, T>(&pair.second);
    pairs_to_save->push_back(std::make_pair(pair.first, serialized));
  }

  db->UpdateEntries(std::move(pairs_to_save), std::move(keys_to_remove),
                    std::move(callback));
}

// Update transactions need to serialize the entries to be updated on background
// task runner. The database can be accessed on same task runner. The caller
// must wrap the callback using RunUpdateCallback() to ensure the callback runs
// in client task runner.
template <typename P, typename T>
void UpdateEntriesWithRemoveFilterFromTaskRunner(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    scoped_refptr<ProtoDatabaseSelector> db,
    Callbacks::UpdateCallback callback) {
  // Serialize the values from Proto to string before passing on to database.
  auto pairs_to_save = std::make_unique<KeyValueVector>();
  for (auto& pair : *entries_to_save) {
    auto serialized = SerializeAsString<P, T>(&pair.second);
    pairs_to_save->push_back(std::make_pair(pair.first, serialized));
  }

  db->UpdateEntriesWithRemoveFilter(std::move(pairs_to_save), delete_key_filter,
                                    std::move(callback));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename P, typename T>
void ParseLoadedEntries(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::LoadCallback callback,
    bool success,
    std::unique_ptr<ValueVector> loaded_entries) {
  auto entries = std::make_unique<std::vector<T>>();

  if (!success || !loaded_entries) {
    entries.reset();
  } else {
    for (const auto& serialized_entry : *loaded_entries) {
      entries->emplace_back(T());
      ParseToClientType<P, T>(serialized_entry, &entries->back());
    }
  }

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entries)));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename P, typename T>
void ParseLoadedKeysAndEntries(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback,
    bool success,
    std::unique_ptr<KeyValueMap> loaded_entries) {
  auto keys_entries = std::make_unique<std::map<std::string, T>>();
  if (!success || !loaded_entries) {
    keys_entries.reset();
  } else {
    for (const auto& pair : *loaded_entries) {
      auto it = keys_entries->emplace(pair.first, T());
      ParseToClientType<P, T>(pair.second, &(it.first->second));
    }
  }

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(keys_entries)));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename P, typename T>
void ParseLoadedEntry(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::GetCallback callback,
    bool success,
    std::unique_ptr<std::string> serialized_entry) {
  auto entry = std::make_unique<T>();

  if (!success || !serialized_entry) {
    entry.reset();
  } else if (!ParseToClientType<P, T>(*serialized_entry, entry.get())) {
    success = false;
  }
  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entry)));
}

}  // namespace

template <typename P, typename T>
ProtoDatabaseImpl<P, T>::ProtoDatabaseImpl(
    ProtoDbType db_type,
    const base::FilePath& db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_type_(db_type),
      db_wrapper_(new ProtoDatabaseSelector(db_type_, task_runner, nullptr)),
      force_unique_db_(true),
      task_runner_(task_runner),
      db_dir_(db_dir) {}

template <typename P, typename T>
ProtoDatabaseImpl<P, T>::ProtoDatabaseImpl(
    ProtoDbType db_type,
    const base::FilePath& db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<SharedProtoDatabaseProvider> db_provider)
    : db_type_(db_type),
      db_wrapper_(new ProtoDatabaseSelector(db_type_,
                                            task_runner,
                                            std::move(db_provider))),
      force_unique_db_(false),
      task_runner_(task_runner),
      db_dir_(db_dir) {}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::Init(
    typename Callbacks::InitStatusCallback callback) {
  bool use_shared_db =
      !force_unique_db_ &&
      SharedProtoDatabaseClientList::ShouldUseSharedDB(db_type_);
  const std::string& client_uma_name =
      SharedProtoDatabaseClientList::ProtoDbTypeToString(db_type_);

  InitInternal(client_uma_name, CreateSimpleOptions(), use_shared_db,
               std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::Init(
    const leveldb_env::Options& unique_db_options,
    typename Callbacks::InitStatusCallback callback) {
  bool use_shared_db =
      !force_unique_db_ &&
      SharedProtoDatabaseClientList::ShouldUseSharedDB(db_type_);
  const std::string& client_uma_name =
      SharedProtoDatabaseClientList::ProtoDbTypeToString(db_type_);

  InitInternal(client_uma_name, unique_db_options, use_shared_db,
               std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::InitInternal(
    const std::string& client_name,
    const leveldb_env::Options& unique_db_options,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProtoDatabaseSelector::InitUniqueOrShared, db_wrapper_,
                     client_name, db_dir_, unique_db_options, use_shared_db,
                     base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    Callbacks::InitStatusCallback callback) {
  DCHECK(force_unique_db_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProtoDatabaseSelector::InitWithDatabase, db_wrapper_,
                     base::Unretained(database), database_dir, options,
                     base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &UpdateEntriesFromTaskRunner<P, T>, std::move(entries_to_save),
      std::move(keys_to_remove), db_wrapper_,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &UpdateEntriesWithRemoveFilterFromTaskRunner<P, T>,
      std::move(entries_to_save), delete_key_filter, db_wrapper_,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter(KeyFilter(), std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                        std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadEntriesWithFilter, db_wrapper_,
                     key_filter, options, target_prefix,
                     base::BindOnce(&ParseLoadedEntries<P, T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(KeyFilter(), std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                               std::move(callback));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeysAndEntriesWithFilter,
                     db_wrapper_, filter, options, target_prefix,
                     base::BindOnce(&ParseLoadedKeysAndEntries<P, T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeysAndEntriesInRange,
                     db_wrapper_, start, end,
                     base::BindOnce(&ParseLoadedKeysAndEntries<P, T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::LoadKeys(Callbacks::LoadKeysCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeys, db_wrapper_,
                     base::BindOnce(&RunLoadKeysCallback,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  base::OnceClosure get_task =
      base::BindOnce(&ProtoDatabaseSelector::GetEntry, db_wrapper_, key,
                     base::BindOnce(&ParseLoadedEntry<P, T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(get_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::Destroy(Callbacks::DestroyCallback callback) {
  base::OnceClosure destroy_task =
      base::BindOnce(&ProtoDatabaseSelector::Destroy, db_wrapper_,
                     base::BindOnce(&RunDestroyCallback,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(destroy_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &ProtoDatabaseSelector::RemoveKeysForTesting, db_wrapper_, key_filter,
      target_prefix,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename P, typename T>
void ProtoDatabaseImpl<P, T>::PostTransaction(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ProtoDatabaseSelector::AddTransaction,
                                        db_wrapper_, std::move(task)));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_