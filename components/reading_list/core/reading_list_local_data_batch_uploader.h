// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace reading_list {

class DualReadingListModel;

class ReadingListLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  // `dual_reading_list_model` must either be null or non-null and outlive this
  // object.
  explicit ReadingListLocalDataBatchUploader(
      DualReadingListModel* dual_reading_list_model);

  ReadingListLocalDataBatchUploader(const ReadingListLocalDataBatchUploader&) =
      delete;
  ReadingListLocalDataBatchUploader& operator=(
      const ReadingListLocalDataBatchUploader&) = delete;

  ~ReadingListLocalDataBatchUploader() override = default;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;

 private:
  bool CanUpload() const;

  const raw_ptr<DualReadingListModel> dual_reading_list_model_;
};

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_LOCAL_DATA_BATCH_UPLOADER_H_
