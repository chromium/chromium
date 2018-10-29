// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_ADD_UNIQUE_URLS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_ADD_UNIQUE_URLS_TASK_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;
struct PrefetchURL;

// Task that adds new URL suggestions to the pipeline. URLs are matched against
// existing ones from any stage of the process so that only new, unique ones are
// actually added.
// Fully processed items are kept in the store in the PrefetchItemState::ZOMBIE
// state until it is confirmed that the client for its namespace is not
// recommending the same URL anymore to avoid processing it twice. So once the
// step described above is done, all same namespace items in the ZOMBIE state
// whose URL didn't match any of the just suggested ones are finally deleted
// from the store.
class AddUniqueUrlsTask : public Task {
 public:
  // Result of executing the command in the store.
  enum class Result {
    NOTHING_ADDED,
    URLS_ADDED,
    STORE_ERROR,
  };

  AddUniqueUrlsTask(PrefetchDispatcher* prefetch_dispatcher,
                    PrefetchStore* prefetch_store,
                    const std::string& name_space,
                    const std::vector<PrefetchURL>& prefetch_urls);
  ~AddUniqueUrlsTask() override;

  void Run() override;

 private:
  void OnUrlsAdded(Result result);

  // Dispatcher to call back to with results. Not owned.
  PrefetchDispatcher* prefetch_dispatcher_;
  // Prefetch store to execute against. Not owned.
  PrefetchStore* prefetch_store_;
  std::string name_space_;
  std::vector<PrefetchURL> prefetch_urls_;

  base::WeakPtrFactory<AddUniqueUrlsTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AddUniqueUrlsTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_ADD_UNIQUE_URLS_TASK_H_
