// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
#define COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_

#include <memory>

#include "components/sync/engine/model_type_connector.h"

namespace syncer {

// A no-op implementation of ModelTypeConnector for testing.
class FakeModelTypeConnector : public ModelTypeConnector {
 public:
  FakeModelTypeConnector();
  ~FakeModelTypeConnector() override;

  void ConnectDataType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) override;
  void DisconnectDataType(ModelType type) override;
  void SetProxyTabsDatatypeEnabled(bool enabled) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
