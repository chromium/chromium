// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/offline_pages/task/task.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feed {
class FeedStream;

// Loads the stream model from storage or network. If data is refreshed from the
// network, it is persisted to |FeedStore| by overwriting any existing stream
// data.
// This task has two modes, see |LoadStreamTask::LoadType|.
class LoadStreamTask : public offline_pages::Task {
 public:
  enum class LoadType {
    // Loads the stream model into memory. If successful, this directly forces a
    // model load in |FeedStream()| before completing the task.
    kInitialLoad,
    // Refreshes the stored stream data from the network. This will fail if the
    // model is already loaded.
    kBackgroundRefresh,
  };

  struct Options {
    // The stream type to load.
    StreamType stream_type;
    LoadType load_type = LoadType::kInitialLoad;
    // Abort the background refresh if there's already unread content.
    bool abort_if_unread_content = false;
    bool refresh_even_when_not_stale = false;
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
    ContentIdSet content_ids;
    LoadType load_type;
    std::unique_ptr<StreamModelUpdateRequest> update_request;
    absl::optional<RequestSchedule> request_schedule;

    // Information about the network request, if one was made.
    absl::optional<NetworkResponseInfo> network_response_info;
    bool loaded_new_content_from_network = false;
    std::unique_ptr<LoadLatencyTimes> latencies;
    absl::optional<bool> fetched_content_has_notice_card;

    // Result of the upload actions task.
    std::unique_ptr<UploadActionsTask::Result> upload_actions_result;

    // Experiments information from the server.
    Experiments experiments;
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

  void LoadFromStoreComplete(LoadStreamFromStoreTask::Result result);
  void UploadActionsComplete(UploadActionsTask::Result result);
  void QueryApiRequestComplete(
      FeedNetwork::ApiResult<feedwire::Response> result);
  void QueryRequestComplete(FeedNetwork::QueryRequestResult result);
  void ProcessNetworkResponse(std::unique_ptr<feedwire::Response> response,
                              NetworkResponseInfo response_info);
  void Done(LoadStreamStatus status);

  Options options_;
  FeedStream* stream_;  // Unowned.
  std::unique_ptr<LoadStreamFromStoreTask> load_from_store_task_;
  std::unique_ptr<StreamModelUpdateRequest> stale_store_state_;

  // Information to be stuffed in |Result|.
  LoadStreamStatus load_from_store_status_ = LoadStreamStatus::kNoStatus;
  absl::optional<NetworkResponseInfo> network_response_info_;
  bool loaded_new_content_from_network_ = false;
  base::TimeDelta stored_content_age_;
  ContentIdSet content_ids_;
  Experiments experiments_;
  std::unique_ptr<StreamModelUpdateRequest> update_request_;
  absl::optional<RequestSchedule> request_schedule_;

  std::unique_ptr<LoadLatencyTimes> latencies_;
  base::TimeTicks task_creation_time_;
  base::TimeTicks fetch_start_time_;
  base::OnceCallback<void(Result)> done_callback_;
  std::unique_ptr<UploadActionsTask> upload_actions_task_;
  std::unique_ptr<UploadActionsTask::Result> upload_actions_result_;
  absl::optional<bool> fetched_content_has_notice_card_;
  base::WeakPtrFactory<LoadStreamTask> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os, const LoadStreamTask::Result&);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
