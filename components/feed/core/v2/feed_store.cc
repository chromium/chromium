// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
namespace {

// Keys are defined as:
// S/<stream-id>                    -> stream_data
// T/<stream-id>/<sequence-number>  -> stream_structures
// c/<content-id>                   -> content
// a/<id>                           -> action
// s/<content-id>                   -> shared_state
// m                                -> metadata
constexpr char kMainStreamId[] = "0";
const char kStreamDataKey[] = "S/0";
const char kLocalActionPrefix[] = "a/";
const char kMetadataKey[] = "m";

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

std::string KeyForContentId(base::StringPiece prefix,
                            const feedwire::ContentId& content_id) {
  return base::StrCat({prefix, content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

std::string ContentKey(const feedwire::ContentId& content_id) {
  return KeyForContentId("c/", content_id);
}

std::string SharedStateKey(const feedwire::ContentId& content_id) {
  return KeyForContentId("s/", content_id);
}

std::string LocalActionKey(int64_t id) {
  return kLocalActionPrefix + base::NumberToString(id);
}

std::string LocalActionKey(const LocalActionId& id) {
  return LocalActionKey(id.GetUnsafeValue());
}

// Returns true if the record key is for stream data (stream_data,
// stream_structures, content, shared_state).
bool IsStreamRecordKey(base::StringPiece key) {
  return key.size() > 1 && key[1] == '/' &&
         (key[0] == 'S' || key[0] == 'T' || key[0] == 'c' || key[0] == 's');
}

bool IsLocalActionKey(const std::string& key) {
  return base::StartsWith(key, kLocalActionPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

std::string KeyForRecord(const feedstore::Record& record) {
  switch (record.data_case()) {
    case feedstore::Record::kStreamData:
      return kStreamDataKey;
    case feedstore::Record::kStreamStructures:
      return base::StrCat(
          {"T/", record.stream_structures().stream_id(), "/",
           base::NumberToString(record.stream_structures().sequence_number())});
    case feedstore::Record::kContent:
      return ContentKey(record.content().content_id());
    case feedstore::Record::kLocalAction:
      return LocalActionKey(record.local_action().id());
    case feedstore::Record::kSharedState:
      return SharedStateKey(record.shared_state().content_id());
    case feedstore::Record::kMetadata:
      return kMetadataKey;
    case feedstore::Record::DATA_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid record case " << record.data_case();
  return "";
}

bool FilterByKey(const base::flat_set<std::string>& key_set,
                 const std::string& key) {
  return key_set.contains(key);
}

feedstore::Record MakeRecord(feedstore::Content content) {
  feedstore::Record record;
  *record.mutable_content() = std::move(content);
  return record;
}

feedstore::Record MakeRecord(
    feedstore::StreamStructureSet stream_structure_set) {
  feedstore::Record record;
  *record.mutable_stream_structures() = std::move(stream_structure_set);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamSharedState shared_state) {
  feedstore::Record record;
  *record.mutable_shared_state() = std::move(shared_state);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamData stream_data) {
  feedstore::Record record;
  *record.mutable_stream_data() = std::move(stream_data);
  return record;
}

feedstore::Record MakeRecord(feedstore::StoredAction action) {
  feedstore::Record record;
  *record.mutable_local_action() = std::move(action);
  return record;
}

feedstore::Record MakeRecord(feedstore::Metadata metadata) {
  feedstore::Record record;
  *record.mutable_metadata() = std::move(metadata);
  return record;
}

template <typename T>
std::pair<std::string, feedstore::Record> MakeKeyAndRecord(T record_data) {
  std::pair<std::string, feedstore::Record> result;
  result.second = MakeRecord(std::move(record_data));
  result.first = KeyForRecord(result.second);
  return result;
}

std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
MakeUpdatesForStreamModelUpdateRequest(
    int32_t structure_set_sequence_number,
    std::unique_ptr<StreamModelUpdateRequest> update_request) {
  auto updates = std::make_unique<
      std::vector<std::pair<std::string, feedstore::Record>>>();
  updates->push_back(MakeKeyAndRecord(std::move(update_request->stream_data)));
  for (feedstore::Content& content : update_request->content) {
    updates->push_back(MakeKeyAndRecord(std::move(content)));
  }
  for (feedstore::StreamSharedState& shared_state :
       update_request->shared_states) {
    updates->push_back(MakeKeyAndRecord(std::move(shared_state)));
  }
  feedstore::StreamStructureSet stream_structure_set;
  stream_structure_set.set_stream_id(kMainStreamId);
  stream_structure_set.set_sequence_number(structure_set_sequence_number);
  for (feedstore::StreamStructure& structure :
       update_request->stream_structures) {
    *stream_structure_set.add_structures() = std::move(structure);
  }
  updates->push_back(MakeKeyAndRecord(std::move(stream_structure_set)));

  return updates;
}

void SortActions(std::vector<feedstore::StoredAction>* actions) {
  std::sort(actions->begin(), actions->end(),
            [](const feedstore::StoredAction& a,
               const feedstore::StoredAction& b) { return a.id() < b.id(); });
}

}  // namespace

FeedStore::LoadStreamResult::LoadStreamResult() = default;
FeedStore::LoadStreamResult::~LoadStreamResult() = default;
FeedStore::LoadStreamResult::LoadStreamResult(LoadStreamResult&&) = default;
FeedStore::LoadStreamResult& FeedStore::LoadStreamResult::operator=(
    LoadStreamResult&&) = default;

FeedStore::FeedStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      database_(std::move(database)) {
}

FeedStore::~FeedStore() = default;

void FeedStore::Initialize(base::OnceClosure initialize_complete) {
  if (IsInitialized()) {
    std::move(initialize_complete).Run();
  } else {
    initialize_callback_ = std::move(initialize_complete);
    database_->Init(
        base::BindOnce(&FeedStore::OnDatabaseInitialized, GetWeakPtr()));
  }
}

void FeedStore::OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status) {
  database_status_ = status;
  if (initialize_callback_)
    std::move(initialize_callback_).Run();
}

bool FeedStore::IsInitialized() const {
  return database_status_ == leveldb_proto::Enums::InitStatus::kOK;
}

bool FeedStore::IsInitializedForTesting() const {
  return IsInitialized();
}

void FeedStore::ReadSingle(
    const std::string& key,
    base::OnceCallback<void(bool, std::unique_ptr<feedstore::Record>)>
        callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  database_->GetEntry(key, std::move(callback));
}

void FeedStore::ReadMany(
    const base::flat_set<std::string>& key_set,
    base::OnceCallback<
        void(bool, std::unique_ptr<std::vector<feedstore::Record>>)> callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  database_->LoadEntriesWithFilter(
      base::BindRepeating(&FilterByKey, std::move(key_set)),
      CreateReadOptions(),
      /*target_prefix=*/"", std::move(callback));
}

void FeedStore::ClearAll(base::OnceCallback<void(bool)> callback) {
  auto filter = [](const std::string& key) { return true; };

  database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      base::BindRepeating(filter), std::move(callback));
}

void FeedStore::LoadStream(
    base::OnceCallback<void(LoadStreamResult)> callback) {
  if (!IsInitialized()) {
    LoadStreamResult result;
    result.read_error = true;
    std::move(callback).Run(std::move(result));
    return;
  }
  auto filter = [](const std::string& key) {
    // Read stream data, stream structures, and pending actions.
    return key == kStreamDataKey ||
           base::StartsWith(key, "T/0/", base::CompareCase::SENSITIVE) ||
           IsLocalActionKey(key);
  };
  database_->LoadEntriesWithFilter(
      base::BindRepeating(filter), CreateReadOptions(),
      /*target_prefix=*/"",
      base::BindOnce(&FeedStore::OnLoadStreamFinished, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnLoadStreamFinished(
    base::OnceCallback<void(LoadStreamResult)> callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  LoadStreamResult result;
  if (!records || !success) {
    result.read_error = true;
  } else {
    for (feedstore::Record& record : *records) {
      switch (record.data_case()) {
        case feedstore::Record::kStreamData:
          result.stream_data = std::move(*record.mutable_stream_data());
          break;
        case feedstore::Record::kStreamStructures:
          result.stream_structures.push_back(
              std::move(*record.mutable_stream_structures()));
          break;
        case feedstore::Record::kLocalAction:
          result.pending_actions.push_back(
              std::move(*record.mutable_local_action()));
          break;
        default:
          break;
      }
    }
  }

  SortActions(&result.pending_actions);
  std::move(callback).Run(std::move(result));
}

void FeedStore::OverwriteStream(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    base::OnceCallback<void(bool)> callback) {
  std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
      updates = MakeUpdatesForStreamModelUpdateRequest(
          /*structure_set_sequence_number=*/0, std::move(update_request));
  UpdateFullStreamData(std::move(updates), std::move(callback));
}

void FeedStore::UpdateFullStreamData(
    std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
        updates,
    base::OnceCallback<void(bool)> callback) {
  // Set up a filter to delete all stream-related data.
  // But we need to exclude keys being written right now.
  std::vector<std::string> key_vector(updates->size());
  for (size_t i = 0; i < key_vector.size(); ++i) {
    key_vector[i] = (*updates)[i].first;
  }
  base::flat_set<std::string> updated_keys(std::move(key_vector));

  auto filter = [](const base::flat_set<std::string>& updated_keys,
                   const std::string& key) {
    return IsStreamRecordKey(key) && !updated_keys.contains(key);
  };

  database_->UpdateEntriesWithRemoveFilter(
      std::move(updates), base::BindRepeating(filter, std::move(updated_keys)),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::SaveStreamUpdate(
    int32_t structure_set_sequence_number,
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    base::OnceCallback<void(bool)> callback) {
  database_->UpdateEntries(
      MakeUpdatesForStreamModelUpdateRequest(structure_set_sequence_number,
                                             std::move(update_request)),
      std::make_unique<leveldb_proto::KeyVector>(),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::ClearStreamData(base::OnceCallback<void(bool)> callback) {
  UpdateFullStreamData(
      std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      std::move(callback));
}

void FeedStore::OnSaveStreamEntriesUpdated(
    base::OnceCallback<void(bool)> complete_callback,
    bool ok) {
  std::move(complete_callback).Run(ok);
}

void FeedStore::WriteOperations(
    int32_t sequence_number,
    std::vector<feedstore::DataOperation> operations) {
  std::vector<feedstore::Record> records;
  feedstore::Record structures_record;
  feedstore::StreamStructureSet& structure_set =
      *structures_record.mutable_stream_structures();
  for (feedstore::DataOperation& operation : operations) {
    *structure_set.add_structures() = std::move(*operation.mutable_structure());
    if (operation.has_content()) {
      feedstore::Record record;
      record.set_allocated_content(operation.release_content());
      records.push_back(std::move(record));
    }
  }
  structure_set.set_stream_id(kMainStreamId);
  structure_set.set_sequence_number(sequence_number);

  records.push_back(std::move(structures_record));
  Write(std::move(records), base::DoNothing());
}

void FeedStore::ReadContent(
    std::vector<feedwire::ContentId> content_ids,
    std::vector<feedwire::ContentId> shared_state_ids,
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        content_callback) {
  std::vector<std::string> key_vector;
  key_vector.reserve(content_ids.size() + shared_state_ids.size());
  for (const auto& content_id : content_ids)
    key_vector.push_back(ContentKey(content_id));
  for (const auto& content_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(content_id));

  for (const auto& shared_state_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(shared_state_id));

  ReadMany(base::flat_set<std::string>(std::move(key_vector)),
           base::BindOnce(&FeedStore::OnReadContentFinished, GetWeakPtr(),
                          std::move(content_callback)));
}

void FeedStore::OnReadContentFinished(
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  if (!success || !records) {
    std::move(callback).Run({}, {});
    return;
  }

  std::vector<feedstore::Content> content;
  // Most of records will be content.
  content.reserve(records->size());
  std::vector<feedstore::StreamSharedState> shared_states;
  for (auto& record : *records) {
    if (record.data_case() == feedstore::Record::kContent)
      content.push_back(std::move(record.content()));
    else if (record.data_case() == feedstore::Record::kSharedState)
      shared_states.push_back(std::move(record.shared_state()));
  }

  std::move(callback).Run(std::move(content), std::move(shared_states));
}

void FeedStore::ReadActions(
    base::OnceCallback<void(std::vector<feedstore::StoredAction>)> callback) {
  database_->LoadEntriesWithFilter(
      base::BindRepeating(IsLocalActionKey),
      base::BindOnce(&FeedStore::OnReadActionsFinished, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnReadActionsFinished(
    base::OnceCallback<void(std::vector<feedstore::StoredAction>)> callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  if (!success || !records) {
    std::move(callback).Run({});
    return;
  }

  std::vector<feedstore::StoredAction> actions;
  actions.reserve(records->size());
  for (auto& record : *records)
    actions.push_back(std::move(record.local_action()));

  SortActions(&actions);
  std::move(callback).Run(std::move(actions));
}

void FeedStore::WriteActions(std::vector<feedstore::StoredAction> actions,
                             base::OnceCallback<void(bool)> callback) {
  std::vector<feedstore::Record> records;
  records.reserve(actions.size());
  for (auto& action : actions) {
    feedstore::Record record;
    *record.mutable_local_action() = std::move(action);
    records.push_back(record);
  }

  Write(std::move(records), std::move(callback));
}

void FeedStore::UpdateActions(
    std::vector<feedstore::StoredAction> actions_to_update,
    std::vector<LocalActionId> ids_to_remove,
    base::OnceCallback<void(bool)> callback) {
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<feedstore::Record>::KeyEntryVector>();
  for (auto& action : actions_to_update)
    entries_to_save->push_back(MakeKeyAndRecord(std::move(action)));

  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  for (LocalActionId id : ids_to_remove)
    keys_to_remove->push_back(LocalActionKey(id));

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove), std::move(callback));
}

void FeedStore::RemoveActions(std::vector<LocalActionId> ids,
                              base::OnceCallback<void(bool)> callback) {
  auto keys = std::make_unique<std::vector<std::string>>();
  keys->reserve(ids.size());
  for (LocalActionId id : ids)
    keys->push_back(LocalActionKey(id));

  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      /*keys_to_remove=*/std::move(keys), std::move(callback));
}

void FeedStore::Write(std::vector<feedstore::Record> records,
                      base::OnceCallback<void(bool)> callback) {
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<feedstore::Record>::KeyEntryVector>();
  for (auto& record : records) {
    std::string key = KeyForRecord(record);
    if (!key.empty())
      entries_to_save->push_back({std::move(key), std::move(record)});
  }

  database_->UpdateEntries(
      std::move(entries_to_save),
      /*keys_to_remove=*/std::make_unique<leveldb_proto::KeyVector>(),
      base::BindOnce(&FeedStore::OnWriteFinished, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnWriteFinished(base::OnceCallback<void(bool)> callback,
                                bool success) {
  std::move(callback).Run(success);
}

void FeedStore::ReadMetadata(
    base::OnceCallback<void(std::unique_ptr<feedstore::Metadata>)> callback) {
  ReadSingle(kMetadataKey, base::BindOnce(&FeedStore::OnReadMetadataFinished,
                                          GetWeakPtr(), std::move(callback)));
}

void FeedStore::OnReadMetadataFinished(
    base::OnceCallback<void(std::unique_ptr<feedstore::Metadata>)> callback,
    bool read_ok,
    std::unique_ptr<feedstore::Record> record) {
  if (!record || !read_ok) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(base::WrapUnique(record->release_metadata()));
}

void FeedStore::WriteMetadata(feedstore::Metadata metadata,
                              base::OnceCallback<void(bool)> callback) {
  Write({MakeRecord(std::move(metadata))}, std::move(callback));
}

}  // namespace feed
