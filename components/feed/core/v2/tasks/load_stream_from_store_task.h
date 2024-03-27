// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/types.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;
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

    // The fields below are provided for all `LoadType`s.

    // Pending actions to be uploaded if the stream is to be loaded from the
    // network.
    std::vector<feedstore::StoredAction> pending_actions;
    // How long since the loaded content was fetched from the server.
    // May be zero if content is not loaded.
    base::TimeDelta content_age;
    ContentHashSet content_ids;

    // Loading result to be logged by
    // LaunchReliabilityLogger::LogCacheReadEnd().
    feedwire::DiscoverCardReadCacheResult reliability_result;
  };

  // Determines what kind of data is loaded. See `Result` for what is loaded.
  enum class LoadType {
    // Load the full stream content.
    kFullLoad = 0,
    // Skips loading stream content.
    kLoadNoContent = 1,
  };

  // TODO(crbug.com/40943733):`feed_stream` may only be null in tests, which set
  // both `IgnoreStalenessForTesting` and `IgnoreAccountForTesting`. Ideally
  // tests would reflect production code and use a non-null pointer.
  LoadStreamFromStoreTask(LoadType load_type,
                          FeedStream* feed_stream,
                          const StreamType& stream_type,
                          FeedStore* store,
                          bool missed_last_refresh,
                          bool is_web_feed_subscriber,
                          base::OnceCallback<void(Result)> callback);
  ~LoadStreamFromStoreTask() override;
  LoadStreamFromStoreTask(const LoadStreamFromStoreTask&) = delete;
  LoadStreamFromStoreTask& operator=(const LoadStreamFromStoreTask&) = delete;

  void IgnoreStalenessForTesting() { ignore_staleness_ = true; }
  void IgnoreAccountForTesting() { ignore_account_ = true; }

 private:
  void Run() override;
  void LoadStreamDone(FeedStore::LoadStreamResult);
  void LoadContentDone(std::vector<feedstore::Content> content,
                       std::vector<feedstore::StreamSharedState> shared_states);
  void Complete(LoadStreamStatus status,
                feedwire::DiscoverCardReadCacheResult reliability_result);

  base::WeakPtr<LoadStreamFromStoreTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  LoadStreamStatus stale_reason_ = LoadStreamStatus::kNoStatus;
  LoadType load_type_;
  const raw_ptr<FeedStream> feed_stream_;
  StreamType stream_type_;
  raw_ptr<FeedStore> store_;  // Unowned.
  bool ignore_staleness_ = false;
  bool missed_last_refresh_ = false;
  bool ignore_account_ = false;
  bool is_web_feed_subscriber_ = false;
  base::OnceCallback<void(Result)> result_callback_;

  // Data to be stuffed into the Result when the task is complete.
  std::unique_ptr<StreamModelUpdateRequest> update_request_;
  std::vector<feedstore::StoredAction> pending_actions_;
  base::TimeDelta content_age_;
  ContentHashSet content_ids_;

  base::WeakPtrFactory<LoadStreamFromStoreTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
