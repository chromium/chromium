// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "ui/base/models/list_model.h"

namespace google_apis {
class RequestSender;
namespace tasks {
class TaskLists;
class Tasks;
}  // namespace tasks
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

struct GlanceablesTask;
struct GlanceablesTaskList;

// Provides implementation for `GlanceablesTasksClient`. Responsible for
// communication with Google Tasks API.
class GlanceablesTasksClientImpl : public GlanceablesTasksClient {
 public:
  // Provides an instance of `google_apis::RequestSender` for the client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  explicit GlanceablesTasksClientImpl(
      const CreateRequestSenderCallback& create_request_sender_callback);
  GlanceablesTasksClientImpl(const GlanceablesTasksClientImpl&) = delete;
  GlanceablesTasksClientImpl& operator=(const GlanceablesTasksClientImpl&) =
      delete;
  ~GlanceablesTasksClientImpl() override;

  // GlanceablesTasksClient:
  void GetTaskLists(
      GlanceablesTasksClient::GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                GlanceablesTasksClient::GetTasksCallback callback) override;

 private:
  // Callback for `GetTaskLists()`. Transforms fetched items to ash-friendly
  // types.
  void OnTaskListsFetched(
      GlanceablesTasksClient::GetTaskListsCallback callback,
      base::expected<std::unique_ptr<google_apis::tasks::TaskLists>,
                     google_apis::ApiErrorCode> result) const;

  // Callback for `GetTasks()`. Transforms fetched items to ash-friendly types.
  void OnTasksFetched(const std::string& task_list_id,
                      GlanceablesTasksClient::GetTasksCallback callback,
                      base::expected<std::unique_ptr<google_apis::tasks::Tasks>,
                                     google_apis::ApiErrorCode> result);

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // All available task lists. Initialized after the first fetch request to
  // distinguish between "not fetched yet" vs. "fetched, but has no items".
  std::unique_ptr<ui::ListModel<GlanceablesTaskList>> task_lists_;

  // All available tasks grouped by task list id.
  base::flat_map<std::string, std::unique_ptr<ui::ListModel<GlanceablesTask>>>
      tasks_in_task_lists_;

  base::WeakPtrFactory<GlanceablesTasksClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
