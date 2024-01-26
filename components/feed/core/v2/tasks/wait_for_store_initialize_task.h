// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStore;
class FeedStream;

// Initializes |store|. This task is run first so that other tasks can assume
// storage is initialized.
class WaitForStoreInitializeTask : public offline_pages::Task {
 public:
  struct Result {
    FeedStore::StartupData startup_data;
    FeedStore::WebFeedStartupData web_feed_startup_data;
  };

  explicit WaitForStoreInitializeTask(
      FeedStore* store,
      FeedStream* stream,
      base::OnceCallback<void(Result)> callback);
  ~WaitForStoreInitializeTask() override;
  WaitForStoreInitializeTask(const WaitForStoreInitializeTask&) = delete;
  WaitForStoreInitializeTask& operator=(const WaitForStoreInitializeTask&) =
      delete;

 private:
  void Run() override;

  void OnStoreInitialized();
  void OnMetadataLoaded(std::unique_ptr<feedstore::Metadata> metadata);

  void ClearAllDone(bool clear_ok);
  void MaybeUpgradeStreamSchema();
  void UpgradeDone(feedstore::Metadata metadata);
  void ReadStartupDataDone(FeedStore::StartupData startup_data);
  void WebFeedStartupDataDone(FeedStore::WebFeedStartupData data);
  void Done();

  const raw_ref<FeedStore> store_;
  const raw_ref<FeedStream> stream_;
  base::OnceCallback<void(Result)> callback_;
  Result result_;
  int done_count_ = 0;

  base::WeakPtrFactory<WaitForStoreInitializeTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_
