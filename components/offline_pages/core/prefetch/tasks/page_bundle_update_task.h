// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_PAGE_BUNDLE_UPDATE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_PAGE_BUNDLE_UPDATE_TASK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;

// Task that writes down the result of a prefetch networking operation and
// optionally schedules action tasks.  Results in the form of |operation_name|
// and vector of |RenderPageInfo| are provided in both GetOperation and
// GeneratePageBundle responses.
//
// This task does 3 things:
// - Any pages in the |pages| list that have succeeded are updated to
//   RECEIVED_BUNDLE and body name / length are updated.
// - Any pages in the |pages| list that failed are updated to FINISHED
//   with the ARCHIVING_FAILED error code.
// - Any pages in the result that are "Pending" and match rows with an
//   empty operation name will update to AWAITING_GCM and save the
//   operation name.
// - Pages in the SENT_GENERATE_PAGE_BUNDLE or SENT_GET_OPERATION state but that
//   are not reflected in the |pages| list are ignored, and will be cleaned up
//   by reconcilers if necessary.
class PageBundleUpdateTask : public Task {
 public:
  // The result is whether we need more action tasks to run right now.
  using PageBundleUpdateResult = bool;

  PageBundleUpdateTask(PrefetchStore* store,
                       PrefetchDispatcher* dispatcher,
                       const std::string& operation_name,
                       const std::vector<RenderPageInfo>& pages);
  ~PageBundleUpdateTask() override;

  // Task implementation.
  void Run() override;

 private:
  void FinishedWork(PageBundleUpdateResult result);

  // Owned by PrefetchService which also transitively owns |this|, so raw
  // pointer is OK.
  PrefetchStore* store_;
  // PrefetchDispatcher owns the task queue which owns |this|, so raw pointer is
  // OK.
  PrefetchDispatcher* dispatcher_;
  std::string operation_name_;
  std::vector<RenderPageInfo> pages_;

  base::WeakPtrFactory<PageBundleUpdateTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PageBundleUpdateTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_PAGE_BUNDLE_UPDATE_TASK_H_
