// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"

namespace ash {

using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::Tasks;

base::OnceClosure GlanceablesTasksClientImpl::GetTaskLists(
    ListTaskListsRequest::Callback callback) {
  std::move(callback).Run(std::make_unique<TaskLists>());
  return base::DoNothing();
}

base::OnceClosure GlanceablesTasksClientImpl::GetTasks(
    ListTasksRequest::Callback callback,
    const std::string& task_list_id) {
  DCHECK(!task_list_id.empty());
  std::move(callback).Run(std::make_unique<Tasks>());
  return base::DoNothing();
}

}  // namespace ash
