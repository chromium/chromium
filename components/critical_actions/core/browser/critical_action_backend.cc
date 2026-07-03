// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_database.h"

namespace critical_actions {

CriticalActionBackend::CriticalActionBackend(const base::FilePath& db_path)
    : db_path_(db_path) {
  // Detach from the construction sequence since the backend is designed to run
  // on a separate background task runner sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CriticalActionBackend::~CriticalActionBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CriticalActionBackend::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_);
  auto db = std::make_unique<CriticalActionDatabase>(db_path_);
  if (!db->Init()) {
    LOG(ERROR) << "Failed to initialize CriticalActionDatabase";
    return;
  }
  db_ = std::move(db);
}

void CriticalActionBackend::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::AddCriticalAction: id="
          << entry.critical_action_id
          << ", type=" << static_cast<int>(entry.action_type);
  if (!db_) {
    return;
  }
  if (!db_->AddCriticalAction(entry)) {
    LOG(WARNING) << "Failed to add critical action: id="
                 << entry.critical_action_id;
  }
}

std::optional<CriticalActionEntry> CriticalActionBackend::GetCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::GetCriticalAction: id="
          << critical_action_id;
  if (!db_) {
    return std::nullopt;
  }
  return db_->GetCriticalAction(critical_action_id);
}

std::vector<CriticalActionEntry> CriticalActionBackend::GetCriticalActions(
    const CriticalActionQueryOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::GetCriticalActions";
  if (!db_) {
    return {};
  }
  return db_->GetCriticalActions(options);
}

void CriticalActionBackend::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::DeleteCriticalAction: id="
          << critical_action_id;
  if (!db_) {
    return;
  }
  if (!db_->DeleteCriticalAction(critical_action_id)) {
    LOG(WARNING) << "Failed to delete critical action: id="
                 << critical_action_id;
  }
}

void CriticalActionBackend::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::DeleteCriticalActionsInTimeRange: "
          << "start_time=" << start_time << ", end_time=" << end_time;
  if (!db_) {
    return;
  }
  if (!db_->DeleteCriticalActionsInTimeRange(start_time, end_time)) {
    LOG(WARNING) << "Failed to delete critical actions in time range: "
                 << "start_time=" << start_time << ", end_time=" << end_time;
  }
}

void CriticalActionBackend::DeleteCriticalActionsByVisitIds(
    const std::vector<int64_t>& visit_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::DeleteCriticalActionsByVisitIds: "
          << "count=" << visit_ids.size();
  if (!db_) {
    return;
  }
  if (!db_->DeleteCriticalActionsByVisitIds(visit_ids)) {
    LOG(WARNING) << "Failed to delete critical actions by visit IDs";
  }
}

}  // namespace critical_actions
