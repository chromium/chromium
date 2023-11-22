// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/tasks/tasks_api_request_types.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::tasks::InsertTaskRequest;
using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
using ::google_apis::tasks::PatchTaskRequest;
using ::google_apis::tasks::Task;
using ::google_apis::tasks::TaskLink;
using ::google_apis::tasks::TaskList;
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::TaskRequestPayload;
using ::google_apis::tasks::Tasks;
using ::google_apis::tasks::TaskStatus;

// Converts `raw_tasks` received from Google Tasks API to ash-friendly types.
std::vector<std::unique_ptr<api::Task>> ConvertTasks(
    const std::vector<std::unique_ptr<Task>>& raw_tasks) {
  // Find root level tasks and collect task ids that have subtasks in one pass.
  std::vector<const Task*> root_tasks;
  base::flat_set<std::string> tasks_with_subtasks;
  for (const auto& item : raw_tasks) {
    if (item->parent_id().empty()) {
      root_tasks.push_back(item.get());
    } else {
      tasks_with_subtasks.insert(item->parent_id());
    }
  }

  // Sort tasks by their position as they appear in the companion app with "My
  // order" option selected.
  // NOTE: ideally sorting should be performed on the UI/presentation layer, but
  // there is a possibility that with further optimizations and plans to keep
  // only top N visible tasks in memory, the sorting will need to be done at
  // this layer.
  std::sort(root_tasks.begin(), root_tasks.end(),
            [](const Task* a, const Task* b) {
              return a->position().compare(b->position()) < 0;
            });

  // Convert `root_tasks` to ash-friendly types.
  std::vector<std::unique_ptr<api::Task>> converted_tasks;
  converted_tasks.reserve(root_tasks.size());
  for (const auto* const root_task : root_tasks) {
    const bool completed = root_task->status() == TaskStatus::kCompleted;
    const bool has_subtasks = tasks_with_subtasks.contains(root_task->id());
    const bool has_email_link =
        std::find_if(root_task->links().begin(), root_task->links().end(),
                     [](const auto& link) {
                       return link->type() == TaskLink::Type::kEmail;
                     }) != root_task->links().end();
    const bool has_notes = !root_task->notes().empty();
    converted_tasks.push_back(std::make_unique<api::Task>(
        root_task->id(), root_task->title(), completed, root_task->due(),
        has_subtasks, has_email_link, has_notes, root_task->updated()));
  }

  return converted_tasks;
}

}  // namespace

TasksClientImpl::TaskListsFetchState::TaskListsFetchState() = default;

TasksClientImpl::TaskListsFetchState::~TaskListsFetchState() = default;

TasksClientImpl::TasksFetchState::TasksFetchState() = default;

TasksClientImpl::TasksFetchState::~TasksFetchState() = default;

TasksClientImpl::TasksClientImpl(
    const TasksClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback,
    net::NetworkTrafficAnnotationTag traffic_annotation_tag)
    : create_request_sender_callback_(create_request_sender_callback),
      traffic_annotation_tag_(traffic_annotation_tag) {}

TasksClientImpl::~TasksClientImpl() = default;

void TasksClientImpl::GetTaskLists(
    api::TasksClient::GetTaskListsCallback callback) {
  if (task_lists_fetch_state_.status == FetchStatus::kFresh) {
    std::move(callback).Run(&task_lists_);
    return;
  }

  task_lists_fetch_state_.callbacks.push_back(std::move(callback));

  if (task_lists_fetch_state_.status != FetchStatus::kRefreshing) {
    task_lists_fetch_state_.status = FetchStatus::kRefreshing;
    FetchTaskListsPage(/*page_token=*/"", /*page_number=*/1);
  }
}

void TasksClientImpl::GetTasks(const std::string& task_list_id,
                               api::TasksClient::GetTasksCallback callback) {
  CHECK(!task_list_id.empty());

  const auto [iter, inserted] = tasks_in_task_lists_.emplace(
      std::piecewise_construct, std::forward_as_tuple(task_list_id),
      std::forward_as_tuple());

  const auto [status_it, state_inserted] =
      tasks_fetch_state_.emplace(task_list_id, nullptr);
  if (!status_it->second) {
    status_it->second = std::make_unique<TasksFetchState>();
  }
  TasksFetchState& fetch_state = *status_it->second;
  if (fetch_state.status == FetchStatus::kFresh) {
    std::move(callback).Run(&iter->second);
    return;
  }

  fetch_state.callbacks.push_back(std::move(callback));

  if (fetch_state.status != FetchStatus::kRefreshing) {
    fetch_state.status = FetchStatus::kRefreshing;
    FetchTasksPage(task_list_id, /*page_token=*/"", /*page_number=*/1,
                   /*accumulated_raw_tasks=*/{});
  }
}

void TasksClientImpl::MarkAsCompleted(const std::string& task_list_id,
                                      const std::string& task_id,
                                      bool completed) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());

  if (completed) {
    pending_completed_tasks_[task_list_id].insert(task_id);
  } else {
    if (pending_completed_tasks_.contains(task_list_id)) {
      pending_completed_tasks_[task_list_id].erase(task_id);
    }
  }
}

void TasksClientImpl::AddTask(const std::string& task_list_id,
                              const std::string& title,
                              api::TasksClient::OnTaskSavedCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!title.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<InsertTaskRequest>(
      request_sender, task_list_id, /*previous_task_id=*/"",
      TaskRequestPayload{.title = title, .status = TaskStatus::kNeedsAction},
      base::BindOnce(&TasksClientImpl::OnTaskAdded, weak_factory_.GetWeakPtr(),
                     task_list_id, base::Time::Now(), std::move(callback))));
}

void TasksClientImpl::UpdateTask(
    const std::string& task_list_id,
    const std::string& task_id,
    const std::string& title,
    api::TasksClient::OnTaskSavedCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());
  CHECK(!title.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<PatchTaskRequest>(
      request_sender, task_list_id, task_id, TaskRequestPayload{.title = title},
      base::BindOnce(&TasksClientImpl::OnTaskUpdated,
                     weak_factory_.GetWeakPtr(), task_list_id,
                     base::Time::Now(), std::move(callback))));
}

void TasksClientImpl::OnGlanceablesBubbleClosed(
    api::TasksClient::OnAllPendingCompletedTasksSavedCallback callback) {
  weak_factory_.InvalidateWeakPtrs();

  int num_tasks_completed = 0;
  for (const auto& [task_list_id, task_ids] : pending_completed_tasks_) {
    num_tasks_completed += task_ids.size();
  }
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(num_tasks_completed, std::move(callback));
  base::UmaHistogramCounts100(
      "Ash.Glanceables.Api.Tasks.SimultaneousMarkAsCompletedRequestsCount",
      num_tasks_completed);

  for (const auto& [task_list_id, task_ids] : pending_completed_tasks_) {
    for (const auto& task_id : task_ids) {
      auto* const request_sender = GetRequestSender();
      request_sender->StartRequestWithAuthRetry(
          std::make_unique<PatchTaskRequest>(
              request_sender, task_list_id, task_id,
              TaskRequestPayload{.status = TaskStatus::kCompleted},
              base::BindOnce(&TasksClientImpl::OnMarkedAsCompleted,
                             weak_factory_.GetWeakPtr(), base::Time::Now(),
                             barrier_closure)));
    }
  }

  for (auto& task_list_state : tasks_fetch_state_) {
    RunGetTasksCallbacks(task_list_state.first, FetchStatus::kNotFresh,
                         &stub_task_list_);
  }

  pending_completed_tasks_.clear();
  tasks_in_task_lists_.clear();
  tasks_fetch_state_.clear();

  task_lists_.DeleteAll();
  RunGetTaskListsCallbacks(FetchStatus::kNotFresh);
}

void TasksClientImpl::FetchTaskListsPage(const std::string& page_token,
                                         int page_number) {
  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListTaskListsRequest>(
          request_sender, page_token,
          base::BindOnce(&TasksClientImpl::OnTaskListsPageFetched,
                         weak_factory_.GetWeakPtr(), base::Time::Now(),
                         page_number)));
  if (task_lists_request_callback_) {
    task_lists_request_callback_.Run(page_token);
  }
}

void TasksClientImpl::OnTaskListsPageFetched(
    const base::Time& request_start_time,
    int page_number,
    base::expected<std::unique_ptr<TaskLists>, ApiErrorCode> result) {
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.GetTaskLists.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    task_lists_.DeleteAll();
    RunGetTaskListsCallbacks(FetchStatus::kNotFresh);
    return;
  }

  for (const auto& raw_item : result.value()->items()) {
    task_lists_.Add(std::make_unique<api::TaskList>(
        raw_item->id(), raw_item->title(), raw_item->updated()));
  }

  if (result.value()->next_page_token().empty()) {
    base::UmaHistogramCounts100(
        "Ash.Glanceables.Api.Tasks.GetTaskLists.PagesCount", page_number);
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.TaskListsCount",
                                task_lists_.item_count());
    RunGetTaskListsCallbacks(FetchStatus::kFresh);
  } else {
    FetchTaskListsPage(result.value()->next_page_token(), page_number + 1);
  }
}

void TasksClientImpl::FetchTasksPage(
    const std::string& task_list_id,
    const std::string& page_token,
    int page_number,
    std::vector<std::unique_ptr<Task>> accumulated_raw_tasks) {
  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<ListTasksRequest>(
      request_sender, task_list_id, page_token,
      base::BindOnce(&TasksClientImpl::OnTasksPageFetched,
                     weak_factory_.GetWeakPtr(), task_list_id,
                     std::move(accumulated_raw_tasks), base::Time::Now(),
                     page_number)));

  if (tasks_request_callback_) {
    tasks_request_callback_.Run(task_list_id, page_token);
  }
}

void TasksClientImpl::OnTasksPageFetched(
    const std::string& task_list_id,
    std::vector<std::unique_ptr<Task>> accumulated_raw_tasks,
    const base::Time& request_start_time,
    int page_number,
    base::expected<std::unique_ptr<Tasks>, ApiErrorCode> result) {
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.GetTasks.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.GetTasks.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  const auto iter = tasks_in_task_lists_.find(task_list_id);

  if (!result.has_value()) {
    iter->second.DeleteAll();
    RunGetTasksCallbacks(task_list_id, FetchStatus::kNotFresh, &iter->second);
    return;
  }

  accumulated_raw_tasks.insert(
      accumulated_raw_tasks.end(),
      std::make_move_iterator(result.value()->mutable_items()->begin()),
      std::make_move_iterator(result.value()->mutable_items()->end()));

  if (result.value()->next_page_token().empty()) {
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.GetTasks.PagesCount",
                                page_number);
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.RawTasksCount",
                                accumulated_raw_tasks.size());
    for (auto& item : ConvertTasks(accumulated_raw_tasks)) {
      iter->second.Add(std::move(item));
    }
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.ProcessedTasksCount",
                                iter->second.item_count());
    RunGetTasksCallbacks(task_list_id, FetchStatus::kFresh, &iter->second);
  } else {
    FetchTasksPage(task_list_id, result.value()->next_page_token(),
                   page_number + 1, std::move(accumulated_raw_tasks));
  }
}

void TasksClientImpl::RunGetTaskListsCallbacks(FetchStatus final_fetch_status) {
  task_lists_fetch_state_.status = final_fetch_status;

  std::vector<GetTaskListsCallback> callbacks;
  task_lists_fetch_state_.callbacks.swap(callbacks);

  for (auto& callback : callbacks) {
    std::move(callback).Run(&task_lists_);
  }
}

void TasksClientImpl::RunGetTasksCallbacks(
    const std::string& task_list_id,
    FetchStatus final_fetch_status,
    const ui::ListModel<api::Task>* tasks) {
  auto fetch_state_it = tasks_fetch_state_.find(task_list_id);
  if (fetch_state_it == tasks_fetch_state_.end()) {
    return;
  }

  TasksFetchState* fetch_state = fetch_state_it->second.get();
  fetch_state->status = final_fetch_status;

  std::vector<GetTasksCallback> callbacks;
  fetch_state->callbacks.swap(callbacks);

  for (auto& callback : callbacks) {
    std::move(callback).Run(tasks);
  }
}

void TasksClientImpl::OnMarkedAsCompleted(
    const base::Time& request_start_time,
    base::RepeatingClosure on_done,
    base::expected<std::unique_ptr<Task>, ApiErrorCode> result) {
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.PatchTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.PatchTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));
  on_done.Run();
}

void TasksClientImpl::OnTaskAdded(
    const std::string& task_list_id,
    const base::Time& request_start_time,
    api::TasksClient::OnTaskSavedCallback callback,
    base::expected<std::unique_ptr<Task>, ApiErrorCode> result) {
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.InsertTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.InsertTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  const auto iter = tasks_in_task_lists_.find(task_list_id);
  if (iter == tasks_in_task_lists_.end()) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  const auto* const task = iter->second.AddAt(
      /*index=*/0,
      std::make_unique<api::Task>(result.value()->id(), result.value()->title(),
                                  /*completed=*/false, /*due=*/absl::nullopt,
                                  /*has_subtasks=*/false,
                                  /*has_email_link=*/false, /*has_notes=*/false,
                                  result.value()->updated()));
  std::move(callback).Run(/*task=*/task);
}

void TasksClientImpl::OnTaskUpdated(
    const std::string& task_list_id,
    const base::Time& request_start_time,
    api::TasksClient::OnTaskSavedCallback callback,
    base::expected<std::unique_ptr<Task>, ApiErrorCode> result) {
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.PatchTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.PatchTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  const auto tasks_iter = tasks_in_task_lists_.find(task_list_id);
  if (tasks_iter == tasks_in_task_lists_.end()) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  const auto task_iter = std::find_if(
      tasks_iter->second.begin(), tasks_iter->second.end(),
      [&result](const auto& task) { return task->id == result->get()->id(); });
  if (task_iter == tasks_iter->second.end()) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  task_iter->get()->title = result->get()->title();
  task_iter->get()->updated = result->get()->updated();
  std::move(callback).Run(/*task=*/task_iter->get());
}

google_apis::RequestSender* TasksClientImpl::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ = std::move(create_request_sender_callback_)
                          .Run({GaiaConstants::kTasksReadOnlyOAuth2Scope,
                                GaiaConstants::kTasksOAuth2Scope},
                               traffic_annotation_tag_);
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

}  // namespace ash
