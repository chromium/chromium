// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_STORE_H_
#define COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_STORE_H_

#include <memory>

#include "components/sync/model/data_type_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockDataTypeStore : public DataTypeStore {
 public:
  MockDataTypeStore();
  ~MockDataTypeStore() override;

  MOCK_METHOD(void,
              ReadData,
              (const IdList& id_list, ReadDataCallback callback),
              (override));
  MOCK_METHOD(void, ReadAllData, (ReadAllDataCallback callback), (override));
  MOCK_METHOD(void,
              ReadAllMetadata,
              (ReadMetadataCallback callback),
              (override));
  MOCK_METHOD(void,
              ReadAllDataAndMetadata,
              (ReadAllDataAndMetadataCallback callback),
              (override));
  MOCK_METHOD(void,
              ReadAllDataAndPreprocess,
              (PreprocessCallback preprocess_on_backend_sequence_callback,
               CallbackWithResult completion_on_frontend_sequence_callback),
              (override));
  MOCK_METHOD(std::unique_ptr<WriteBatch>, CreateWriteBatch, (), (override));
  MOCK_METHOD(void,
              CommitWriteBatch,
              (std::unique_ptr<WriteBatch> write_batch,
               CallbackWithResult callback),
              (override));
  MOCK_METHOD(void,
              DeleteAllDataAndMetadata,
              (CallbackWithResult callback),
              (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_DATA_TYPE_STORE_H_
