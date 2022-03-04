// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

#include "base/rand_util.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "sql/database.h"

namespace segmentation_platform {

namespace {

bool SanityCheckUrl(const GURL& url, UrlId url_id) {
  return url.is_valid() && !url.is_empty() && !url_id.is_null();
}

}  // namespace

UkmDatabaseBackend::UkmDatabaseBackend(
    const base::FilePath& database_path,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : database_path_(database_path),
      callback_task_runner_(callback_task_runner),
      db_(sql::DatabaseOptions()),
      metrics_table_(&db_),
      url_table_(&db_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

UkmDatabaseBackend::~UkmDatabaseBackend() = default;

void UkmDatabaseBackend::InitDatabase(
    UkmDatabaseBackend::SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_.Open(database_path_)) {
    callback_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
    return;
  }
  bool result = metrics_table_.InitTable() && url_table_.InitTable();
  callback_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback), result));
}

void UkmDatabaseBackend::StoreUkmEntry(ukm::mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MetricsRowEventId event_id =
      MetricsRowEventId::FromUnsafeValue(base::RandUint64());
  // If we have an URL ID for the entry, then use it, otherwise the URL ID will
  // be updated when to all metrics when UpdateUrlForUkmSource() is called.
  UrlId url_id;
  auto it = source_to_url_.find(entry->source_id);
  if (it != source_to_url_.end())
    url_id = it->second;

  UkmMetricsTable::MetricsRow row = {
      .event_timestamp = base::Time::Now(),
      .url_id = url_id,
      .source_id = entry->source_id,
      .event_id = event_id,
      .event_hash = UkmEventHash::FromUnsafeValue(entry->event_hash)};
  for (const auto& metric_and_value : entry->metrics) {
    row.metric_hash = UkmMetricHash::FromUnsafeValue(metric_and_value.first);
    row.metric_value = metric_and_value.second;
    metrics_table_.AddUkmEvent(row);
  }
}

void UkmDatabaseBackend::UpdateUrlForUkmSource(ukm::SourceId source_id,
                                               const GURL& url,
                                               bool is_validated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UrlId url_id = UkmUrlTable::GenerateUrlId(url);
  if (!SanityCheckUrl(url, url_id))
    return;

  if (!url_table_.IsUrlInTable(url_id)) {
    if (is_validated) {
      url_table_.WriteUrl(url, url_id);
      // Remove from list so we don't add the URL again to table later.
      urls_not_validated_.erase(url_id);
    } else {
      urls_not_validated_.insert(url_id);
    }
  }
  // Keep track of source to URL ID mapping for future metrics.
  source_to_url_[source_id] = url_id;
  // Update all entries in metrics table with the URL ID.
  metrics_table_.UpdateUrlIdForSource(source_id, url_id);
}

void UkmDatabaseBackend::OnUrlValidated(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UrlId url_id = UkmUrlTable::GenerateUrlId(url);
  // Write URL to table only if it's needed and it's not already added.
  if (urls_not_validated_.count(url_id) && SanityCheckUrl(url, url_id)) {
    url_table_.WriteUrl(url, url_id);
    urls_not_validated_.erase(url_id);
  }
}

void UkmDatabaseBackend::RemoveUrls(const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<UrlId> url_ids;
  for (const GURL& url : urls) {
    UrlId id = UkmUrlTable::GenerateUrlId(url);
    // Do not accidentally remove all entries without URL (kInvalidUrlID).
    if (!SanityCheckUrl(url, id))
      continue;
    url_ids.push_back(id);
    urls_not_validated_.erase(id);
  }
  url_table_.RemoveUrls(url_ids);
  metrics_table_.DeleteEventsForUrls(url_ids);
}

}  // namespace segmentation_platform
