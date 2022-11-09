// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_STREAM_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_STREAM_TASK_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;

// Clears stream local Feed data for the given stream type.
// 1. Clear store.
// 2. Trigger reload if surfaces are attached.
class ClearStreamTask : public offline_pages::Task {
 public:
  explicit ClearStreamTask(FeedStream* stream, const StreamType& stream_type);
  ~ClearStreamTask() override;
  ClearStreamTask(const ClearStreamTask&) = delete;
  ClearStreamTask& operator=(const ClearStreamTask&) = delete;

 private:
  base::WeakPtr<ClearStreamTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // offline_pages::Task.
  void Run() override;

  void StoreClearComplete(bool ok);

  const raw_ref<FeedStream> stream_;
  StreamType stream_type_;
  base::WeakPtrFactory<ClearStreamTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_STREAM_TASK_H_
