// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STORE_VISUALS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STORE_VISUALS_TASK_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

extern const base::TimeDelta kVisualsExpirationDelta;

class OfflinePageMetadataStore;

// StoreVisualsTask stores a thumbnail and favicon in the page_thumbnails table.
// Only non-empty values for |thumbnail| and |favicon| are stored; passing empty
// strings will not overwrite existing thumbnails and favicons.
class StoreVisualsTask : public Task {
 public:
  using CompleteCallback = base::OnceCallback<void(bool, std::string)>;
  using RowUpdatedCallback =
      base::OnceCallback<void(bool, std::string, std::string)>;

  ~StoreVisualsTask() override;

  static std::unique_ptr<StoreVisualsTask> MakeStoreThumbnailTask(
      OfflinePageMetadataStore* store,
      int64_t offline_id,
      std::string thumbnail,
      CompleteCallback callback);
  static std::unique_ptr<StoreVisualsTask> MakeStoreFaviconTask(
      OfflinePageMetadataStore* store,
      int64_t offline_id,
      std::string favicon,
      CompleteCallback callback);

  // Task implementation:
  void Run() override;

 private:
  StoreVisualsTask(OfflinePageMetadataStore* store,
                   int64_t offline_id,
                   std::string thumbnail,
                   std::string favicon,
                   RowUpdatedCallback complete_callback);
  void Complete(bool success);

  OfflinePageMetadataStore* store_;
  int64_t offline_id_;
  base::Time expiration_;
  std::string thumbnail_;
  std::string favicon_;
  RowUpdatedCallback complete_callback_;
  base::WeakPtrFactory<StoreVisualsTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StoreVisualsTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STORE_VISUALS_TASK_H_
