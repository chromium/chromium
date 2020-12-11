// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_GET_PREFETCH_SUGGESTIONS_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_GET_PREFETCH_SUGGESTIONS_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
struct PrefetchSuggestion;
}

namespace feed {
class FeedStream;
class StreamModel;

// Get the list of prefetch suggestions.
class GetPrefetchSuggestionsTask : public offline_pages::Task {
 public:
  explicit GetPrefetchSuggestionsTask(
      FeedStream* stream,
      base::OnceCallback<void(std::vector<offline_pages::PrefetchSuggestion>)>
          result_callback);
  ~GetPrefetchSuggestionsTask() override;
  GetPrefetchSuggestionsTask(const GetPrefetchSuggestionsTask&) = delete;
  GetPrefetchSuggestionsTask& operator=(const GetPrefetchSuggestionsTask&) =
      delete;

 private:
  base::WeakPtr<GetPrefetchSuggestionsTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // offline_pages::Task.
  void Run() override;

  void LoadStreamComplete(LoadStreamFromStoreTask::Result result);

  void PullSuggestionsFromModel(const StreamModel& model);

  FeedStream* stream_;
  base::OnceCallback<void(std::vector<offline_pages::PrefetchSuggestion>)>
      result_callback_;

  std::unique_ptr<LoadStreamFromStoreTask> load_from_store_task_;

  base::WeakPtrFactory<GetPrefetchSuggestionsTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_GET_PREFETCH_SUGGESTIONS_TASK_H_
