// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_model_type_connector.h"

#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

FakeModelTypeConnector::FakeModelTypeConnector() = default;

FakeModelTypeConnector::~FakeModelTypeConnector() = default;

void FakeModelTypeConnector::ConnectDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeModelTypeConnector::DisconnectDataType(ModelType type) {}

}  // namespace syncer
