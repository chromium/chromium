// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONNECTOR_H_
#define COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONNECTOR_H_

#include <memory>

#include "components/sync/engine/data_type_connector.h"

namespace syncer {

// A no-op implementation of DataTypeConnector for testing.
class FakeDataTypeConnector : public DataTypeConnector {
 public:
  FakeDataTypeConnector();
  ~FakeDataTypeConnector() override;

  void ConnectDataType(
      DataType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) override;
  void DisconnectDataType(DataType type) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONNECTOR_H_
