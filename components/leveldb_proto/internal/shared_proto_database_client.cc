// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database_client.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {
namespace {
const ProtoDbType* g_obsolete_client_list_for_testing = nullptr;

// Holds the db wrapper alive and callback is called at destruction. This class
// is used to post multiple update tasks on |db_wrapper| and keep the instance
// alive till all the callbacks are returned.
class ObsoleteClientsDbHolder
    : public base::RefCounted<ObsoleteClientsDbHolder> {
 public:
  ObsoleteClientsDbHolder(std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
                          Callbacks::UpdateCallback callback)
      : success_(true),
        owned_db_wrapper_(std::move(db_wrapper)),
        callback_(std::move(callback)) {}

  void set_success(bool success) { success_ &= success; }

 private:
  friend class RefCounted<ObsoleteClientsDbHolder>;
  ~ObsoleteClientsDbHolder() { std::move(callback_).Run(success_); }

  bool success_;
  std::unique_ptr<ProtoLevelDBWrapper> owned_db_wrapper_;
  Callbacks::UpdateCallback callback_;
};

}  // namespace

// static
std::string SharedProtoDatabaseClient::StripPrefix(const std::string& key,
                                                   const std::string& prefix) {
  return base::StartsWith(key, prefix, base::CompareCase::SENSITIVE)
             ? key.substr(prefix.length())
             : key;
}

// static
std::unique_ptr<KeyVector> SharedProtoDatabaseClient::PrefixStrings(
    std::unique_ptr<KeyVector> strings,
    const std::string& prefix) {
  for (auto& str : *strings)
    str.assign(base::StrCat({prefix, str}));
  return strings;
}

// static
bool SharedProtoDatabaseClient::KeyFilterStripPrefix(
    const KeyFilter& key_filter,
    const std::string& prefix,
    const std::string& key) {
  if (key_filter.is_null())
    return true;
  return key_filter.Run(StripPrefix(key, prefix));
}

// static
void SharedProtoDatabaseClient::GetSharedDatabaseInitStatusAsync(
    const std::string& client_db_id,
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    Callbacks::InitStatusCallback callback) {
  shared_db->GetDatabaseInitStatusAsync(client_db_id, std::move(callback));
}

// static
void SharedProtoDatabaseClient::UpdateClientMetadataAsync(
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    const std::string& client_db_id,
    SharedDBMetadataProto::MigrationStatus migration_status,
    ClientCorruptCallback callback) {
  shared_db->UpdateClientMetadataAsync(client_db_id, migration_status,
                                       std::move(callback));
}

// static
void SharedProtoDatabaseClient::DestroyObsoleteSharedProtoDatabaseClients(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
    Callbacks::UpdateCallback callback) {
  ProtoLevelDBWrapper* db_wrapper_ptr = db_wrapper.get();
  scoped_refptr<ObsoleteClientsDbHolder> db_holder =
      new ObsoleteClientsDbHolder(std::move(db_wrapper), std::move(callback));

  const ProtoDbType* list = g_obsolete_client_list_for_testing
                                ? g_obsolete_client_list_for_testing
                                : kObsoleteSharedProtoDbTypeClients;
  for (size_t i = 0; list[i] != ProtoDbType::LAST; ++i) {
    // Callback keeps a ref pointer to db_holder alive till the changes are
    // done. |db_holder| will be destroyed once all the RemoveKeys() calls
    // return.
    Callbacks::UpdateCallback callback_wrapper =
        base::BindOnce([](scoped_refptr<ObsoleteClientsDbHolder> db_holder,
                          bool success) { db_holder->set_success(success); },
                       db_holder);
    // Remove all type prefixes for the client.
    // TODO(ssid): Support cleanup of namespaces for clients. This code assumes
    // the prefix contains the client namespace at the beginning.
    db_wrapper_ptr->RemoveKeys(
        base::BindRepeating([](const std::string& key) { return true; }),
        SharedProtoDatabaseClient::PrefixForDatabase(list[i]),
        std::move(callback_wrapper));
  }
}

// static
void SharedProtoDatabaseClient::SetObsoleteClientListForTesting(
    const ProtoDbType* list) {
  g_obsolete_client_list_for_testing = list;
}

// static
std::string SharedProtoDatabaseClient::PrefixForDatabase(ProtoDbType db_type) {
  return base::StringPrintf("%d_", static_cast<int>(db_type));
}

SharedProtoDatabaseClient::SharedProtoDatabaseClient(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
    ProtoDbType db_type,
    const scoped_refptr<SharedProtoDatabase>& parent_db)
    : UniqueProtoDatabase(std::move(db_wrapper)),
      prefix_(PrefixForDatabase(db_type)),
      parent_db_(parent_db) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SharedProtoDatabaseClient::~SharedProtoDatabaseClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedProtoDatabaseClient::Init(const std::string& client_uma_name,
                                     Callbacks::InitStatusCallback callback) {
  SetMetricsId(client_uma_name);
  GetSharedDatabaseInitStatusAsync(prefix_, parent_db_, std::move(callback));
}

void SharedProtoDatabaseClient::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    bool destroy_on_corruption,
    Callbacks::InitStatusCallback callback) {
  NOTREACHED();
}

void SharedProtoDatabaseClient::UpdateEntries(
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::UpdateEntries(
      PrefixKeyEntryVector(std::move(entries_to_save), prefix_),
      PrefixStrings(std::move(keys_to_remove), prefix_), std::move(callback));
}

void SharedProtoDatabaseClient::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter(std::move(entries_to_save), delete_key_filter,
                                std::string(), std::move(callback));
}

void SharedProtoDatabaseClient::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::UpdateEntriesWithRemoveFilter(
      PrefixKeyEntryVector(std::move(entries_to_save), prefix_),
      base::BindRepeating(&KeyFilterStripPrefix, delete_key_filter, prefix_),
      prefix_ + target_prefix, std::move(callback));
}

void SharedProtoDatabaseClient::LoadEntries(Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadEntriesWithFilter(KeyFilter(), std::move(callback));
}

void SharedProtoDatabaseClient::LoadEntriesWithFilter(
    const KeyFilter& filter,
    Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                        std::move(callback));
}

void SharedProtoDatabaseClient::LoadEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::LoadEntriesWithFilter(
      base::BindRepeating(&KeyFilterStripPrefix, filter, prefix_), options,
      prefix_ + target_prefix, std::move(callback));
}

void SharedProtoDatabaseClient::LoadKeys(Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadKeys(std::string(), std::move(callback));
}

void SharedProtoDatabaseClient::LoadKeys(const std::string& target_prefix,
                                         Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::LoadKeys(
      prefix_ + target_prefix,
      base::BindOnce(&SharedProtoDatabaseClient::StripPrefixLoadKeysCallback,
                     std::move(callback), prefix_));
}

void SharedProtoDatabaseClient::LoadKeysAndEntries(
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(KeyFilter(), std::move(callback));
}

void SharedProtoDatabaseClient::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                               std::move(callback));
}

void SharedProtoDatabaseClient::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&KeyFilterStripPrefix, filter, prefix_), options,
      prefix_ + target_prefix,
      base::BindOnce(
          &SharedProtoDatabaseClient::StripPrefixLoadKeysAndEntriesCallback,
          std::move(callback), prefix_));
}

void SharedProtoDatabaseClient::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::LoadKeysAndEntriesInRange(
      prefix_ + start, prefix_ + end,
      base::BindOnce(
          &SharedProtoDatabaseClient::StripPrefixLoadKeysAndEntriesCallback,
          std::move(callback), prefix_));
}

void SharedProtoDatabaseClient::GetEntry(const std::string& key,
                                         Callbacks::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase::GetEntry(prefix_ + key, std::move(callback));
}

void SharedProtoDatabaseClient::Destroy(Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter(
      std::make_unique<KeyValueVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindOnce([](Callbacks::DestroyCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void SharedProtoDatabaseClient::UpdateClientInitMetadata(
    SharedDBMetadataProto::MigrationStatus migration_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  migration_status_ = migration_status;
  // Tell the SharedProtoDatabase that we've seen the corruption state so it's
  // safe to update its records for this client.
  UpdateClientMetadataAsync(parent_db_, prefix_, migration_status_,
                            base::BindOnce([](bool success) {
                              // TODO(thildebr): Should we do anything special
                              // here? If the shared DB can't update the
                              // client's corruption counter to match its own,
                              // then the client will think it's corrupt on the
                              // next Init as well.
                            }));
}

// static
void SharedProtoDatabaseClient::StripPrefixLoadKeysCallback(
    Callbacks::LoadKeysCallback callback,
    const std::string& prefix,
    bool success,
    std::unique_ptr<leveldb_proto::KeyVector> keys) {
  auto stripped_keys = std::make_unique<leveldb_proto::KeyVector>();
  for (auto& key : *keys)
    stripped_keys->emplace_back(StripPrefix(key, prefix));
  std::move(callback).Run(success, std::move(stripped_keys));
}

// static
void SharedProtoDatabaseClient::StripPrefixLoadKeysAndEntriesCallback(
    Callbacks::LoadKeysAndEntriesCallback callback,
    const std::string& prefix,
    bool success,
    std::unique_ptr<KeyValueMap> keys_entries) {
  auto stripped_keys_map = std::make_unique<KeyValueMap>();
  for (auto& key_entry : *keys_entries) {
    stripped_keys_map->insert(
        std::make_pair(StripPrefix(key_entry.first, prefix), key_entry.second));
  }
  std::move(callback).Run(success, std::move(stripped_keys_map));
}

// static
std::unique_ptr<KeyValueVector> SharedProtoDatabaseClient::PrefixKeyEntryVector(
    std::unique_ptr<KeyValueVector> kev,
    const std::string& prefix) {
  for (auto& key_entry_pair : *kev) {
    key_entry_pair.first = base::StrCat({prefix, key_entry_pair.first});
  }
  return kev;
}

}  // namespace leveldb_proto
