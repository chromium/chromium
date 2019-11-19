// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_ADD_PAGE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_ADD_PAGE_TASK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

enum class AddPageResult;
enum class ItemActionStatus;
class OfflinePageMetadataStore;

// Task that adds a new page to the metadata store.
// The caller needs to provide a callback which consumes an AddPageResult and
// the page that was added.
// TODO(romax): Remove OfflinePageItem from the callback if it is unnecessary
// after taskifying OfflinePageModel.
class AddPageTask : public Task {
 public:
  typedef base::OnceCallback<void(AddPageResult)> AddPageTaskCallback;

  AddPageTask(OfflinePageMetadataStore* store,
              const OfflinePageItem& offline_page,
              AddPageTaskCallback callback);
  ~AddPageTask() override;

  // Task implementation.
  void Run() override;

 private:
  void OnAddPageDone(ItemActionStatus status);
  void InformAddPageDone(AddPageResult result);

  // The metadata store to insert the page. Not owned.
  OfflinePageMetadataStore* store_;

  OfflinePageItem offline_page_;
  AddPageTaskCallback callback_;

  base::WeakPtrFactory<AddPageTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AddPageTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_ADD_PAGE_TASK_H_
