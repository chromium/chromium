// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
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

std::vector<GlanceablesTaskList> ConvertTaskLists(
    const std::vector<std::unique_ptr<TaskList>>& raw_items) {
  std::vector<GlanceablesTaskList> converted_task_lists;
  converted_task_lists.reserve(raw_items.size());
  for (const auto& item : raw_items) {
    converted_task_lists.emplace_back(item->id(), item->title(),
                                      item->updated());
  }
  return converted_task_lists;
}

GlanceablesTask ConvertTask(
    const Task* const task,
    base::flat_map<std::string, std::vector<const Task*>>& grouped_subtasks) {
  const auto iter = grouped_subtasks.find(task->id());
  std::vector<GlanceablesTask> converted_subtasks;
  if (iter != grouped_subtasks.end()) {
    converted_subtasks.reserve(iter->second.size());
    for (const auto* const subtask : iter->second) {
      converted_subtasks.push_back(ConvertTask(subtask, grouped_subtasks));
    }
    grouped_subtasks.erase(iter);
  }

  return GlanceablesTask(task->id(), task->title(),
                         task->status() == Task::Status::kCompleted,
                         converted_subtasks);
}

std::vector<GlanceablesTask> ConvertTasks(
    const std::vector<std::unique_ptr<Task>>& raw_items) {
  // Find root level tasks and group all other subtasks by their parent id.
  std::vector<const Task*> root_tasks;
  base::flat_map<std::string, std::vector<const Task*>> grouped_subtasks;
  for (const auto& item : raw_items) {
    if (item->parent_id().empty()) {
      root_tasks.push_back(item.get());
      continue;
    }

    grouped_subtasks[item->parent_id()].push_back(item.get());
  }

  std::vector<GlanceablesTask> converted_tasks;
  converted_tasks.reserve(root_tasks.size());
  for (const auto* const root_task : root_tasks) {
    converted_tasks.push_back(ConvertTask(root_task, grouped_subtasks));
  }

  if (!grouped_subtasks.empty()) {
    // At this moment `grouped_subtasks` should be empty. If not - something is
    // wrong with the returned data (some tasks point to invalid `parent_id()`).
    return std::vector<GlanceablesTask>();
  }

  return converted_tasks;
}

}  // namespace

GlanceablesTasksClientImpl::GlanceablesTasksClientImpl(
    const GlanceablesTasksClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesTasksClientImpl::~GlanceablesTasksClientImpl() = default;

base::OnceClosure GlanceablesTasksClientImpl::GetTaskLists(
    GlanceablesTasksClient::GetTaskListsCallback callback) {
  EnsureRequestSenderExists();
  return request_sender_->StartRequestWithAuthRetry(
      std::make_unique<ListTaskListsRequest>(
          request_sender_.get(),
          base::BindOnce(&GlanceablesTasksClientImpl::OnTaskListsFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

base::OnceClosure GlanceablesTasksClientImpl::GetTasks(
    GlanceablesTasksClient::GetTasksCallback callback,
    const std::string& task_list_id) {
  DCHECK(!task_list_id.empty());
  EnsureRequestSenderExists();
  return request_sender_->StartRequestWithAuthRetry(
      std::make_unique<ListTasksRequest>(
          request_sender_.get(),
          base::BindOnce(&GlanceablesTasksClientImpl::OnTasksFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback)),
          task_list_id));
}

void GlanceablesTasksClientImpl::OnTaskListsFetched(
    GlanceablesTasksClient::GetTaskListsCallback callback,
    base::expected<std::unique_ptr<TaskLists>, ApiErrorCode> result) const {
  std::move(callback).Run(result.has_value()
                              ? ConvertTaskLists(result.value()->items())
                              : std::vector<GlanceablesTaskList>());
}

void GlanceablesTasksClientImpl::OnTasksFetched(
    GlanceablesTasksClient::GetTasksCallback callback,
    base::expected<std::unique_ptr<Tasks>, ApiErrorCode> result) const {
  std::move(callback).Run(result.has_value()
                              ? ConvertTasks(result.value()->items())
                              : std::vector<GlanceablesTask>());
}

void GlanceablesTasksClientImpl::EnsureRequestSenderExists() {
  if (request_sender_) {
    return;
  }
  DCHECK(create_request_sender_callback_);
  request_sender_ = std::move(create_request_sender_callback_)
                        .Run({GaiaConstants::kTasksReadOnlyOAuth2Scope},
                             kTrafficAnnotationTag);
  DCHECK(request_sender_);
}

}  // namespace ash
