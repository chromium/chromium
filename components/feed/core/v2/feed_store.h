// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/types.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {
struct StreamModelUpdateRequest;

// Reads and writes data to persistent storage. See
// components/feed/core/proto/v2/store.proto for the schema. Note that FeedStore
// automatically populates all stream_id fields for storage protos. This ensures
// that database keys are consistent with stored messages.
class FeedStore {
 public:
  static constexpr int kCurrentStreamSchemaVersion = 1;
  struct LoadStreamResult {
    LoadStreamResult();
    ~LoadStreamResult();
    LoadStreamResult(LoadStreamResult&&);
    LoadStreamResult& operator=(LoadStreamResult&&);

    bool read_error = false;
    StreamType stream_type;
    feedstore::StreamData stream_data;
    std::vector<feedstore::StreamStructureSet> stream_structures;
    // These are sorted by increasing ID.
    std::vector<feedstore::StoredAction> pending_actions;
  };
  struct StartupData {
    StartupData();
    StartupData(StartupData&&);
    ~StartupData();
    StartupData& operator=(StartupData&&);

    std::unique_ptr<feedstore::Metadata> metadata;
    std::vector<feedstore::StreamData> stream_data;
  };
  struct WebFeedStartupData {
    WebFeedStartupData();
    WebFeedStartupData(WebFeedStartupData&&);
    ~WebFeedStartupData();
    WebFeedStartupData& operator=(WebFeedStartupData&&);

    feedstore::SubscribedWebFeeds subscribed_web_feeds;
    feedstore::RecommendedWebFeedIndex recommended_feed_index;
    std::vector<feedstore::PendingWebFeedOperation> pending_operations;
  };

  explicit FeedStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>>
          database);
  ~FeedStore();
  FeedStore(const FeedStore&) = delete;
  FeedStore& operator=(const FeedStore&) = delete;

  void Initialize(base::OnceClosure initialize_complete);

  // Erase all data in the store.
  void ClearAll(base::OnceCallback<void(bool)> callback);

  void LoadStream(const StreamType& stream_type,
                  base::OnceCallback<void(LoadStreamResult)> callback);

  // Stores the content of |update_request| in place of any existing stream
  // data.
  void OverwriteStream(const StreamType& stream_type,
                       std::unique_ptr<StreamModelUpdateRequest> update_request,
                       base::OnceCallback<void(bool)> callback);

  // Stores the content of |update_request| as an update to existing stream
  // data.
  void SaveStreamUpdate(
      const StreamType& stream_type,
      int32_t structure_set_sequence_number,
      std::unique_ptr<StreamModelUpdateRequest> update_request,
      base::OnceCallback<void(bool)> callback);

  void ClearStreamData(const StreamType& stream_type,
                       base::OnceCallback<void(bool)> callback);

  void ClearAllStreamData(StreamKind stream_kind,
                          base::OnceCallback<void(bool)> callback);
  void WriteOperations(const StreamType& stream_type,
                       int32_t sequence_number,
                       std::vector<feedstore::DataOperation> operations);

  // Read StreamData and pass it to stream_data_callback, or nullptr on failure.
  void ReadStreamData(
      base::OnceCallback<void(std::unique_ptr<feedstore::StreamData>)>
          stream_data_callback);

  // Read Content and StreamSharedStates and pass them to content_callback, or
  // nullptrs on failure.
  void ReadContent(
      const StreamType& stream_type,
      std::vector<feedwire::ContentId> content_ids,
      std::vector<feedwire::ContentId> shared_state_ids,
      base::OnceCallback<void(std::vector<feedstore::Content>,
                              std::vector<feedstore::StreamSharedState>)>
          content_callback);

  void ReadActions(
      base::OnceCallback<void(std::vector<feedstore::StoredAction>)> callback);
  void WriteActions(std::vector<feedstore::StoredAction> actions,
                    base::OnceCallback<void(bool)> callback);
  void UpdateActions(std::vector<feedstore::StoredAction> actions_to_update,
                     std::vector<LocalActionId> ids_to_remove,
                     base::OnceCallback<void(bool)> callback);
  void RemoveActions(std::vector<LocalActionId> ids,
                     base::OnceCallback<void(bool)> callback);

  void ReadMetadata(
      base::OnceCallback<void(std::unique_ptr<feedstore::Metadata>)> callback);
  void WriteMetadata(feedstore::Metadata metadata,
                     base::OnceCallback<void(bool)> callback);
  void UpgradeFromStreamSchemaV0(
      feedstore::Metadata old_metadata,
      base::OnceCallback<void(feedstore::Metadata)> callback);
  void ReadWebFeedStartupData(
      base::OnceCallback<void(WebFeedStartupData)> callback);
  void ReadStartupData(base::OnceCallback<void(StartupData)> callback);
  void WriteRecommendedFeeds(feedstore::RecommendedWebFeedIndex index,
                             std::vector<feedstore::WebFeedInfo> web_feed_info,
                             base::OnceClosure callback);
  void WriteSubscribedFeeds(feedstore::SubscribedWebFeeds index,
                            base::OnceClosure callback);
  void ReadRecommendedWebFeedInfo(
      const std::string& web_feed_id,
      base::OnceCallback<void(std::unique_ptr<feedstore::WebFeedInfo>)>
          callback);
  void ReadAllPendingWebFeedOperations(
      base::OnceCallback<
          void(std::vector<feedstore::PendingWebFeedOperation>)>);
  void RemovePendingWebFeedOperation(int64_t operation_id);
  void WritePendingWebFeedOperation(
      feedstore::PendingWebFeedOperation operation);

  void WriteDocView(feedstore::DocView doc_view);
  void RemoveDocViews(std::vector<feedstore::DocView> doc_ids);
  void ReadDocViews(
      base::OnceCallback<void(std::vector<feedstore::DocView>)> callback);

  bool IsInitializedForTesting() const;

  leveldb_proto::ProtoDatabase<feedstore::Record>* GetDatabaseForTesting() {
    return database_.get();
  }

  base::WeakPtr<FeedStore> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  bool IsInitialized() const;
  // Overwrites all stream data with |updates|.
  void UpdateFullStreamData(
      const StreamType& stream_type,
      std::unique_ptr<std::vector<std::pair<std::string, feedstore::Record>>>
          updates,
      base::OnceCallback<void(bool)> callback);

  void Write(std::vector<feedstore::Record> records,
             base::OnceCallback<void(bool)> callback);
  void ReadSingle(
      const std::string& key,
      base::OnceCallback<void(bool, std::unique_ptr<feedstore::Record>)>
          callback);
  void ReadMany(const base::flat_set<std::string>& key_set,
                base::OnceCallback<
                    void(bool, std::unique_ptr<std::vector<feedstore::Record>>)>
                    callback);
  void OnSaveStreamEntriesUpdated(
      base::OnceCallback<void(bool)> complete_callback,
      bool ok);
  void OnLoadStreamFinished(
      const StreamType& stream_type,
      base::OnceCallback<void(LoadStreamResult)> callback,
      bool success,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnReadContentFinished(
      base::OnceCallback<void(std::vector<feedstore::Content>,
                              std::vector<feedstore::StreamSharedState>)>
          callback,
      bool success,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnReadActionsFinished(
      base::OnceCallback<void(std::vector<feedstore::StoredAction>)> callback,
      bool success,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnWriteFinished(base::OnceCallback<void(bool)> callback, bool success);
  void OnReadMetadataFinished(
      base::OnceCallback<void(std::unique_ptr<feedstore::Metadata>)> callback,
      bool read_ok,
      std::unique_ptr<feedstore::Record> record);
  void OnReadWebFeedStartupDataFinished(
      base::OnceCallback<void(WebFeedStartupData)> callback,
      bool read_ok,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void OnReadStartupDataFinished(
      base::OnceCallback<void(StartupData)> callback,
      bool read_ok,
      std::unique_ptr<std::vector<feedstore::Record>> records);
  void ReadRecommendedWebFeedInfoFinished(
      base::OnceCallback<void(std::unique_ptr<feedstore::WebFeedInfo>)>
          callback,
      bool read_ok,
      std::unique_ptr<feedstore::Record> record);

  base::OnceClosure initialize_callback_;
  leveldb_proto::Enums::InitStatus database_status_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database_;
  base::WeakPtrFactory<FeedStore> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STORE_H_
