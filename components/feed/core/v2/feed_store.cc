// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"

#include <string_view>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
namespace {

// Keys are defined as:
// [Key format]                     -> [Record field]
// S/<stream-key>                   -> stream_data
// T/<stream-key>/<sequence-number> -> stream_structures
// c/<stream-key>/<content-id>      -> content
// s/<stream-key>/<content-id>      -> shared_state
// a/<action-id>                    -> action
// m                                -> metadata
// subs                             -> subscribed_web_feeds
// recommendedIndex                 -> recommended_web_feed_index
// R/<web_feed_id>                  -> recommended_web_feed
// W/<operation-id>                 -> pending_web_feed_operation
// v/<docid>/<timestamp>            -> docview
constexpr char kLocalActionPrefix[] = "a/";
constexpr char kMetadataKey[] = "m";
constexpr char kSubscribedFeedsKey[] = "subs";
constexpr char kRecommendedIndexKey[] = "recommendedIndex";
constexpr char kPendingWebFeedOperationPrefix[] = "W/";
constexpr char kStreamDataPrefix[] = "S/";
constexpr char kkSingleWebFeedStreamDataPrefix[] = "S/c";

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

// For avoiding multiple `StrCat()` calls when generating a single string.
#define CONTENT_ID_STRING_PARTS(content_id)                                  \
  content_id.content_domain(), ",", base::NumberToString(content_id.type()), \
      ",", base::NumberToString(content_id.id())

std::string StreamDataKey(std::string_view stream_key) {
  return base::StrCat({kStreamDataPrefix, stream_key});
}
std::string StreamDataKey(const StreamType& stream_type) {
  return StreamDataKey(feedstore::StreamKey(stream_type));
}
std::string ContentKey(std::string_view stream_type,
                       const feedwire::ContentId& content_id) {
  return base::StrCat(
      {"c/", stream_type, "/", CONTENT_ID_STRING_PARTS(content_id)});
}
std::string ContentKey(const StreamType& stream_type,
                       const feedwire::ContentId& content_id) {
  return ContentKey(feedstore::StreamKey(stream_type), content_id);
}
std::string SharedStateKey(std::string_view stream_type,
                           const feedwire::ContentId& content_id) {
  return base::StrCat(
      {"s/", stream_type, "/", CONTENT_ID_STRING_PARTS(content_id)});
}
std::string SharedStateKey(const StreamType& stream_type,
                           const feedwire::ContentId& content_id) {
  return SharedStateKey(feedstore::StreamKey(stream_type), content_id);
}
std::string LocalActionKey(int64_t id) {
  return kLocalActionPrefix + base::NumberToString(id);
}
std::string LocalActionKey(const LocalActionId& id) {
  return LocalActionKey(id.GetUnsafeValue());
}
std::string DocViewKey(const feedstore::DocView& doc_view) {
  return base::StrCat({"v/", base::NumberToString(doc_view.docid()), "/",
                       base::NumberToString(doc_view.view_time_millis())});
}

// Returns true if the record key is for stream data (stream_data,
// stream_structures, content, shared_state).
bool IsAnyStreamRecordKey(const std::string& key) {
  return key.size() > 1 && key[1] == '/' &&
         (key[0] == 'S' || key[0] == 'T' || key[0] == 'c' || key[0] == 's');
}

// For matching keys that belong to a specific stream type.
class StreamKeyMatcher {
 public:
  explicit StreamKeyMatcher(const StreamType& stream_type) {
    stream_key_ = std::string(feedstore::StreamKey(stream_type));
    stream_key_plus_slash_ = stream_key_ + '/';
  }

  // Returns true if `key` is a key specific to `stream_type`.
  bool IsKeyForStream(std::string_view key) const {
    if (key.size() < 2 || key[1] != '/') {
      return false;
    }
    const std::string_view key_suffix = key.substr(2);
    switch (key[0]) {
      case 'S':
        return key_suffix == stream_key_;
      case 'T':
      case 'c':
      case 's':
        return base::StartsWith(key_suffix, stream_key_plus_slash_);
    }
    return false;
  }

 private:
  std::string stream_key_;
  std::string stream_key_plus_slash_;
};

// For matching keys that belong to a specific stream type.
class StreamPrefixMatcher {
 public:
  explicit StreamPrefixMatcher(StreamKind stream_kind) {
    stream_prefix_ = std::string(feedstore::StreamPrefix(stream_kind));
  }

  // Returns true if `key` is a key specific to `stream_kind`.
  bool IsKeyForStream(std::string_view key) const {
    if (key.size() < 2 || key[1] != '/') {
      return false;
    }
    const std::string_view key_suffix = key.substr(2);
    switch (key[0]) {
      case 'S':
      case 'T':
      case 'c':
      case 's':
        return base::StartsWith(key_suffix, stream_prefix_);
    }
    return false;
  }

 private:
  std::string stream_prefix_;
};

bool IsLocalActionKey(const std::string& key) {
  return base::StartsWith(key, kLocalActionPrefix);
}

std::string KeyForRecord(const feedstore::Record& record) {
  switch (record.data_case()) {
    case feedstore::Record::kStreamData: {
      const std::string stream_key = record.stream_data().stream_key();
      return stream_key.empty() ? StreamDataKey(StreamType(StreamKind::kForYou))
                                : StreamDataKey(stream_key);
    }
    case feedstore::Record::kStreamStructures:
      return base::StrCat(
          {"T/", record.stream_structures().stream_key(), "/",
           base::NumberToString(record.stream_structures().sequence_number())});
    case feedstore::Record::kContent:
      return ContentKey(record.content().stream_key(),
                        record.content().content_id());
    case feedstore::Record::kLocalAction:
      return LocalActionKey(record.local_action().id());
    case feedstore::Record::kSharedState:
      return SharedStateKey(record.shared_state().stream_key(),
                            record.shared_state().content_id());
    case feedstore::Record::kMetadata:
      return kMetadataKey;
    case feedstore::Record::kSubscribedWebFeeds:
      return kSubscribedFeedsKey;
    case feedstore::Record::kRecommendedWebFeed:
      return base::StrCat({"R/", record.recommended_web_feed().web_feed_id()});
    case feedstore::Record::kRecommendedWebFeedIndex:
      return kRecommendedIndexKey;
    case feedstore::Record::kPendingWebFeedOperation:
      return base::StrCat(
          {"W/",
           base::NumberToString(record.pending_web_feed_operation().id())});
    case feedstore::Record::kDocView:
      return DocViewKey(record.doc_view());
    case feedstore::Record::DATA_NOT_SET:
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid record case " << record.data_case();
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

feedstore::Record MakeRecord(feedstore::RecommendedWebFeedIndex index) {
  feedstore::Record record;
  *record.mutable_recommended_web_feed_index() = std::move(index);
  return record;
}

feedstore::Record MakeRecord(
    feedstore::SubscribedWebFeeds subscribed_web_feeds) {
  feedstore::Record record;
  *record.mutable_subscribed_web_feeds() = std::move(subscribed_web_feeds);
  return record;
}

feedstore::Record MakeRecord(feedstore::WebFeedInfo web_feed_info) {
  feedstore::Record record;
  *record.mutable_recommended_web_feed() = std::move(web_feed_info);
  return record;
}

feedstore::Record MakeRecord(feedstore::PendingWebFeedOperation operation) {
  feedstore::Record record;
  *record.mutable_pending_web_feed_operation() = std::move(operation);
  return record;
}

feedstore::Record MakeRecord(feedstore::DocView doc_view) {
  feedstore::Record record;
  *record.mutable_doc_view() = std::move(doc_view);
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
    const StreamType& stream_type,
    std::unique_ptr<StreamModelUpdateRequest> update_request) {
  std::string stream_key = feedstore::StreamKey(stream_type);
  auto updates = std::make_unique<
      std::vector<std::pair<std::string, feedstore::Record>>>();
  update_request->stream_data.set_stream_key(stream_key);
  updates->push_back(MakeKeyAndRecord(std::move(update_request->stream_data)));
  for (feedstore::Content& content : update_request->content) {
    content.set_stream_key(stream_key);
    updates->push_back(MakeKeyAndRecord(std::move(content)));
  }
  for (feedstore::StreamSharedState& shared_state :
       update_request->shared_states) {
    shared_state.set_stream_key(stream_key);
    updates->push_back(MakeKeyAndRecord(std::move(shared_state)));
  }
  feedstore::StreamStructureSet stream_structure_set;
  stream_structure_set.set_stream_key(stream_key);
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

base::OnceCallback<void(bool)> DropBoolParam(base::OnceClosure callback) {
  return base::BindOnce(
      [](base::OnceClosure callback, bool ok) { std::move(callback).Run(); },
      std::move(callback));
}

}  // namespace

FeedStore::LoadStreamResult::LoadStreamResult() = default;
FeedStore::LoadStreamResult::~LoadStreamResult() = default;
FeedStore::LoadStreamResult::LoadStreamResult(LoadStreamResult&&) = default;
FeedStore::LoadStreamResult& FeedStore::LoadStreamResult::operator=(
    LoadStreamResult&&) = default;

FeedStore::StartupData::StartupData() = default;
FeedStore::StartupData::StartupData(StartupData&&) = default;
FeedStore::StartupData::~StartupData() = default;
FeedStore::StartupData& FeedStore::StartupData::operator=(StartupData&&) =
    default;

FeedStore::WebFeedStartupData::WebFeedStartupData() = default;
FeedStore::WebFeedStartupData::WebFeedStartupData(WebFeedStartupData&&) =
    default;
FeedStore::WebFeedStartupData::~WebFeedStartupData() = default;
FeedStore::WebFeedStartupData& FeedStore::WebFeedStartupData::operator=(
    WebFeedStartupData&&) = default;

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
    const StreamType& stream_type,
    base::OnceCallback<void(LoadStreamResult)> callback) {
  if (!IsInitialized()) {
    LoadStreamResult result;
    result.read_error = true;
    std::move(callback).Run(std::move(result));
    return;
  }
  auto filter = [](const std::string& stream_data_key,
                   const std::string& structure_key_prefix,
                   const std::string& key) {
    return key == stream_data_key ||
           base::StartsWith(key, structure_key_prefix) || IsLocalActionKey(key);
  };

  database_->LoadEntriesWithFilter(
      base::BindRepeating(
          filter, StreamDataKey(stream_type),
          base::StrCat({"T/", feedstore::StreamKey(stream_type), "/"})),
      CreateReadOptions(),
      /*target_prefix=*/"",
      base::BindOnce(&FeedStore::OnLoadStreamFinished, GetWeakPtr(),
                     stream_type, std::move(callback)));
}

void FeedStore::OnLoadStreamFinished(
    const StreamType& stream_type,
    base::OnceCallback<void(LoadStreamResult)> callback,
    bool success,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  LoadStreamResult result;
  result.stream_type = stream_type;
  if (!records || !success) {
    result.read_error = true;
  } else {
    for (feedstore::Record& record : *records) {
      switch (record.data_case()) {
        case feedstore::Record::kStreamData:
          result.stream_data = std::move(*record.mutable_stream_data());
          DLOG_IF(ERROR, result.stream_data.stream_key() !=
                             feedstore::StreamKey(stream_type))
              << "Read a record with the wrong stream_key";
          break;
        case feedstore::Record::kStreamStructures:
          DLOG_IF(ERROR, record.stream_structures().stream_key() !=
                             feedstore::StreamKey(stream_type))
              << "Read a record with the wrong stream_key";
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
    const StreamType& stream_type,
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    base::OnceCallback<void(bool)> callback) {
  std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
      updates = MakeUpdatesForStreamModelUpdateRequest(
          /*structure_set_sequence_number=*/0, stream_type,
          std::move(update_request));
  UpdateFullStreamData(stream_type, std::move(updates), std::move(callback));
}

void FeedStore::UpdateFullStreamData(
    const StreamType& stream_type,
    std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
        updates,
    base::OnceCallback<void(bool)> callback) {
  // Set up a filter to delete all stream-related data.
  // But we need to exclude keys being written right now.
  auto updated_keys = base::MakeFlatSet<std::string>(
      *updates, {}, &std::pair<std::string, feedstore::Record>::first);
  StreamKeyMatcher key_matcher(stream_type);
  auto filter = [](const StreamKeyMatcher& key_matcher,
                   const base::flat_set<std::string>& updated_keys,
                   const std::string& key) {
    return key_matcher.IsKeyForStream(key) && !updated_keys.contains(key);
  };

  database_->UpdateEntriesWithRemoveFilter(
      std::move(updates),
      base::BindRepeating(filter, key_matcher, std::move(updated_keys)),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::SaveStreamUpdate(
    const StreamType& stream_type,
    int32_t structure_set_sequence_number,
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    base::OnceCallback<void(bool)> callback) {
  database_->UpdateEntries(
      MakeUpdatesForStreamModelUpdateRequest(structure_set_sequence_number,
                                             stream_type,
                                             std::move(update_request)),
      std::make_unique<leveldb_proto::KeyVector>(),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::ClearStreamData(const StreamType& stream_type,
                                base::OnceCallback<void(bool)> callback) {
  UpdateFullStreamData(
      stream_type,
      std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      std::move(callback));
}

void FeedStore::ClearAllStreamData(StreamKind stream_kind,
                                   base::OnceCallback<void(bool)> callback) {
  auto updates = std::make_unique<
      std::vector<std::pair<std::string, feedstore::Record>>>();
  // Set up a filter to delete all stream-related data.
  // But we need to exclude keys being written right now.
  StreamPrefixMatcher key_matcher(stream_kind);
  auto filter = [](const StreamPrefixMatcher& key_matcher,
                   const std::string& key) {
    return key_matcher.IsKeyForStream(key);
  };

  database_->UpdateEntriesWithRemoveFilter(
      std::move(updates), base::BindRepeating(filter, key_matcher),
      base::BindOnce(&FeedStore::OnSaveStreamEntriesUpdated, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnSaveStreamEntriesUpdated(
    base::OnceCallback<void(bool)> complete_callback,
    bool ok) {
  std::move(complete_callback).Run(ok);
}

void FeedStore::WriteOperations(
    const StreamType& stream_type,
    int32_t sequence_number,
    std::vector<feedstore::DataOperation> operations) {
  std::string stream_key = feedstore::StreamKey(stream_type);
  std::vector<feedstore::Record> records;
  feedstore::Record structures_record;
  feedstore::StreamStructureSet& structure_set =
      *structures_record.mutable_stream_structures();
  for (feedstore::DataOperation& operation : operations) {
    *structure_set.add_structures() = std::move(*operation.mutable_structure());
    if (operation.has_content()) {
      feedstore::Record record;
      operation.mutable_content()->set_stream_key(stream_key);
      record.set_allocated_content(operation.release_content());
      records.push_back(std::move(record));
    }
  }
  structure_set.set_stream_key(std::string(feedstore::StreamKey(stream_type)));
  structure_set.set_sequence_number(sequence_number);

  records.push_back(std::move(structures_record));
  Write(std::move(records), base::DoNothing());
}

void FeedStore::ReadContent(
    const StreamType& stream_type,
    std::vector<feedwire::ContentId> content_ids,
    std::vector<feedwire::ContentId> shared_state_ids,
    base::OnceCallback<void(std::vector<feedstore::Content>,
                            std::vector<feedstore::StreamSharedState>)>
        content_callback) {
  std::vector<std::string> key_vector;
  key_vector.reserve(content_ids.size() + shared_state_ids.size());
  for (const auto& content_id : content_ids)
    key_vector.push_back(ContentKey(stream_type, content_id));
  for (const auto& content_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(stream_type, content_id));

  for (const auto& shared_state_id : shared_state_ids)
    key_vector.push_back(SharedStateKey(stream_type, shared_state_id));

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

void FeedStore::ReadWebFeedStartupData(
    base::OnceCallback<void(WebFeedStartupData)> callback) {
  auto is_startup_data_filter = [](const std::string& key) {
    return key == kSubscribedFeedsKey || key == kRecommendedIndexKey ||
           base::StartsWith(key, kPendingWebFeedOperationPrefix);
  };

  database_->LoadEntriesWithFilter(
      base::BindRepeating(is_startup_data_filter),
      base::BindOnce(&FeedStore::OnReadWebFeedStartupDataFinished, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnReadWebFeedStartupDataFinished(
    base::OnceCallback<void(WebFeedStartupData)> callback,
    bool read_ok,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  WebFeedStartupData result;
  if (records) {
    for (feedstore::Record& r : *records) {
      if (r.has_recommended_web_feed_index()) {
        result.recommended_feed_index =
            std::move(*r.mutable_recommended_web_feed_index());
      } else if (r.has_subscribed_web_feeds()) {
        result.subscribed_web_feeds =
            std::move(*r.mutable_subscribed_web_feeds());
      } else if (r.has_pending_web_feed_operation()) {
        result.pending_operations.push_back(
            std::move(*r.mutable_pending_web_feed_operation()));
      } else {
        DLOG(ERROR) << "OnReadWebFeedStartupDataFinished: Got record with no "
                       "useful data. data_case="
                    << static_cast<int>(r.data_case());
      }
    }
  }
  std::move(callback).Run(std::move(result));
}

void FeedStore::ReadStartupData(
    base::OnceCallback<void(StartupData)> callback) {
  if (!IsInitialized()) {
    OnReadStartupDataFinished(std::move(callback), false, nullptr);
    return;
  }
  const base::flat_set<std::string>& key_set = {
      StreamDataKey(StreamType(StreamKind::kFollowing)),
      StreamDataKey(StreamType(StreamKind::kForYou)), kMetadataKey};

  auto is_startup_data_filter = [](const base::flat_set<std::string>& key_set,
                                   const std::string& key) {
    return key_set.contains(key) ||
           base::StartsWith(key, kkSingleWebFeedStreamDataPrefix);
  };

  database_->LoadEntriesWithFilter(
      base::BindRepeating(is_startup_data_filter, std::move(key_set)),
      base::BindOnce(&FeedStore::OnReadStartupDataFinished, GetWeakPtr(),
                     std::move(callback)));
}

void FeedStore::OnReadStartupDataFinished(
    base::OnceCallback<void(StartupData)> callback,
    bool read_ok,
    std::unique_ptr<std::vector<feedstore::Record>> records) {
  StartupData result;
  if (records) {
    for (feedstore::Record& r : *records) {
      if (r.has_stream_data()) {
        result.stream_data.push_back(std::move(r.stream_data()));
      } else if (r.has_metadata()) {
        result.metadata = base::WrapUnique(r.release_metadata());
      } else {
        DLOG(ERROR) << "OnReadStartupDataFinished: Got record with no "
                       "useful data. data_case="
                    << static_cast<int>(r.data_case());
      }
    }
  }
  std::move(callback).Run(std::move(result));
}

void FeedStore::WriteRecommendedFeeds(
    feedstore::RecommendedWebFeedIndex index,
    std::vector<feedstore::WebFeedInfo> web_feed_info,
    base::OnceClosure callback) {
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<feedstore::Record>::KeyEntryVector>();
  entries_to_save->push_back(MakeKeyAndRecord(std::move(index)));
  for (auto& info : web_feed_info) {
    entries_to_save->push_back(MakeKeyAndRecord(std::move(info)));
  }

  auto remove_record = [](const std::string& key) {
    return key.size() > 1 && key[1] == '/' && key[0] == 'R';
  };
  database_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                           base::BindRepeating(remove_record),
                                           DropBoolParam(std::move(callback)));
}

void FeedStore::WriteSubscribedFeeds(feedstore::SubscribedWebFeeds index,
                                     base::OnceClosure callback) {
  Write({MakeRecord(index)}, DropBoolParam(std::move(callback)));
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

void FeedStore::UpgradeFromStreamSchemaV0(
    feedstore::Metadata metadata,
    base::OnceCallback<void(feedstore::Metadata)> callback) {
  // Migration does two things:
  // 1. Record a new metadata with the new schema version.
  // 2. Delete all stream data.
  metadata.set_stream_schema_version(kCurrentStreamSchemaVersion);

  auto updates = std::make_unique<
      std::vector<std::pair<std::string, feedstore::Record>>>();
  updates->emplace_back(kMetadataKey, MakeRecord(metadata));

  database_->UpdateEntriesWithRemoveFilter(
      std::move(updates), base::BindRepeating(&IsAnyStreamRecordKey),
      DropBoolParam(base::BindOnce(std::move(callback), std::move(metadata))));
}

void FeedStore::ReadRecommendedWebFeedInfo(
    const std::string& web_feed_id,
    base::OnceCallback<void(std::unique_ptr<feedstore::WebFeedInfo>)>
        callback) {
  ReadSingle("R/" + web_feed_id,
             base::BindOnce(&FeedStore::ReadRecommendedWebFeedInfoFinished,
                            GetWeakPtr(), std::move(callback)));
}

void FeedStore::ReadRecommendedWebFeedInfoFinished(
    base::OnceCallback<void(std::unique_ptr<feedstore::WebFeedInfo>)> callback,
    bool read_ok,
    std::unique_ptr<feedstore::Record> record) {
  if (!record || !read_ok) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(
      base::WrapUnique(record->release_recommended_web_feed()));
}

void FeedStore::WritePendingWebFeedOperation(
    feedstore::PendingWebFeedOperation operation) {
  Write({MakeRecord(std::move(operation))}, base::DoNothing());
}

void FeedStore::RemovePendingWebFeedOperation(int64_t operation_id) {
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  keys_to_remove->push_back(base::StrCat(
      {kPendingWebFeedOperationPrefix, base::NumberToString(operation_id)}));

  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      std::move(keys_to_remove), base::DoNothing());
}

void FeedStore::WriteDocView(feedstore::DocView doc_view) {
  std::vector<feedstore::Record> records;
  records.push_back(MakeRecord(std::move(doc_view)));
  Write(std::move(records), base::DoNothing());
}

void FeedStore::RemoveDocViews(std::vector<feedstore::DocView> doc_views) {
  if (doc_views.empty()) {
    return;
  }
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  for (const feedstore::DocView& doc_view : doc_views) {
    keys_to_remove->push_back(DocViewKey(doc_view));
  }
  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          std::vector<std::pair<std::string, feedstore::Record>>>(),
      std::move(keys_to_remove), base::DoNothing());
}

void FeedStore::ReadDocViews(
    base::OnceCallback<void(std::vector<feedstore::DocView>)> callback) {
  auto adapter =
      [](base::OnceCallback<void(std::vector<feedstore::DocView>)> callback,
         bool ok,
         std::unique_ptr<std::map<std::string, feedstore::Record>> results) {
        std::vector<feedstore::DocView> doc_views;
        for (auto& entry : *results) {
          feedstore::Record& record = entry.second;
          doc_views.push_back(std::move(record.doc_view()));
        }
        std::move(callback).Run(std::move(doc_views));
      };
  database_->LoadKeysAndEntriesInRange(
      "v/0", "v/~", base::BindOnce(adapter, std::move(callback)));
}

}  // namespace feed
