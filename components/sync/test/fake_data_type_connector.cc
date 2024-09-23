// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_connector.h"

#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

FakeDataTypeConnector::FakeDataTypeConnector() = default;

FakeDataTypeConnector::~FakeDataTypeConnector() = default;

void FakeDataTypeConnector::ConnectDataType(
    DataType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeDataTypeConnector::DisconnectDataType(DataType type) {}

}  // namespace syncer
