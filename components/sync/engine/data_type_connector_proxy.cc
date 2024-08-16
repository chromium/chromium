// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_connector_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

DataTypeConnectorProxy::DataTypeConnectorProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<DataTypeConnector>& data_type_connector)
    : task_runner_(task_runner), data_type_connector_(data_type_connector) {}

DataTypeConnectorProxy::~DataTypeConnectorProxy() = default;

void DataTypeConnectorProxy::ConnectDataType(
    DataType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataTypeConnector::ConnectDataType, data_type_connector_,
                     type, std::move(activation_response)));
}

void DataTypeConnectorProxy::DisconnectDataType(DataType type) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DataTypeConnector::DisconnectDataType,
                                        data_type_connector_, type));
}

}  // namespace syncer
