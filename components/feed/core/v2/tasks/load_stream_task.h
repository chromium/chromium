// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/surface_updater.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/view_demotion.h"
#include "components/offline_pages/task/task.h"
#include "components/version_info/channel.h"

namespace feed {
class FeedStream;
class LaunchReliabilityLogger;
struct DocViewDigest;

// Loads the stream model from storage or network. If data is refreshed from the
// network, it is persisted to |FeedStore| by overwriting any existing stream
// data.
// This task has three modes, see |LoadType| in enums.h.
class LoadStreamTask : public offline_pages::Task {
 public:
  // Returns the `LaunchResult` that contains the terminal failure result if the
  // parameters do not represent a successful Feed response. Returns a
  // `load_stream_status` of `LoadStreamStatus::kNoStatus` if there was no
  // failure.
  static LaunchResult LaunchResultFromNetworkInfo(
      const NetworkResponseInfo& network_response_info,
      bool has_parsed_body);

  struct Options {
    // The stream type to load.
    StreamType stream_type;
    LoadType load_type = LoadType::kInitialLoad;
    // Abort the background refresh if there's already unread content.
    bool abort_if_unread_content = false;
    bool refresh_even_when_not_stale = false;
    // The Entry point for a singlewebfeed stream
    SingleWebFeedEntryPoint single_feed_entry_point =
        SingleWebFeedEntryPoint::kOther;
  };

  struct Result {
    Result();
    Result(const StreamType& stream_type, LoadStreamStatus status);
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);
    StreamType stream_type;
    // Final status of loading the stream.
    LoadStreamStatus final_status = LoadStreamStatus::kNoStatus;
    // Status of just loading the stream from the persistent store, if that
    // was attempted.
    LoadStreamStatus load_from_store_status = LoadStreamStatus::kNoStatus;
    // Age of content loaded from local storage. Zero if none was loaded.
    base::TimeDelta stored_content_age;
    // Set of content IDs present in the feed.
    ContentHashSet content_ids;
    LoadType load_type = LoadType::kInitialLoad;
    std::unique_ptr<StreamModelUpdateRequest> update_request;
    std::optional<RequestSchedule> request_schedule;

    // Information about the network request, if one was made.
    std::optional<NetworkResponseInfo> network_response_info;
    bool loaded_new_content_from_network = false;
    std::unique_ptr<LoadLatencyTimes> latencies;

    // Result of the upload actions task.
    std::unique_ptr<UploadActionsTask::Result> upload_actions_result;

    // Experiments information from the server.
    Experiments experiments;

    // Reliability logging feed launch result: CARDS_UNSPECIFIED if loading is
    // successful.
    feedwire::DiscoverLaunchResult launch_result;

    // The entry point for a Single Web Feed.
    SingleWebFeedEntryPoint single_feed_entry_point =
        SingleWebFeedEntryPoint::kOther;
  };

  LoadStreamTask(const Options& options,
                 FeedStream* stream,
                 base::OnceCallback<void(Result)> done_callback);
  ~LoadStreamTask() override;
  LoadStreamTask(const LoadStreamTask&) = delete;
  LoadStreamTask& operator=(const LoadStreamTask&) = delete;

 private:
  void Run() override;
  base::WeakPtr<LoadStreamTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void CheckIfSubscriberComplete(bool is_web_feed_subscriber);
  void ResumeAtStart();
  bool CheckPreconditions();
  void PassedPreconditions();

  void UploadActions(
      std::vector<feedstore::StoredAction> pending_actions_from_store);
  void SendFeedQueryRequest();

  void LoadFromNetwork1(
      std::vector<feedstore::StoredAction> pending_actions_from_store,
      bool need_to_read_pending_actions);
  void LoadFromNetwork2(
      std::vector<feedstore::StoredAction> pending_actions_from_store,
      bool need_to_read_pending_actions,
      DocViewDigest doc_view_digest);
  void LoadFromStoreComplete(LoadStreamFromStoreTask::Result result);
  void UploadActionsComplete(UploadActionsTask::Result result);
  void QueryApiRequestComplete(
      FeedNetwork::ApiResult<feedwire::Response> result);
  void QueryRequestComplete(FeedNetwork::QueryRequestResult result);
  template <typename Response>
  void ProcessNetworkResponse(std::unique_ptr<Response> response,
                              NetworkResponseInfo response_info);
  void RequestFinished(LaunchResult result);
  void Done(LaunchResult result);

  LaunchReliabilityLogger& GetLaunchReliabilityLogger() const;

  Options options_;
  const raw_ref<FeedStream> stream_;  // Unowned.
  std::unique_ptr<LoadStreamFromStoreTask> load_from_store_task_;
  std::unique_ptr<StreamModelUpdateRequest> stale_store_state_;

  std::vector<DocViewCount> doc_view_counts_;

  // Information to be stuffed in |Result|.
  LoadStreamStatus load_from_store_status_ = LoadStreamStatus::kNoStatus;
  std::optional<NetworkResponseInfo> network_response_info_;
  bool loaded_new_content_from_network_ = false;
  base::TimeDelta stored_content_age_;
  ContentHashSet content_ids_;
  Experiments experiments_;
  std::unique_ptr<StreamModelUpdateRequest> update_request_;
  std::optional<RequestSchedule> request_schedule_;
  NetworkRequestId network_request_id_;
  base::TimeTicks response_received_timestamp_;

  std::unique_ptr<LoadLatencyTimes> latencies_;
  base::TimeTicks task_creation_time_;
  base::TimeTicks fetch_start_time_;
  base::OnceCallback<void(Result)> done_callback_;
  std::unique_ptr<UploadActionsTask> upload_actions_task_;
  std::unique_ptr<UploadActionsTask::Result> upload_actions_result_;
  int64_t server_receive_timestamp_ns_ = 0l;
  int64_t server_send_timestamp_ns_ = 0l;
  bool is_web_feed_subscriber_ = false;
  base::WeakPtrFactory<LoadStreamTask> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os, const LoadStreamTask::Result&);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
