// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database.h"

#include "base/rand_util.h"
#include "base/task/thread_pool.h"

namespace segmentation_platform {

UkmDatabase::UkmDatabase(const base::FilePath& database_path)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

// TODO(ssid): Delete backend in right task runner.
UkmDatabase::~UkmDatabase() = default;

void UkmDatabase::InitDatabase(UkmDatabase::InitCallback callback) {
  // TODO(ssid): Implement
}

void UkmDatabase::UkmEntryAdded(ukm::mojom::UkmEntryPtr ukm_entry) {
  // TODO(ssid): Implement
}

void UkmDatabase::UkmSourceUrlUpdated(ukm::SourceId source_id,
                                      const GURL& url,
                                      bool is_validated) {
  // TODO(ssid): Implement
}

void UkmDatabase::OnUrlValidated(const GURL& url) {
  // TODO(ssid): Implement
}

void UkmDatabase::RemoveUrls(const std::vector<GURL>& urls) {
  // TODO(ssid): Implement
}

}  // namespace segmentation_platform
