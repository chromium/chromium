// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_backend.h"

namespace critical_actions {

CriticalActionService::CriticalActionService(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    history::HistoryService* history_service)
    : backend_(backend_task_runner, db_path) {
  backend_.AsyncCall(&CriticalActionBackend::Init);
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

CriticalActionService::~CriticalActionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CriticalActionService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  history_service_observation_.Reset();
  backend_.Reset();
}

void CriticalActionService::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::AddCriticalAction).WithArgs(entry);
}

void CriticalActionService::GetCriticalAction(
    std::string_view critical_action_id,
    base::OnceCallback<void(std::optional<CriticalActionEntry>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (!backend_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::GetCriticalAction)
      .WithArgs(std::string(critical_action_id))
      .Then(std::move(callback));
}

void CriticalActionService::GetCriticalActions(
    const CriticalActionQueryOptions& options,
    base::OnceCallback<void(std::vector<CriticalActionEntry>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (!backend_) {
    std::move(callback).Run({});
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::GetCriticalActions)
      .WithArgs(options)
      .Then(std::move(callback));
}

void CriticalActionService::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalAction)
      .WithArgs(std::string(critical_action_id));
}

void CriticalActionService::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalActionsInTimeRange)
      .WithArgs(start_time, end_time);
}

void CriticalActionService::DeleteCriticalActionsByVisitIds(
    const std::vector<int64_t>& visit_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalActionsByVisitIds)
      .WithArgs(visit_ids);
}

void CriticalActionService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (deletion_info.IsAllHistory()) {
    DeleteCriticalActionsInTimeRange(base::Time(), base::Time::Max());
  } else if (deletion_info.time_range().IsValid()) {
    DeleteCriticalActionsInTimeRange(deletion_info.time_range().begin(),
                                     deletion_info.time_range().end());
  } else if (!deletion_info.deleted_visit_ids().empty()) {
    std::vector<int64_t> visit_ids(deletion_info.deleted_visit_ids().begin(),
                                   deletion_info.deleted_visit_ids().end());
    DeleteCriticalActionsByVisitIds(visit_ids);
  }
}

void CriticalActionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

}  // namespace critical_actions
