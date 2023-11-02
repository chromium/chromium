// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_VISUALS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_VISUALS_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class OfflinePageMetadataStore;

// GetVisualsTask reads a thumbnail and favicon from the page_thumbnails
// table.
class GetVisualsTask : public Task {
 public:
  typedef base::OnceCallback<void(std::unique_ptr<OfflinePageVisuals>)>
      CompleteCallback;

  GetVisualsTask(OfflinePageMetadataStore* store,
                 int64_t offline_id,
                 CompleteCallback complete_callback);

  GetVisualsTask(const GetVisualsTask&) = delete;
  GetVisualsTask& operator=(const GetVisualsTask&) = delete;

  ~GetVisualsTask() override;

 private:
  typedef std::unique_ptr<OfflinePageVisuals> Result;

  // Task implementation:
  void Run() override;

  void Complete(Result result);

  raw_ptr<OfflinePageMetadataStore> store_;
  int64_t offline_id_;
  base::OnceCallback<void(std::unique_ptr<OfflinePageVisuals>)>
      complete_callback_;
  base::WeakPtrFactory<GetVisualsTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_VISUALS_TASK_H_
