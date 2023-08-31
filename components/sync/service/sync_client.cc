// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"

namespace syncer {

void SyncClient::GetLocalDataDescriptions(
    ModelTypeSet types,
    base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
        callback) {
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableBatchUploadLocalDataWithDummyDataForTesting)) {
    // Create dummy data.
    std::map<syncer::ModelType, syncer::LocalDataDescription> result;
    if (types.Has(syncer::PASSWORDS)) {
      result.emplace(syncer::PASSWORDS,
                     syncer::LocalDataDescription{
                         syncer::PASSWORDS, 5,
                         std::vector<std::string>{"amazon.de", "airbnb.com",
                                                  "facebook.com"}});
    }
    if (types.Has(syncer::BOOKMARKS)) {
      result.emplace(syncer::BOOKMARKS,
                     syncer::LocalDataDescription{
                         syncer::BOOKMARKS, 2,
                         std::vector<std::string>{"amazon.de", "airbnb.com"}});
    }
    if (types.Has(syncer::READING_LIST)) {
      result.emplace(
          syncer::READING_LIST,
          syncer::LocalDataDescription{
              syncer::READING_LIST, 2,
              std::vector<std::string>{"medium.com", "nytimes.com"}});
    }

    // Run the callback asynchronously with configurable delay
    // SyncResponseDelayForBatchUploadLocalDataWithDummyDataForTesting.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)),
        syncer::kSyncResponseDelayForBatchUploadLocalDataWithDummyDataForTesting
            .Get());
    return;
  }

  NOTIMPLEMENTED() << "SyncClient implementations should implement this.";
}

void SyncClient::TriggerLocalDataMigration(ModelTypeSet types) {
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableBatchUploadLocalDataWithDummyDataForTesting)) {
    // Return directly since there is nothing to do with the dummy data.
    return;
  }

  NOTIMPLEMENTED() << "SyncClient implementations should implement this.";
}
}  // namespace syncer
