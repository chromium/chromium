// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_MODEL_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_TEST_MOCK_MODEL_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback.h"
#include "components/sync/service/model_type_local_data_batch_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

struct LocalDataDescription;

class MockModelTypeLocalDataBatchUploader
    : public ModelTypeLocalDataBatchUploader {
 public:
  MockModelTypeLocalDataBatchUploader();

  MockModelTypeLocalDataBatchUploader(
      const MockModelTypeLocalDataBatchUploader&) = delete;
  MockModelTypeLocalDataBatchUploader& operator=(
      const MockModelTypeLocalDataBatchUploader&) = delete;

  ~MockModelTypeLocalDataBatchUploader() override;

  // ModelTypeLocalDataBatchUploader implementation.
  MOCK_METHOD(void,
              GetLocalDataDescription,
              (base::OnceCallback<void(syncer::LocalDataDescription)>),
              (override));
  MOCK_METHOD(void, TriggerLocalDataMigration, (), (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_MODEL_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
