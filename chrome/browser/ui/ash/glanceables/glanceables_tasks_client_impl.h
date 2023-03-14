// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_

#include <string>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "base/functional/callback_forward.h"
#include "google_apis/tasks/tasks_api_requests.h"

namespace ash {

// Provides implementation for `GlanceablesTasksClient`. Responsible for
// communication with Google Tasks API.
class GlanceablesTasksClientImpl : public GlanceablesTasksClient {
 public:
  GlanceablesTasksClientImpl() = default;
  GlanceablesTasksClientImpl(const GlanceablesTasksClientImpl&) = delete;
  GlanceablesTasksClientImpl& operator=(const GlanceablesTasksClientImpl&) =
      delete;
  ~GlanceablesTasksClientImpl() override = default;

  // GlanceablesTasksClient:
  base::OnceClosure GetTaskLists(
      google_apis::tasks::ListTaskListsRequest::Callback callback) override;
  base::OnceClosure GetTasks(
      google_apis::tasks::ListTasksRequest::Callback callback,
      const std::string& task_list_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
