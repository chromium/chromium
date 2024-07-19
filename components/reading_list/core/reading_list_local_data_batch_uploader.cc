// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/sync/service/local_data_description.h"
#include "url/gurl.h"

namespace reading_list {

ReadingListLocalDataBatchUploader::ReadingListLocalDataBatchUploader(
    DualReadingListModel* dual_reading_list_model)
    : dual_reading_list_model_(dual_reading_list_model) {}

void ReadingListLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  if (!CanUpload()) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  base::flat_set<GURL> keys =
      dual_reading_list_model_->GetKeysThatNeedUploadToSyncServer();
  std::move(callback).Run(
      syncer::LocalDataDescription(std::vector(keys.begin(), keys.end())));
}

void ReadingListLocalDataBatchUploader::TriggerLocalDataMigration() {
  if (!CanUpload()) {
    return;
  }

  dual_reading_list_model_->MarkAllForUploadToSyncServerIfNeeded();
}

bool ReadingListLocalDataBatchUploader::CanUpload() const {
  // TODO(crbug.com/354146311): Check GetAccountModelIfSyncing() isn't null
  return dual_reading_list_model_ && dual_reading_list_model_->loaded();
}

}  // namespace reading_list
