// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEANUP_VISUALS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEANUP_VISUALS_TASK_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class OfflinePageMetadataStore;

// CleanupVisualsTask deletes rows from page_thumbnails if they
// are no longer needed.
class CleanupVisualsTask : public Task {
 public:
  struct Result {
    bool success = false;
    int removed_rows = 0;
  };

  CleanupVisualsTask(OfflinePageMetadataStore* store,
                     base::Time now,
                     CleanupVisualsCallback complete_callback);
  ~CleanupVisualsTask() override;

  // Task implementation:
  void Run() override;

 private:
  void Complete(Result result);
  OfflinePageMetadataStore* store_;
  base::Time now_;

  CleanupVisualsCallback complete_callback_;
  base::WeakPtrFactory<CleanupVisualsTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CleanupVisualsTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEANUP_VISUALS_TASK_H_
