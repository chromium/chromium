// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_database.h"

namespace critical_actions {

namespace {

constexpr base::TimeDelta kInitialInitRetryDelay = base::Seconds(1);
constexpr int kMaxInitRetries = 5;
static_assert(kMaxInitRetries < 30, "1 << init_retry_count_ must not overflow");

}  // namespace

CriticalActionBackend::CriticalActionBackend(const base::FilePath& db_path)
    : db_path_(db_path),
      busy_timeout_(CriticalActionDatabase::kDefaultBusyTimeout) {
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
  DCHECK(!init_retry_timer_.IsRunning());
  TryInit(/*advance_backoff=*/true);
}

void CriticalActionBackend::SetBusyTimeoutForTesting(
    base::TimeDelta busy_timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  busy_timeout_ = busy_timeout;
}

void CriticalActionBackend::TryInit(bool advance_backoff) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_);
  auto db = std::make_unique<CriticalActionDatabase>(db_path_);
  switch (db->Init(busy_timeout_)) {
    case CriticalActionDatabase::InitStatus::kOk:
      init_retry_timer_.Stop();
      db_ = std::move(db);
      init_retry_count_ = 0;
      early_retry_used_ = false;
      return;
    case CriticalActionDatabase::InitStatus::kBusyTimeout:
      if (!advance_backoff) {
        return;
      }
      if (init_retry_count_ < kMaxInitRetries) {
        const base::TimeDelta delay =
            kInitialInitRetryDelay * (1 << init_retry_count_);
        ++init_retry_count_;
        early_retry_used_ = false;
        last_busy_failure_ticks_ = base::TimeTicks::Now();
        init_retry_timer_.Start(FROM_HERE, delay,
                                base::BindOnce(&CriticalActionBackend::TryInit,
                                               base::Unretained(this),
                                               /*advance_backoff=*/true));
        return;
      }
      LOG(ERROR) << "Failed to initialize CriticalActionDatabase after "
                 << init_retry_count_ << " busy retries";
      return;
    case CriticalActionDatabase::InitStatus::kError:
      init_retry_timer_.Stop();
      LOG(ERROR) << "Failed to initialize CriticalActionDatabase";
      return;
  }
}

bool CriticalActionBackend::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_) {
    return true;
  }
  if (!init_retry_timer_.IsRunning()) {
    return false;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now >= init_retry_timer_.desired_run_time()) {
    init_retry_timer_.Stop();
    TryInit(/*advance_backoff=*/true);
    return db_ != nullptr;
  }
  if (early_retry_used_ ||
      now - last_busy_failure_ticks_ < kInitialInitRetryDelay) {
    return false;
  }
  early_retry_used_ = true;
  last_busy_failure_ticks_ = now;
  TryInit(/*advance_backoff=*/false);
  return db_ != nullptr;
}

void CriticalActionBackend::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::AddCriticalAction: id="
          << entry.critical_action_id
          << ", type=" << static_cast<int>(entry.action_type);
  if (!EnsureInitialized()) {
    return;
  }
  if (!db_->AddCriticalAction(entry)) {
    LOG(WARNING) << "Failed to add critical action: id="
                 << entry.critical_action_id;
  }
}

void CriticalActionBackend::SetCriticalActionsConversationId(
    const std::vector<std::string>& critical_action_ids,
    std::string_view conversation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::SetCriticalActionsConversationId: "
             "critical_action_ids="
          << critical_action_ids.size() << ", conv_id=" << conversation_id;
  if (!EnsureInitialized()) {
    return;
  }
  if (!db_->SetCriticalActionsConversationId(critical_action_ids,
                                             conversation_id)) {
    LOG(WARNING) << "Failed to set critical actions conversation ID";
  }
}

std::optional<CriticalActionEntry> CriticalActionBackend::GetCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::GetCriticalAction: id="
          << critical_action_id;
  if (!EnsureInitialized()) {
    return std::nullopt;
  }
  return db_->GetCriticalAction(critical_action_id);
}

std::vector<CriticalActionEntry> CriticalActionBackend::GetCriticalActions(
    const CriticalActionQueryOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::GetCriticalActions";
  if (!EnsureInitialized()) {
    return {};
  }
  return db_->GetCriticalActions(options);
}

void CriticalActionBackend::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CriticalActionBackend::DeleteCriticalAction: id="
          << critical_action_id;
  if (!EnsureInitialized()) {
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
  if (!EnsureInitialized()) {
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
  if (!EnsureInitialized()) {
    return;
  }
  if (!db_->DeleteCriticalActionsByVisitIds(visit_ids)) {
    LOG(WARNING) << "Failed to delete critical actions by visit IDs";
  }
}

}  // namespace critical_actions
