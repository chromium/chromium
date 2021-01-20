// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_ALL_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_ALL_TASK_H_

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;

// Clears all local Feed data.
// 1. Unload model.
// 2. Clear store.
// 3. Trigger reload if surfaces are attached.
class ClearAllTask : public offline_pages::Task {
 public:
  explicit ClearAllTask(FeedStream* stream);
  ~ClearAllTask() override;
  ClearAllTask(const ClearAllTask&) = delete;
  ClearAllTask& operator=(const ClearAllTask&) = delete;

 private:
  base::WeakPtr<ClearAllTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // offline_pages::Task.
  void Run() override;

  void StoreClearComplete(bool ok);

  FeedStream* stream_;
  base::WeakPtrFactory<ClearAllTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_CLEAR_ALL_TASK_H_
