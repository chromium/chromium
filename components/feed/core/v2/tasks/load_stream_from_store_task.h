// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/offline_pages/task/task.h"

namespace feed {
struct StreamModelUpdateRequest;

// Attempts to load stream data from persistent storage.
class LoadStreamFromStoreTask : public offline_pages::Task {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);
    LoadStreamStatus status = LoadStreamStatus::kNoStatus;
    // Only provided if using |LoadType::kFullLoad| AND successful.
    std::unique_ptr<StreamModelUpdateRequest> update_request;
    // This data is provided when |LoadType::kPendingActionsOnly|, or
    // when loading fails.
    std::string consistency_token;
    // Pending actions to be uploaded if the stream is to be loaded from the
    // network.
    std::vector<feedstore::StoredAction> pending_actions;
    // How long since the loaded content was fetched from the server.
    // May be zero if content is not loaded.
    base::TimeDelta content_age;
  };

  enum class LoadType {
    kFullLoad = 0,
    kPendingActionsOnly = 1,
  };

  LoadStreamFromStoreTask(LoadType load_type,
                          const StreamType& stream_type,
                          FeedStore* store,
                          bool missed_last_refresh,
                          base::OnceCallback<void(Result)> callback);
  ~LoadStreamFromStoreTask() override;
  LoadStreamFromStoreTask(const LoadStreamFromStoreTask&) = delete;
  LoadStreamFromStoreTask& operator=(const LoadStreamFromStoreTask&) = delete;

  void IgnoreStalenessForTesting() { ignore_staleness_ = true; }

 private:
  void Run() override;

  void LoadStreamDone(FeedStore::LoadStreamResult);
  void LoadContentDone(std::vector<feedstore::Content> content,
                       std::vector<feedstore::StreamSharedState> shared_states);
  void Complete(LoadStreamStatus status);

  base::WeakPtr<LoadStreamFromStoreTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  LoadStreamStatus stale_reason_ = LoadStreamStatus::kNoStatus;
  LoadType load_type_;
  StreamType stream_type_;
  FeedStore* store_;  // Unowned.
  bool ignore_staleness_ = false;
  bool missed_last_refresh_ = false;
  base::OnceCallback<void(Result)> result_callback_;

  // Data to be stuffed into the Result when the task is complete.
  std::unique_ptr<StreamModelUpdateRequest> update_request_;
  std::vector<feedstore::StoredAction> pending_actions_;
  base::TimeDelta content_age_;

  base::WeakPtrFactory<LoadStreamFromStoreTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
