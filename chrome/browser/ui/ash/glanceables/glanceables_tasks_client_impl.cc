// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
using ::google_apis::tasks::PatchTaskRequest;
using ::google_apis::tasks::Task;
using ::google_apis::tasks::TaskList;
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::Tasks;

// TODO(b/269750741): Update the traffic annotation tag once all "[TBD]" items
// are ready.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("glanceables_tasks_integration", R"(
        semantics {
          sender: "Glanceables keyed service"
          description: "Provide ChromeOS users quick access to their "
                       "task lists without opening the app or website"
          trigger: "[TBD] Depends on UI surface and pre-fetching strategy"
          internal {
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          data: "The request is authenticated with an OAuth2 access token "
                "identifying the Google account"
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-03-14"
        }
        policy {
          cookies_allowed: NO
          setting: "[TBD] This feature cannot be disabled in settings"
          policy_exception_justification: "WIP, guarded by `GlanceablesV2` flag"
        }
    )");

// Converts a single `raw_task` received from Google Tasks API to ash-friendly
// type. Recursively converts all subtasks related to this task.
// `grouped_subtasks` - allows to find subtasks by their parent id.
std::unique_ptr<GlanceablesTask> ConvertIndividualTask(
    const Task* const raw_task,
    base::flat_map<std::string, std::vector<const Task*>>& grouped_subtasks) {
  const auto iter = grouped_subtasks.find(raw_task->id());
  std::vector<std::unique_ptr<GlanceablesTask>> converted_subtasks;
  if (iter != grouped_subtasks.end()) {
    converted_subtasks.reserve(iter->second.size());
    for (const auto* const raw_subtask : iter->second) {
      converted_subtasks.push_back(
          ConvertIndividualTask(raw_subtask, grouped_subtasks));
    }
    grouped_subtasks.erase(iter);
  }

  return std::make_unique<GlanceablesTask>(
      raw_task->id(), raw_task->title(),
      /*completed=*/raw_task->status() == Task::Status::kCompleted,
      std::move(converted_subtasks));
}

// Entry point to convert `raw_tasks` received from Google Tasks API to
// ash-friendly types.
std::vector<std::unique_ptr<GlanceablesTask>> ConvertTasks(
    const std::vector<std::unique_ptr<Task>>& raw_tasks) {
  // Find root level tasks and group all other subtasks by their parent id.
  std::vector<const Task*> root_tasks;
  base::flat_map<std::string, std::vector<const Task*>> grouped_subtasks;
  for (const auto& item : raw_tasks) {
    if (item->parent_id().empty()) {
      root_tasks.push_back(item.get());
      continue;
    }

    grouped_subtasks[item->parent_id()].push_back(item.get());
  }

  std::vector<std::unique_ptr<GlanceablesTask>> converted_tasks;
  converted_tasks.reserve(root_tasks.size());
  for (const auto* const root_task : root_tasks) {
    converted_tasks.push_back(
        ConvertIndividualTask(root_task, grouped_subtasks));
  }

  if (!grouped_subtasks.empty()) {
    // At this moment `grouped_subtasks` should be empty. If not - something is
    // wrong with the returned data (some tasks point to invalid `parent_id()`).
    return std::vector<std::unique_ptr<GlanceablesTask>>();
  }

  return converted_tasks;
}

}  // namespace

GlanceablesTasksClientImpl::GlanceablesTasksClientImpl(
    const GlanceablesTasksClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesTasksClientImpl::~GlanceablesTasksClientImpl() = default;

void GlanceablesTasksClientImpl::GetTaskLists(
    GlanceablesTasksClient::GetTaskListsCallback callback) {
  if (task_lists_) {
    std::move(callback).Run(task_lists_.get());
    return;
  }
  task_lists_ = std::make_unique<ui::ListModel<GlanceablesTaskList>>();
  FetchTaskListsPage(/*page_token=*/"", std::move(callback));
}

void GlanceablesTasksClientImpl::GetTasks(
    const std::string& task_list_id,
    GlanceablesTasksClient::GetTasksCallback callback) {
  CHECK(!task_list_id.empty());
  const auto [iter, inserted] = tasks_in_task_lists_.emplace(
      task_list_id, std::make_unique<ui::ListModel<GlanceablesTask>>());
  if (!inserted) {
    std::move(callback).Run(iter->second.get());
    return;
  }
  FetchTasksPage(task_list_id, /*page_token=*/"", /*accumulated_raw_tasks=*/{},
                 std::move(callback));
}

void GlanceablesTasksClientImpl::MarkAsCompleted(
    const std::string& task_list_id,
    const std::string& task_id,
    GlanceablesTasksClient::MarkAsCompletedCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());
  CHECK(callback);

  GetRequestSender()->StartRequestWithAuthRetry(
      std::make_unique<PatchTaskRequest>(
          request_sender_.get(),
          base::BindOnce(&GlanceablesTasksClientImpl::OnMarkedAsCompleted,
                         weak_factory_.GetWeakPtr(), task_list_id, task_id,
                         std::move(callback)),
          task_list_id, task_id, Task::Status::kCompleted));
}

void GlanceablesTasksClientImpl::FetchTaskListsPage(
    const std::string& page_token,
    GlanceablesTasksClient::GetTaskListsCallback callback) {
  GetRequestSender()->StartRequestWithAuthRetry(
      std::make_unique<ListTaskListsRequest>(
          request_sender_.get(),
          base::BindOnce(&GlanceablesTasksClientImpl::OnTaskListsPageFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback)),
          page_token));
}

void GlanceablesTasksClientImpl::OnTaskListsPageFetched(
    GlanceablesTasksClient::GetTaskListsCallback callback,
    base::expected<std::unique_ptr<TaskLists>, ApiErrorCode> result) {
  if (!result.has_value()) {
    task_lists_->DeleteAll();
    std::move(callback).Run(task_lists_.get());
    return;
  }

  for (const auto& raw_item : result.value()->items()) {
    task_lists_->Add(std::make_unique<GlanceablesTaskList>(
        raw_item->id(), raw_item->title(), raw_item->updated()));
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(task_lists_.get());
  } else {
    FetchTaskListsPage(result.value()->next_page_token(), std::move(callback));
  }
}

void GlanceablesTasksClientImpl::FetchTasksPage(
    const std::string& task_list_id,
    const std::string& page_token,
    std::vector<std::unique_ptr<Task>> accumulated_raw_tasks,
    GlanceablesTasksClient::GetTasksCallback callback) {
  GetRequestSender()->StartRequestWithAuthRetry(
      std::make_unique<ListTasksRequest>(
          request_sender_.get(),
          base::BindOnce(&GlanceablesTasksClientImpl::OnTasksPageFetched,
                         weak_factory_.GetWeakPtr(), task_list_id,
                         std::move(accumulated_raw_tasks), std::move(callback)),
          task_list_id, page_token));
}

void GlanceablesTasksClientImpl::OnTasksPageFetched(
    const std::string& task_list_id,
    std::vector<std::unique_ptr<Task>> accumulated_raw_tasks,
    GlanceablesTasksClient::GetTasksCallback callback,
    base::expected<std::unique_ptr<Tasks>, ApiErrorCode> result) {
  const auto iter = tasks_in_task_lists_.find(task_list_id);

  if (!result.has_value()) {
    std::move(callback).Run(iter->second.get());
    return;
  }

  accumulated_raw_tasks.insert(
      accumulated_raw_tasks.end(),
      std::make_move_iterator(result.value()->mutable_items()->begin()),
      std::make_move_iterator(result.value()->mutable_items()->end()));

  if (result.value()->next_page_token().empty()) {
    for (auto& item : ConvertTasks(accumulated_raw_tasks)) {
      iter->second->Add(std::move(item));
    }
    std::move(callback).Run(iter->second.get());
  } else {
    FetchTasksPage(task_list_id, result.value()->next_page_token(),
                   std::move(accumulated_raw_tasks), std::move(callback));
  }
}

void GlanceablesTasksClientImpl::OnMarkedAsCompleted(
    const std::string& task_list_id,
    const std::string& task_id,
    GlanceablesTasksClient::MarkAsCompletedCallback callback,
    ApiErrorCode status_code) {
  if (status_code != ApiErrorCode::HTTP_SUCCESS) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  const auto task_list_iter = tasks_in_task_lists_.find(task_list_id);
  if (task_list_iter == tasks_in_task_lists_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  const auto task_iter = std::find_if(
      task_list_iter->second->begin(), task_list_iter->second->end(),
      [&task_id](const auto& task) { return task->id == task_id; });
  if (task_iter == task_list_iter->second->end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  const auto task_index = task_iter - task_list_iter->second->begin();
  task_list_iter->second->RemoveAt(task_index);
  std::move(callback).Run(/*success=*/true);
}

google_apis::RequestSender* GlanceablesTasksClientImpl::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ = std::move(create_request_sender_callback_)
                          .Run({GaiaConstants::kTasksReadOnlyOAuth2Scope},
                               kTrafficAnnotationTag);
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

}  // namespace ash
