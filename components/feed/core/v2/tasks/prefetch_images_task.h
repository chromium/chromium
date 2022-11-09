// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_PREFETCH_IMAGES_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_PREFETCH_IMAGES_TASK_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;
class StreamModel;

// Prefetch the images in the model.
class PrefetchImagesTask : public offline_pages::Task {
 public:
  explicit PrefetchImagesTask(FeedStream* stream);
  ~PrefetchImagesTask() override;
  PrefetchImagesTask(const PrefetchImagesTask&) = delete;
  PrefetchImagesTask& operator=(const PrefetchImagesTask&) = delete;

 private:
  base::WeakPtr<PrefetchImagesTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // offline_pages::Task.
  void Run() override;

  void LoadStreamComplete(LoadStreamFromStoreTask::Result result);

  void PrefetchImagesFromModel(const StreamModel& model);

  void MaybePrefetchImage(const GURL& gurl);

  const raw_ref<FeedStream> stream_;
  std::unordered_set<std::string> previously_fetched_;
  unsigned long max_images_per_refresh_;

  std::unique_ptr<LoadStreamFromStoreTask> load_from_store_task_;

  base::WeakPtrFactory<PrefetchImagesTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_PREFETCH_IMAGES_TASK_H_
