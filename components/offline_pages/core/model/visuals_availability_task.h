// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_VISUALS_AVAILABILITY_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_VISUALS_AVAILABILITY_TASK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class OfflinePageMetadataStore;

// Checks if a thumbnail exists for the specified offline id.
class VisualsAvailabilityTask : public Task {
 public:
  using VisualsAvailableCallback =
      base::OnceCallback<void(VisualsAvailability)>;

  VisualsAvailabilityTask(OfflinePageMetadataStore* store,
                          int64_t offline_id,
                          VisualsAvailableCallback exists_callback);
  ~VisualsAvailabilityTask() override;

 private:
  // Task implementation:
  void Run() override;

  void OnVisualsAvailable(VisualsAvailability availability);

  OfflinePageMetadataStore* store_;
  int64_t offline_id_;
  VisualsAvailableCallback exists_callback_;
  base::WeakPtrFactory<VisualsAvailabilityTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(VisualsAvailabilityTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_VISUALS_AVAILABILITY_TASK_H_
