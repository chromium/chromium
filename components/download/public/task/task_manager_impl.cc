// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/task/task_manager_impl.h"

namespace download {
namespace {

bool HasDuplicateParams(
    const std::map<DownloadTaskType, TaskManager::TaskParams>& map,
    DownloadTaskType task_type,
    const TaskManager::TaskParams& params) {
  auto it = map.find(task_type);
  return it != map.end() && it->second == params;
}

}  // namespace

TaskManagerImpl::TaskManagerImpl(std::unique_ptr<TaskScheduler> task_scheduler)
    : task_scheduler_(std::move(task_scheduler)) {}

TaskManagerImpl::~TaskManagerImpl() = default;

void TaskManagerImpl::ScheduleTask(DownloadTaskType task_type,
                                   const TaskParams& params) {
  if (HasDuplicateParams(current_task_params_, task_type, params) ||
      HasDuplicateParams(pending_task_params_, task_type, params)) {
    return;
  }

  pending_task_params_[task_type] = params;
  if (IsRunningTask(task_type))
    return;

  task_scheduler_->ScheduleTask(
      task_type, params.require_unmetered_network, params.require_charging,
      params.optimal_battery_percentage, params.window_start_time_seconds,
      params.window_end_time_seconds);
}

void TaskManagerImpl::UnscheduleTask(DownloadTaskType task_type) {
  pending_task_params_.erase(task_type);
  if (IsRunningTask(task_type))
    return;

  task_scheduler_->CancelTask(task_type);
}

void TaskManagerImpl::OnStartScheduledTask(DownloadTaskType task_type,
                                           TaskFinishedCallback callback) {
  if (pending_task_params_.find(task_type) != pending_task_params_.end())
    current_task_params_[task_type] = pending_task_params_[task_type];

  pending_task_params_.erase(task_type);
  task_finished_callbacks_[task_type] = std::move(callback);
}

void TaskManagerImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  DCHECK(IsRunningTask(task_type));
  current_task_params_.erase(task_type);
  task_finished_callbacks_.erase(task_type);

  if (pending_task_params_.find(task_type) != pending_task_params_.end()) {
    auto params = pending_task_params_[task_type];
    pending_task_params_.erase(task_type);
    ScheduleTask(task_type, params);
  }
}

bool TaskManagerImpl::IsRunningTask(DownloadTaskType task_type) const {
  return task_finished_callbacks_.find(task_type) !=
         task_finished_callbacks_.end();
}

void TaskManagerImpl::NotifyTaskFinished(DownloadTaskType task_type,
                                         bool needs_reschedule) {
  if (!IsRunningTask(task_type))
    return;

  current_task_params_.erase(task_type);

  bool has_pending_params =
      pending_task_params_.find(task_type) != pending_task_params_.end();

  std::move(task_finished_callbacks_[task_type])
      .Run(needs_reschedule && !has_pending_params);
  task_finished_callbacks_.erase(task_type);

  if (has_pending_params) {
    auto params = pending_task_params_[task_type];
    pending_task_params_.erase(task_type);
    ScheduleTask(task_type, params);
  }
}

}  // namespace download
