// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"

class GURL;

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
  syncer::LocalDataDescription local_data_description =
      syncer::LocalDataDescription(std::vector(keys.begin(), keys.end()));

  if (base::FeatureList::IsEnabled(
          syncer::kSyncReadingListBatchUploadSelectedItems)) {
    local_data_description.type = syncer::DataType::READING_LIST;
    for (const GURL& key : keys) {
      local_data_description.local_data_models.push_back(
          DataItemModelFromURL(key));
    }
  }

  std::move(callback).Run(local_data_description);
}

void ReadingListLocalDataBatchUploader::TriggerLocalDataMigration() {
  if (!CanUpload()) {
    return;
  }

  dual_reading_list_model_->MarkAllForUploadToSyncServerIfNeeded();
}

void ReadingListLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  CHECK(base::FeatureList::IsEnabled(
      syncer::kSyncReadingListBatchUploadSelectedItems));

  if (!CanUpload()) {
    return;
  }

  dual_reading_list_model_->MarkEntriesForUploadToSyncServerIfNeeded(
      base::MakeFlatSet<GURL>(
          items, /*comp=*/{},
          /*proj=*/[](const syncer::LocalDataItemModel::DataId& id) {
            return std::get<GURL>(id);
          }));
}

bool ReadingListLocalDataBatchUploader::CanUpload() const {
  // TODO(crbug.com/354146311): Check GetAccountModelIfSyncing() isn't null
  return dual_reading_list_model_ && dual_reading_list_model_->loaded();
}

syncer::LocalDataItemModel
ReadingListLocalDataBatchUploader::DataItemModelFromURL(const GURL& url) const {
  syncer::LocalDataItemModel item;
  item.id = url;
  item.icon = syncer::LocalDataItemModel::PageUrlIcon(url);
  item.title = dual_reading_list_model_->GetEntryByURL(url)->Title();
  return item;
}

}  // namespace reading_list
