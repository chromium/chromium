// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace base {
class Time;
}  // namespace base

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
  ~MarkPageAccessedTask() override;

  // Task implementation.
  void Run() override;

 private:
  void OnMarkPageAccessedDone(bool result);

  // The metadata store used to update the page. Not owned.
  OfflinePageMetadataStore* store_;

  int64_t offline_id_;
  base::Time access_time_;

  base::WeakPtrFactory<MarkPageAccessedTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MarkPageAccessedTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_MARK_PAGE_ACCESSED_TASK_H_
