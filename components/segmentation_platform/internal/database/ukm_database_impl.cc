// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

namespace segmentation_platform {

UkmDatabaseImpl::UkmDatabaseImpl(const base::FilePath& database_path,
                                 bool in_memory)
    : backend_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
               database_path,
               in_memory) {}

UkmDatabaseImpl::~UkmDatabaseImpl() = default;

void UkmDatabaseImpl::InitDatabase(SuccessCallback callback) {
  backend_.AsyncCall(&UkmDatabaseBackend::InitDatabase)
      .Then(std::move(callback));
}

void UkmDatabaseImpl::StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) {
  backend_.AsyncCall(&UkmDatabaseBackend::StoreUkmEntry)
      .WithArgs(std::move(ukm_entry));
}

void UkmDatabaseImpl::UpdateUrlForUkmSource(ukm::SourceId source_id,
                                            const GURL& url,
                                            bool is_validated,
                                            const std::string& profile_id) {
  backend_.AsyncCall(&UkmDatabaseBackend::UpdateUrlForUkmSource)
      .WithArgs(source_id, url, is_validated, profile_id);
}

void UkmDatabaseImpl::OnUrlValidated(const GURL& url,
                                     const std::string& profile_id) {
  backend_.AsyncCall(&UkmDatabaseBackend::OnUrlValidated)
      .WithArgs(url, profile_id);
}

void UkmDatabaseImpl::RemoveUrls(const std::vector<GURL>& urls, bool all_urls) {
  backend_.AsyncCall(&UkmDatabaseBackend::RemoveUrls).WithArgs(urls, all_urls);
}

void UkmDatabaseImpl::AddUmaMetric(const std::string& profile_id,
                                   const UmaMetricEntry& row) {
  backend_.AsyncCall(&UkmDatabaseBackend::AddUmaMetric)
      .WithArgs(profile_id, row);
}

void UkmDatabaseImpl::RunReadOnlyQueries(QueryList&& queries,
                                         QueryCallback callback) {
  backend_.AsyncCall(&UkmDatabaseBackend::RunReadOnlyQueries)
      .WithArgs(std::move(queries))
      .Then(std::move(callback));
}

void UkmDatabaseImpl::CleanupOldEntries(base::Time ukm_time_limit,
                                        base::Time uma_time_limit) {
  backend_.AsyncCall(&UkmDatabaseBackend::CleanupOldEntries)
      .WithArgs(ukm_time_limit, uma_time_limit);
}

void UkmDatabaseImpl::CleanupItems(const std::string& profile_id,
                                   std::vector<CleanupItem> cleanup_items) {
  backend_.AsyncCall(&UkmDatabaseBackend::CleanupItems)
      .WithArgs(profile_id, std::move(cleanup_items));
}

void UkmDatabaseImpl::CommitTransactionForTesting() {
  backend_.AsyncCall(&UkmDatabaseBackend::CommitTransactionForTesting);
}

}  // namespace segmentation_platform
