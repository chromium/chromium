// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

// Gets offline pages that match the criteria.
class GetPagesTask : public Task {
 public:
  // Structure defining and intermediate read result.
  struct ReadResult {
    ReadResult();
    ReadResult(const ReadResult& other);
    ~ReadResult();

    bool success = false;
    std::vector<OfflinePageItem> pages;
  };

  GetPagesTask(OfflinePageMetadataStore* store,
               const PageCriteria& criteria,
               MultipleOfflinePageItemCallback callback);

  GetPagesTask(const GetPagesTask&) = delete;
  GetPagesTask& operator=(const GetPagesTask&) = delete;

  ~GetPagesTask() override;

  // Reads and returns all pages matching |criteria|. This function reads
  // from the database and should be called from within an
  // |SqlStoreBase::Execute()| call.
  static ReadResult ReadPagesWithCriteriaSync(
      const PageCriteria& criteria,
      sql::Database* db);

 private:
  // Task implementation:
  void Run() override;

  void CompleteWithResult(ReadResult result);

  raw_ptr<OfflinePageMetadataStore> store_;
  PageCriteria criteria_;
  MultipleOfflinePageItemCallback callback_;

  base::WeakPtrFactory<GetPagesTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_
