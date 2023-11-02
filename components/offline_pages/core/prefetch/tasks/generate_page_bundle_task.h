// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_TASK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchNetworkRequestFactory;
class PrefetchStore;

// Task that attempts to start archiving the URLs the prefetch service has
// determined are viable to prefetch.
class GeneratePageBundleTask : public Task {
 public:
  struct UrlAndIds;

  GeneratePageBundleTask(PrefetchDispatcher* prefetch_dispatcher,
                         PrefetchStore* prefetch_store,
                         const std::string& gcm_token,
                         PrefetchNetworkRequestFactory* request_factory,
                         PrefetchRequestFinishedCallback callback);

  GeneratePageBundleTask(const GeneratePageBundleTask&) = delete;
  GeneratePageBundleTask& operator=(const GeneratePageBundleTask&) = delete;

  ~GeneratePageBundleTask() override;

 private:
  // Task implementation.
  void Run() override;
  void StartGeneratePageBundle(std::unique_ptr<UrlAndIds> url_and_ids);

  raw_ptr<PrefetchDispatcher> prefetch_dispatcher_;
  raw_ptr<PrefetchStore> prefetch_store_;
  std::string gcm_token_;
  raw_ptr<PrefetchNetworkRequestFactory> request_factory_;
  PrefetchRequestFinishedCallback callback_;

  base::WeakPtrFactory<GeneratePageBundleTask> weak_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_TASK_H_
