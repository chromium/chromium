// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_

#include "components/sync/service/model_type_local_data_batch_uploader.h"

namespace reading_list {

class ReadingListLocalDataBatchUploader
    : public syncer::ModelTypeLocalDataBatchUploader {
 public:
  ReadingListLocalDataBatchUploader() = default;

  ReadingListLocalDataBatchUploader(const ReadingListLocalDataBatchUploader&) =
      delete;
  ReadingListLocalDataBatchUploader& operator=(
      const ReadingListLocalDataBatchUploader&) = delete;

  ~ReadingListLocalDataBatchUploader() override = default;

  // syncer::ModelTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;
};

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_
