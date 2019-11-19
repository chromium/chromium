// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_downloader_quota.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/variations/variations_associated_data.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {

// static
const int64_t PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes =
    20LL * 1024 * 1024;  // 20 MB

namespace {
static const char kMaxDailyQuotaBytesParamName[] =
    "offline_pages_max_daily_quota_bytes";

constexpr base::TimeDelta kQuotaPeriod = base::TimeDelta::FromDays(1);

// Normalize quota to [0, GetMaxDailyQuotaBytes()].
int64_t NormalizeQuota(int64_t quota) {
  if (quota < 0)
    return 0;
  if (quota > PrefetchDownloaderQuota::GetMaxDailyQuotaBytes())
    return PrefetchDownloaderQuota::GetMaxDailyQuotaBytes();
  return quota;
}

}  // namespace

PrefetchDownloaderQuota::PrefetchDownloaderQuota(sql::Database* db,
                                                 const base::Clock* clock)
    : db_(db), clock_(clock) {
  DCHECK(db_);
  DCHECK(clock_);
}

PrefetchDownloaderQuota::~PrefetchDownloaderQuota() = default;

int64_t PrefetchDownloaderQuota::GetMaxDailyQuotaBytes() {
  std::string quota_bytes_as_string(variations::GetVariationParamValueByFeature(
      offline_pages::kPrefetchingOfflinePagesFeature,
      kMaxDailyQuotaBytesParamName));
  if (quota_bytes_as_string.empty())
    return kDefaultMaxDailyQuotaBytes;
  int64_t quota_bytes = 0;
  if (!base::StringToInt64(quota_bytes_as_string, &quota_bytes) ||
      quota_bytes < 0) {
    DLOG(WARNING) << "Invalid field trial param "
                  << kMaxDailyQuotaBytesParamName << " under feature "
                  << offline_pages::kPrefetchingOfflinePagesFeature.name
                  << " with string value " << quota_bytes_as_string
                  << ". Falling back to default value of "
                  << kDefaultMaxDailyQuotaBytes;
    return kDefaultMaxDailyQuotaBytes;
  }
  return quota_bytes;
}

int64_t PrefetchDownloaderQuota::GetAvailableQuotaBytes() {
  static const char kSql[] =
      "SELECT update_time, available_quota FROM prefetch_downloader_quota"
      " WHERE quota_id = 1";
  if (db_ == nullptr || clock_ == nullptr)
    return -1LL;

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  if (!statement.Step()) {
    if (!statement.Succeeded())
      return -1LL;

    return GetMaxDailyQuotaBytes();
  }

  base::Time update_time =
      store_utils::FromDatabaseTime(statement.ColumnInt64(0));
  int64_t available_quota = statement.ColumnInt64(1);

  int64_t remaining_quota =
      available_quota +
      (GetMaxDailyQuotaBytes() * (clock_->Now() - update_time)) / kQuotaPeriod;

  if (remaining_quota < 0)
    SetAvailableQuotaBytes(0);

  return NormalizeQuota(remaining_quota);
}

bool PrefetchDownloaderQuota::SetAvailableQuotaBytes(int64_t quota) {
  static const char kSql[] =
      "INSERT OR REPLACE INTO prefetch_downloader_quota"
      " (quota_id, update_time, available_quota)"
      " VALUES (1, ?, ?)";
  if (db_ == nullptr || clock_ == nullptr)
    return false;

  quota = NormalizeQuota(quota);

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(clock_->Now()));
  statement.BindInt64(1, quota);
  return statement.Run();
}

}  // namespace offline_pages
