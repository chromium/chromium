// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/tasks/tasks_api_requests.h"

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
  base::OnceClosure GetTaskLists(
      GlanceablesTasksClient::GetTaskListsCallback callback) override;
  base::OnceClosure GetTasks(GlanceablesTasksClient::GetTasksCallback callback,
                             const std::string& task_list_id) override;

 private:
  // Callback for `GetTaskLists()`. Transforms fetched items to ash-friendly
  // types.
  void OnTaskListsFetched(
      GlanceablesTasksClient::GetTaskListsCallback callback,
      base::expected<std::unique_ptr<google_apis::tasks::TaskLists>,
                     google_apis::ApiErrorCode> result) const;

  // Callback for `GetTasks()`. Transforms fetched items to ash-friendly types.
  void OnTasksFetched(GlanceablesTasksClient::GetTasksCallback callback,
                      base::expected<std::unique_ptr<google_apis::tasks::Tasks>,
                                     google_apis::ApiErrorCode> result) const;

  // Creates `request_sender_` by calling `create_request_sender_callback_` on
  // demand.
  void EnsureRequestSenderExists();

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  base::WeakPtrFactory<GlanceablesTasksClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
