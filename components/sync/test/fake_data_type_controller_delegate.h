// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/model_error.h"

namespace sync_pb {
class DataTypeState;
}  // namespace sync_pb

namespace syncer {

class FakeDataTypeControllerDelegate : public DataTypeControllerDelegate {
 public:
  explicit FakeDataTypeControllerDelegate(DataType type);

  FakeDataTypeControllerDelegate(const FakeDataTypeControllerDelegate&) =
      delete;
  FakeDataTypeControllerDelegate& operator=(
      const FakeDataTypeControllerDelegate&) = delete;

  ~FakeDataTypeControllerDelegate() override;

  // Determines the DataTypeState returned in Connect() as part of
  // DataTypeActivationResponse.
  void SetDataTypeStateForActivationResponse(
      const sync_pb::DataTypeState& data_type_state);

  // Influences the bit |skip_engine_connection| returned in Connect() as part
  // of DataTypeActivationResponse.
  void EnableSkipEngineConnectionForActivationResponse();

  // By default, this delegate (model) completes startup automatically when
  // OnSyncStarting() is invoked. For tests that want to manually control the
  // completion, or mimic errors during startup, EnableManualModelStart() can
  // be used, which means OnSyncStarting() will only complete when
  // SimulateModelStartFinished() is called.
  void EnableManualModelStart();
  void SimulateModelStartFinished();

  // Simulates a model running into an error (e.g. IO failed). This can happen
  // before or after the model has started/loaded.
  void SimulateModelError(const ModelError& error);

  // The number of times sync metadata was cleared, via either
  // OnSyncStopping(CLEAR_METADATA) or ClearMetadataIfStopped() while sync
  // was actually stopped.
  // TODO(crbug.com/40945017): Replace this with something like "HasMetadata".
  int clear_metadata_count() const;

  // DataTypeControllerDelegate overrides
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void HasUnsyncedData(base::OnceCallback<void(bool)> callback) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

  base::WeakPtr<DataTypeControllerDelegate> GetWeakPtr();

 private:
  std::unique_ptr<DataTypeActivationResponse> MakeActivationResponse() const;

  const DataType type_;
  bool manual_model_start_enabled_ = false;
  bool sync_started_ = false;
  int clear_metadata_count_ = 0;
  DataTypeActivationResponse activation_response_;
  std::optional<ModelError> model_error_;
  StartCallback start_callback_;
  ModelErrorHandler error_handler_;
  base::WeakPtrFactory<FakeDataTypeControllerDelegate> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_CONTROLLER_DELEGATE_H_
