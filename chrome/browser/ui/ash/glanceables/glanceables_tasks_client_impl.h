// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/api/tasks/tasks_client.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "ui/base/models/list_model.h"

namespace base {
class Time;
}  // namespace base

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

namespace api {
struct Task;
struct TaskList;
}  // namespace api

// Provides implementation for `api::TasksClient`. Responsible for
// communication with Google Tasks API.
class TasksClientImpl : public api::TasksClient {
 public:
  // Provides an instance of `google_apis::RequestSender` for the client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  explicit TasksClientImpl(
      const CreateRequestSenderCallback& create_request_sender_callback);
  TasksClientImpl(const TasksClientImpl&) = delete;
  TasksClientImpl& operator=(const TasksClientImpl&) = delete;
  ~TasksClientImpl() override;

  // api::TasksClient:
  void GetTaskLists(api::TasksClient::GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                api::TasksClient::GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       bool completed) override;
  void AddTask(const std::string& task_list_id,
               const std::string& title) override;
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  api::TasksClient::UpdateTaskCallback callback) override;
  void OnGlanceablesBubbleClosed(
      api::TasksClient::OnAllPendingCompletedTasksSavedCallback callback =
          base::DoNothing()) override;

  using TaskListsRequestCallback =
      base::RepeatingCallback<void(const std::string& page_token)>;
  void set_task_lists_request_callback_for_testing(
      const TaskListsRequestCallback& callback) {
    task_lists_request_callback_ = callback;
  }

  using TasksRequestCallback =
      base::RepeatingCallback<void(const std::string& task_list_id,
                                   const std::string& page_token)>;
  void set_tasks_request_callback_for_testing(
      const TasksRequestCallback& callback) {
    tasks_request_callback_ = callback;
  }

 private:
  enum class FetchStatus { kNotFresh, kRefreshing, kFresh };

  // A structure that keeps track of fetch status and list of pending
  // callbacks for a task lists fetch request.
  struct TaskListsFetchState {
    TaskListsFetchState();
    ~TaskListsFetchState();

    FetchStatus status = FetchStatus::kNotFresh;
    // Callbacks to be called when all task lists get fetched using tasks API.
    // Should be non-empty if a task lists fetch is in progress.
    std::vector<api::TasksClient::GetTaskListsCallback> callbacks;
  };

  // A structure that keeps track of fetch status and list of pending callbacks
  // for a single tasks in a task list fetch request.
  struct TasksFetchState {
    TasksFetchState();
    ~TasksFetchState();

    FetchStatus status = FetchStatus::kNotFresh;
    // Callbacks to be called when all tasks in a task list get fetched using
    // tasks API.
    // Should be non-empty if a tasks fetch for the target task list is in
    // progress.
    std::vector<api::TasksClient::GetTasksCallback> callbacks;
  };

  // Fetches one page of task lists data.
  // `page_token`  - token specifying the result page to return, comes from the
  //                 previous fetch request. Use an empty string to fetch the
  //                 first page.
  // `page_number` - 1-based page number of this fetch request. Used for UMA
  //                 to track the total number of pages needed to fetch.
  void FetchTaskListsPage(const std::string& page_token, int page_number);

  // Callback for `FetchTaskListsPage()`. Transforms fetched items to
  // ash-friendly types. If `next_page_token()` in the `result` is not empty -
  // calls another `FetchTaskListsPage()`, otherwise runs
  // `RunGetTaskListsCallbacks()`.
  void OnTaskListsPageFetched(
      const base::Time& request_start_time,
      int page_number,
      base::expected<std::unique_ptr<google_apis::tasks::TaskLists>,
                     google_apis::ApiErrorCode> result);

  // Fetches one page of tasks data.
  // `task_list_id`          - task list identifier.
  // `page_token`            - token specifying the result page to return, comes
  //                           from the previous fetch request. Use an empty
  //                           string to fetch the first page.
  // `page_number`           - 1-based page number of this fetch request. Used
  //                           for UMA to track the total number of pages needed
  //                           to fetch.
  // `accumulated_raw_tasks` - in contrast to the task lists conversion logic,
  //                           tasks can't be converted independently on every
  //                           single page response (subtasks could go first,
  //                           but their parent tasks will be on the next page).
  //                           This parameter helps to accumulate all of them
  //                           first and then do the conversion once the last
  //                           page is fetched.
  void FetchTasksPage(const std::string& task_list_id,
                      const std::string& page_token,
                      int page_number,
                      std::vector<std::unique_ptr<google_apis::tasks::Task>>
                          accumulated_raw_tasks);

  // Callback for `FetchTasksPage()`. Transforms fetched items to ash-friendly
  // types. If `next_page_token()` in the `result` is not empty - calls another
  // `FetchTasksPage()`, otherwise runs `RunGetTasksCallbacks()`.
  void OnTasksPageFetched(
      const std::string& task_list_id,
      std::vector<std::unique_ptr<google_apis::tasks::Task>>
          accumulated_raw_tasks,
      const base::Time& request_start_time,
      int page_number,
      base::expected<std::unique_ptr<google_apis::tasks::Tasks>,
                     google_apis::ApiErrorCode> result);

  // Callback for `MarkAsCompleted()` request. Does not removes the task from
  // `tasks_in_task_lists_` as it will be cleared by
  // `OnGlanceablesBubbleClosed`.
  void OnMarkedAsCompleted(const base::Time& request_start_time,
                           base::RepeatingClosure on_done,
                           google_apis::ApiErrorCode status_code);

  // Done callback for `AddTask()` request.
  // `task_list_id` - id of the task list used in the request.
  // `result`       - newly created task or HTTP error.
  void OnTaskAdded(const std::string& task_list_id,
                   base::expected<std::unique_ptr<google_apis::tasks::Task>,
                                  google_apis::ApiErrorCode> result);

  // Done callback for `UpdateTask()` request.
  // `task_list_id` - id of the task list used in the request.
  // `status_code`  - HTTP status code of the operation.
  void OnTaskUpdated(api::TasksClient::UpdateTaskCallback callback,
                     google_apis::ApiErrorCode status_code);

  // To be called when requests to get user's task lists complete.
  // It sets the task lists fetch status to `final_fetch_status`, and runs all
  // pending callbacks in `task_lists_fetch_state_`.
  void RunGetTaskListsCallbacks(FetchStatus final_fetch_status);

  // To be called when requests to get tasks in the task list identified by
  // `task_list_id` complete. It sets fetch status for the task list fetch to
  // `final_fetch_status`, and runs all pending callbacks for the task list
  // (kept in `tasks_fetch_state_` map). The callbacks are run with `tasks`.
  void RunGetTasksCallbacks(const std::string& task_list_id,
                            FetchStatus final_fetch_status,
                            ui::ListModel<api::Task>* tasks);

  // A map of `task_list_id` to a set of `task_id` that are pending to be
  // completed.
  std::map<std::string, std::set<std::string>> pending_completed_tasks_;

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // The current fetch state for the users task lists.
  TaskListsFetchState task_lists_fetch_state_;

  // All available task lists.
  ui::ListModel<api::TaskList> task_lists_;

  // All available tasks grouped by task list id.
  std::map<std::string, ui::ListModel<api::Task>> tasks_in_task_lists_;

  // Map that contains fetch states for tasks requests from different task
  // lists. Mapped by the task list id.
  std::map<std::string, std::unique_ptr<TasksFetchState>> tasks_fetch_state_;

  // Stub tasks list model that can be used to return an empty task list to
  // `GetTasks()` requests.
  ui::ListModel<api::Task> stub_task_list_;

  // Callbacks invoked whenever a tasks API request is made. Used primarily
  // in tests.
  TaskListsRequestCallback task_lists_request_callback_;
  TasksRequestCallback tasks_request_callback_;

  base::WeakPtrFactory<TasksClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_TASKS_CLIENT_IMPL_H_
