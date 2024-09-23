// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_PROXY_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_PROXY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_connector.h"

namespace syncer {

// Proxies all DataTypeConnector calls to another thread. Typically used by
// the SyncBackend to call from the UI thread to the real DataTypeConnector on
// the sync thread.
class DataTypeConnectorProxy : public DataTypeConnector {
 public:
  DataTypeConnectorProxy(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<DataTypeConnector>& data_type_connector);
  ~DataTypeConnectorProxy() override;

  // DataTypeConnector implementation
  void ConnectDataType(
      DataType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) override;
  void DisconnectDataType(DataType type) override;

 private:
  // A SequencedTaskRunner representing the thread where the DataTypeConnector
  // lives.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The DataTypeConnector this object is wrapping.
  base::WeakPtr<DataTypeConnector> data_type_connector_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_PROXY_H_
