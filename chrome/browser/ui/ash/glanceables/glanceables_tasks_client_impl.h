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
  void MarkAsCompleted(
      const std::string& task_list_id,
      const std::string& task_id,
      GlanceablesTasksClient::MarkAsCompletedCallback callback) override;

 private:
  // Fetches one page of task lists data.
  // `page_token` - token specifying the result page to return, comes from the
  //                previous fetch request. Use an empty string to fetch the
  //                first page.
  // `callback`   - done callback passed from `GetTaskLists()` to
  //                `OnTaskListsPageFetched()`.
  void FetchTaskListsPage(
      const std::string& page_token,
      GlanceablesTasksClient::GetTaskListsCallback callback);

  // Callback for `FetchTaskListsPage()`. Transforms fetched items to
  // ash-friendly types. If `next_page_token()` in the `result` is not empty -
  // calls another `FetchTaskListsPage()`, otherwise runs `callback`.
  void OnTaskListsPageFetched(
      GlanceablesTasksClient::GetTaskListsCallback callback,
      base::expected<std::unique_ptr<google_apis::tasks::TaskLists>,
                     google_apis::ApiErrorCode> result);

  // Fetches one page of tasks data.
  // `task_list_id`          - task list identifier.
  // `page_token`            - token specifying the result page to return, comes
  //                           from the previous fetch request. Use an empty
  //                           string to fetch the first page.
  // `accumulated_raw_tasks` - in contrast to the task lists conversion logic,
  //                           tasks can't be converted independently on every
  //                           single page response (subtasks could go first,
  //                           but their parent tasks will be on the next page).
  //                           This parameter helps to accumulate all of them
  //                           first and then do the conversion once the last
  //                           page is fetched.
  // `callback`              - done callback passed from `GetTasks()` to
  //                           `OnTasksPageFetched()`.
  void FetchTasksPage(const std::string& task_list_id,
                      const std::string& page_token,
                      std::vector<std::unique_ptr<google_apis::tasks::Task>>
                          accumulated_raw_tasks,
                      GlanceablesTasksClient::GetTasksCallback callback);

  // Callback for `FetchTasksPage()`. Transforms fetched items to ash-friendly
  // types. If `next_page_token()` in the `result` is not empty - calls another
  // `FetchTasksPage()`, otherwise runs `callback`.
  void OnTasksPageFetched(
      const std::string& task_list_id,
      std::vector<std::unique_ptr<google_apis::tasks::Task>>
          accumulated_raw_tasks,
      GlanceablesTasksClient::GetTasksCallback callback,
      base::expected<std::unique_ptr<google_apis::tasks::Tasks>,
                     google_apis::ApiErrorCode> result);

  // Callback for `MarkAsCompleted()` request. Removes the task from
  // `tasks_in_task_lists_` if succeeded. Runs `callback` passed from
  // `MarkAsCompleted()` when done.
  void OnMarkedAsCompleted(
      const std::string& task_list_id,
      const std::string& task_id,
      GlanceablesTasksClient::MarkAsCompletedCallback callback,
      google_apis::ApiErrorCode status_code);

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
