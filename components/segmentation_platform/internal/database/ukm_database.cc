// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database.h"

#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

namespace segmentation_platform {

UkmDatabase::UkmDatabase(const base::FilePath& database_path)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      backend_(std::make_unique<UkmDatabaseBackend>(database_path,
                                                    backend_task_runner_)) {}

UkmDatabase::~UkmDatabase() {
  backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
}

void UkmDatabase::InitDatabase() {
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::InitDatabase,
                                backend_->GetWeakPtr(), base::DoNothing()));
}

void UkmDatabase::StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) {
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::StoreUkmEntry,
                                backend_->GetWeakPtr(), std::move(ukm_entry)));
}

void UkmDatabase::UpdateUrlForUkmSource(ukm::SourceId source_id,
                                        const GURL& url,
                                        bool is_validated) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmDatabaseBackend::UpdateUrlForUkmSource,
                     backend_->GetWeakPtr(), source_id, url, is_validated));
}

void UkmDatabase::OnUrlValidated(const GURL& url) {
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmDatabaseBackend::OnUrlValidated,
                                backend_->GetWeakPtr(), url));
}

void UkmDatabase::RemoveUrls(const std::vector<GURL>& urls) {
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(&UkmDatabaseBackend::RemoveUrls,
                                                backend_->GetWeakPtr(), urls));
}

UkmDatabase::CustomSqlQuery::CustomSqlQuery() = default;

UkmDatabase::CustomSqlQuery::CustomSqlQuery(CustomSqlQuery&&) = default;

UkmDatabase::CustomSqlQuery::CustomSqlQuery(
    const base::StringPiece& query,
    const std::vector<ProcessedValue>& bind_values)
    : query(query), bind_values(bind_values) {}

UkmDatabase::CustomSqlQuery::~CustomSqlQuery() = default;

void UkmDatabase::RunReadonlyQueries(const QueryList& queries,
                                     QueryCallback callback) {
  // TODO(haileywang): Implement.
}

}  // namespace segmentation_platform
