// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_REMOVE_URL_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_REMOVE_URL_TASK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/sql_callback_task.h"
#include "components/offline_pages/task/task.h"
#include "url/gurl.h"

namespace offline_pages {
class PrefetchStore;

// Creates a task that removes a URL from the pipeline. Finalizes any item with
// matching URL if it's not already finalized.
std::unique_ptr<SqlCallbackTask> MakeRemoveUrlTask(PrefetchStore* store,
                                                   const GURL& url);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_REMOVE_URL_TASK_H_
