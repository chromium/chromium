// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/offline_pages/task/task.h"
#include "components/version_info/channel.h"

namespace feed {
class FeedStream;
class LaunchReliabilityLogger;

// Fetches additional content from the network when the model is already loaded.
// Unlike |LoadStreamTask|, this task does not directly persist data to
// |FeedStore|. Instead, |StreamModel| handles persisting the additional
// content.
class LoadMoreTask : public offline_pages::Task {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    StreamType stream_type;
    // Final status of loading the stream.
    LoadStreamStatus final_status = LoadStreamStatus::kNoStatus;
    bool loaded_new_content_from_network = false;
    std::optional<RequestSchedule> request_schedule;
    std::unique_ptr<StreamModelUpdateRequest> model_update_request;
  };

  LoadMoreTask(const StreamType& stream_type,
               FeedStream* stream,
               base::OnceCallback<void(Result)> done_callback);
  ~LoadMoreTask() override;
  LoadMoreTask(const LoadMoreTask&) = delete;
  LoadMoreTask& operator=(const LoadMoreTask&) = delete;

 private:
  void Run() override;
  base::WeakPtr<LoadMoreTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void UploadActionsComplete(UploadActionsTask::Result result);
  void QueryApiRequestComplete(
      FeedNetwork::ApiResult<feedwire::Response> result);
  void QueryRequestComplete(FeedNetwork::QueryRequestResult result);
  void ProcessNetworkResponse(std::unique_ptr<feedwire::Response> response_body,
                              NetworkResponseInfo response_info);
  void RequestFinished(LoadStreamStatus status,
                       int network_status_code,
                       int64_t server_receive_timestamp_ns,
                       int64_t server_send_timestamp_ns);
  void Done(LoadStreamStatus status);

  LaunchReliabilityLogger& GetLaunchReliabilityLogger() const;

  StreamType stream_type_;
  const raw_ref<FeedStream> stream_;  // Unowned.
  base::TimeTicks fetch_start_time_;
  std::unique_ptr<UploadActionsTask> upload_actions_task_;

  Result result_;

  base::OnceCallback<void(Result)> done_callback_;
  base::WeakPtrFactory<LoadMoreTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_
