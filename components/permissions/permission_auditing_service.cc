// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_auditing_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
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
    backend_task_runner_->DeleteSoon(FROM_HERE, db_.get());
    db_ = nullptr;
  }
}

void PermissionAuditingService::Init(const base::FilePath& database_path) {
  DCHECK(!db_);
  db_ = new PermissionAuditingDatabase();
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&PermissionAuditingDatabase::Init),
                     base::Unretained(db_), database_path));
}

void PermissionAuditingService::StartPeriodicCullingOfExpiredSessions() {
  timer_.Start(
      FROM_HERE, kUsageSessionCullingInterval,
      base::BindRepeating(&PermissionAuditingService::ExpireOldSessions,
                          this->AsWeakPtr()));
}

void PermissionAuditingService::StorePermissionUsage(
    const PermissionUsageSession& session) {
  DCHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&PermissionAuditingDatabase::StorePermissionUsage),
          base::Unretained(db_), session));
}

void PermissionAuditingService::GetPermissionUsageHistory(
    ContentSettingsType type,
    const url::Origin& origin,
    base::Time start_time,
    PermissionUsageHistoryCallback result_callback) {
  DCHECK(db_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PermissionAuditingDatabase::GetPermissionUsageHistory,
                     base::Unretained(db_), type, origin, start_time),
      std::move(result_callback));
}

void PermissionAuditingService::GetLastPermissionUsageTime(
    ContentSettingsType type,
    const url::Origin& origin,
    LastPermissionUsageTimeCallback result_callback) {
  DCHECK(db_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PermissionAuditingDatabase::GetLastPermissionUsageTime,
                     base::Unretained(db_), type, origin),
      std::move(result_callback));
}

void PermissionAuditingService::UpdateEndTime(ContentSettingsType type,
                                              const url::Origin& origin,
                                              base::Time start_time,
                                              base::Time new_end_time) {
  DCHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&PermissionAuditingDatabase::UpdateEndTime),
          base::Unretained(db_), type, origin, start_time, new_end_time));
}

void PermissionAuditingService::DeleteSessionsBetween(base::Time start,
                                                      base::Time end) {
  DCHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PermissionAuditingDatabase::DeleteSessionsBetween),
                     base::Unretained(db_), start, end));
}

void PermissionAuditingService::ExpireOldSessions() {
  DCHECK(db_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PermissionAuditingDatabase::DeleteSessionsBetween),
                     base::Unretained(db_), base::Time(),
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
