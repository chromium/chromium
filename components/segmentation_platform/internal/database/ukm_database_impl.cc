// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

namespace segmentation_platform {

UkmDatabaseImpl::UkmDatabaseImpl(const base::FilePath& database_path,
                                 bool in_memory)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      backend_(std::make_unique<UkmDatabaseBackend>(
          database_path,
          in_memory,
          base::SequencedTaskRunner::GetCurrentDefault())) {}

UkmDatabaseImpl::~UkmDatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
}

void UkmDatabaseImpl::InitDatabase(SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::InitDatabase,
                                backend_->GetWeakPtr(), std::move(callback)));
}

void UkmDatabaseImpl::StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::StoreUkmEntry,
                                backend_->GetWeakPtr(), std::move(ukm_entry)));
}

void UkmDatabaseImpl::UpdateUrlForUkmSource(ukm::SourceId source_id,
                                            const GURL& url,
                                            bool is_validated,
                                            const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::UpdateUrlForUkmSource,
                                backend_->GetWeakPtr(), source_id, url,
                                is_validated, profile_id));
}

void UkmDatabaseImpl::OnUrlValidated(const GURL& url,
                                     const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::OnUrlValidated,
                                backend_->GetWeakPtr(), url, profile_id));
}

void UkmDatabaseImpl::RemoveUrls(const std::vector<GURL>& urls, bool all_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::RemoveUrls,
                                backend_->GetWeakPtr(), urls, all_urls));
}

void UkmDatabaseImpl::AddUmaMetric(const std::string& profile_id,
                                   const UmaMetricEntry& row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::AddUmaMetric,
                                backend_->GetWeakPtr(), profile_id, row));
}

void UkmDatabaseImpl::RunReadOnlyQueries(QueryList&& queries,
                                         QueryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::RunReadOnlyQueries,
                                backend_->GetWeakPtr(), std::move(queries),
                                std::move(callback)));
}

void UkmDatabaseImpl::DeleteEntriesOlderThan(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::DeleteEntriesOlderThan,
                                backend_->GetWeakPtr(), time));
}

void UkmDatabaseImpl::CleanupItems(const std::string& profile_id,
                                   std::vector<CleanupItem> cleanup_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmDatabaseBackend::CleanupItems, backend_->GetWeakPtr(),
                     profile_id, std::move(cleanup_items)));
}

void UkmDatabaseImpl::CommitTransactionForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmDatabaseBackend::CommitTransactionForTesting,
                     backend_->GetWeakPtr()));
}

}  // namespace segmentation_platform
