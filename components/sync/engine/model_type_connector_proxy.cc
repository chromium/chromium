// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_connector_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

ModelTypeConnectorProxy::ModelTypeConnectorProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<ModelTypeConnector>& model_type_connector)
    : task_runner_(task_runner), model_type_connector_(model_type_connector) {}

ModelTypeConnectorProxy::~ModelTypeConnectorProxy() = default;

void ModelTypeConnectorProxy::ConnectDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ModelTypeConnector::ConnectDataType,
                                        model_type_connector_, type,
                                        std::move(activation_response)));
}

void ModelTypeConnectorProxy::DisconnectDataType(ModelType type) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ModelTypeConnector::DisconnectDataType,
                                        model_type_connector_, type));
}

void ModelTypeConnectorProxy::SetProxyTabsDatatypeEnabled(bool enabled) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelTypeConnector::SetProxyTabsDatatypeEnabled,
                     model_type_connector_, enabled));
}

}  // namespace syncer
