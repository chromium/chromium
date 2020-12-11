// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;
class FeedStore;

// Initializes |store|. This task is run first so that other tasks can assume
// storage is initialized.
class WaitForStoreInitializeTask : public offline_pages::Task {
 public:
  explicit WaitForStoreInitializeTask(FeedStream* stream);
  ~WaitForStoreInitializeTask() override;
  WaitForStoreInitializeTask(const WaitForStoreInitializeTask&) = delete;
  WaitForStoreInitializeTask& operator=(const WaitForStoreInitializeTask&) =
      delete;

 private:
  void Run() override;

  void OnStoreInitialized();
  void OnMetadataLoaded(std::unique_ptr<feedstore::Metadata> metadata);

  FeedStream* stream_;
  FeedStore* store_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_WAIT_FOR_STORE_INITIALIZE_TASK_H_
