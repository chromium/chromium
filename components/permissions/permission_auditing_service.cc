// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_auditing_service.h"

#include "utility"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/permissions/permission_auditing_database.h"

namespace {

// Specifies the permissions usage session lifetime. Each session older
// than this value is to be deleted.
constexpr base::TimeDelta kUsageSessionMaxAge = base::Days(90);

// Specifies the time period between the regular sessions deletions.
constexpr base::TimeDelta kUsageSessionCullingInterval = base::Minutes(30);

}  // namespace

namespace permissions {

PermissionAuditingService::PermissionAuditingService(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_task_runner_(backend_task_runner) {}

PermissionAuditingService::~PermissionAuditingService() {
  if (db_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
  }
}

void PermissionAuditingService::Init(const base::FilePath& database_path) {
  CHECK(!db_);
  db_ = std::make_unique<PermissionAuditingDatabase>();
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&PermissionAuditingDatabase::Init),
                     base::Unretained(db_.get()), database_path));
}

void PermissionAuditingService::StartPeriodicCullingOfExpiredSessions() {
  timer_.Start(
      FROM_HERE, kUsageSessionCullingInterval,
      base::BindRepeating(&PermissionAuditingService::ExpireOldSessions,
                          this->AsWeakPtr()));
}

void PermissionAuditingService::StorePermissionUsage(
    const PermissionUsageSession& session) {
  CHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&PermissionAuditingDatabase::StorePermissionUsage),
          base::Unretained(db_.get()), session));
}

void PermissionAuditingService::GetPermissionUsageHistory(
    ContentSettingsType type,
    const url::Origin& origin,
    base::Time start_time,
    PermissionUsageHistoryCallback result_callback) {
  CHECK(db_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PermissionAuditingDatabase::GetPermissionUsageHistory,
                     base::Unretained(db_.get()), type, origin, start_time),
      std::move(result_callback));
}

void PermissionAuditingService::GetLastPermissionUsageTime(
    ContentSettingsType type,
    const url::Origin& origin,
    LastPermissionUsageTimeCallback result_callback) {
  CHECK(db_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PermissionAuditingDatabase::GetLastPermissionUsageTime,
                     base::Unretained(db_.get()), type, origin),
      std::move(result_callback));
}

void PermissionAuditingService::UpdateEndTime(ContentSettingsType type,
                                              const url::Origin& origin,
                                              base::Time start_time,
                                              base::Time new_end_time) {
  CHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&PermissionAuditingDatabase::UpdateEndTime),
          base::Unretained(db_.get()), type, origin, start_time, new_end_time));
}

void PermissionAuditingService::DeleteSessionsBetween(base::Time start,
                                                      base::Time end) {
  CHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PermissionAuditingDatabase::DeleteSessionsBetween),
                     base::Unretained(db_.get()), start, end));
}

void PermissionAuditingService::ExpireOldSessions() {
  CHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PermissionAuditingDatabase::DeleteSessionsBetween),
                     base::Unretained(db_.get()), base::Time(),
                     base::Time::Now() - kUsageSessionMaxAge));
}

// static
base::TimeDelta PermissionAuditingService::GetUsageSessionMaxAge() {
  return kUsageSessionMaxAge;
}

// static
base::TimeDelta PermissionAuditingService::GetUsageSessionCullingInterval() {
  return kUsageSessionCullingInterval;
}

}  // namespace permissions
