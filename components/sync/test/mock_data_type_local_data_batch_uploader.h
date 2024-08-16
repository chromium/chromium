// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

struct LocalDataDescription;

class MockDataTypeLocalDataBatchUploader
    : public DataTypeLocalDataBatchUploader {
 public:
  MockDataTypeLocalDataBatchUploader();

  MockDataTypeLocalDataBatchUploader(
      const MockDataTypeLocalDataBatchUploader&) = delete;
  MockDataTypeLocalDataBatchUploader& operator=(
      const MockDataTypeLocalDataBatchUploader&) = delete;

  ~MockDataTypeLocalDataBatchUploader() override;

  // DataTypeLocalDataBatchUploader implementation.
  MOCK_METHOD(void,
              GetLocalDataDescription,
              (base::OnceCallback<void(syncer::LocalDataDescription)>),
              (override));
  MOCK_METHOD(void, TriggerLocalDataMigration, (), (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
