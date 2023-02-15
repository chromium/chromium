// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/database_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/common/database/database_identifier.h"

using content::BrowserThread;
using content::StorageUsageInfo;
using storage::DatabaseIdentifier;

namespace browsing_data {

DatabaseHelper::DatabaseHelper(content::StoragePartition* storage_partition)
    : tracker_(storage_partition->GetDatabaseTracker()) {}

DatabaseHelper::~DatabaseHelper() {}

void DatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  tracker_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](storage::DatabaseTracker* tracker) {
            std::list<StorageUsageInfo> result;
            std::vector<storage::OriginInfo> origins_info;
            if (tracker->GetAllOriginsInfo(&origins_info)) {
              for (const storage::OriginInfo& info : origins_info) {
                url::Origin origin = storage::GetOriginFromIdentifier(
                    info.GetOriginIdentifier());
                if (!HasWebScheme(origin.GetURL()))
                  continue;
                result.emplace_back(blink::StorageKey::CreateFirstParty(origin),
                                    info.TotalSize(), info.LastModified());
              }
            }
            return result;
          },
          base::RetainedRef(tracker_)),
      std::move(callback));
}

void DatabaseHelper::DeleteDatabase(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&storage::DatabaseTracker::DeleteDataForOrigin,
                                tracker_, origin, base::DoNothing()));
}

CannedDatabaseHelper::CannedDatabaseHelper(
    content::StoragePartition* storage_partition)
    : DatabaseHelper(storage_partition) {}

void CannedDatabaseHelper::Add(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.
  pending_origins_.insert(origin);
}

void CannedDatabaseHelper::Reset() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_origins_.clear();
}

bool CannedDatabaseHelper::empty() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.empty();
}

size_t CannedDatabaseHelper::GetCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedDatabaseHelper::GetOrigins() {
  return pending_origins_;
}

void CannedDatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_) {
    result.emplace_back(blink::StorageKey::CreateFirstParty(origin), 0,
                        base::Time());
  }

  std::move(callback).Run(result);
}

void CannedDatabaseHelper::DeleteDatabase(const url::Origin& origin) {
  pending_origins_.erase(origin);
  DatabaseHelper::DeleteDatabase(origin);
}

CannedDatabaseHelper::~CannedDatabaseHelper() {}

}  // namespace browsing_data
