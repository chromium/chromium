// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class OfflinePageMetadataStore;

// Task that marks a page accessed in the metadata store. It takes the offline
// ID of the page accessed, and the time when it was accessed.
// There is no callback needed for this task.
class MarkPageAccessedTask : public Task {
 public:
  MarkPageAccessedTask(OfflinePageMetadataStore* store,
                       int64_t offline_id,
                       const base::Time& access_time);

  MarkPageAccessedTask(const MarkPageAccessedTask&) = delete;
  MarkPageAccessedTask& operator=(const MarkPageAccessedTask&) = delete;

  ~MarkPageAccessedTask() override;

 private:
  // Task implementation.
  void Run() override;

  void OnMarkPageAccessedDone(bool result);

  // The metadata store used to update the page. Not owned.
  raw_ptr<OfflinePageMetadataStore> store_;

  int64_t offline_id_;
  base::Time access_time_;

  base::WeakPtrFactory<MarkPageAccessedTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_
