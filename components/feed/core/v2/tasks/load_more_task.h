// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/offline_pages/task/task.h"
#include "components/version_info/channel.h"

namespace feed {
class FeedStream;

// Fetches additional content from the network when the model is already loaded.
// Unlike |LoadStreamTask|, this task does not directly persist data to
// |FeedStore|. Instead, |StreamModel| handles persisting the additional
// content.
class LoadMoreTask : public offline_pages::Task {
 public:
  struct Result {
    // Final status of loading the stream.
    LoadStreamStatus final_status = LoadStreamStatus::kNoStatus;
    bool loaded_new_content_from_network = false;
  };

  LoadMoreTask(FeedStream* stream,
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
  void QueryRequestComplete(bool was_signed_in_request,
                            FeedNetwork::QueryRequestResult result);
  void Done(LoadStreamStatus status);

  FeedStream* stream_;  // Unowned.
  base::TimeTicks fetch_start_time_;
  std::unique_ptr<UploadActionsTask> upload_actions_task_;

  bool loaded_new_content_from_network_ = false;

  base::OnceCallback<void(Result)> done_callback_;
  base::WeakPtrFactory<LoadMoreTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_MORE_TASK_H_
