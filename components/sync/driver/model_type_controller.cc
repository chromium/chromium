// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"

namespace syncer {
namespace {

void ReportErrorOnModelThread(
    scoped_refptr<base::SequencedTaskRunner> ui_thread,
    const ModelErrorHandler& error_handler,
    const ModelError& error) {
  ui_thread->PostTask(error.location(), base::BindOnce(error_handler, error));
}

// Takes the strictest policy for clearing sync metadata.
SyncStopMetadataFate TakeStrictestMetadataFate(SyncStopMetadataFate fate1,
                                               SyncStopMetadataFate fate2) {
  switch (fate1) {
    case CLEAR_METADATA:
      return CLEAR_METADATA;
    case KEEP_METADATA:
      return fate2;
  }
  NOTREACHED();
  return KEEP_METADATA;
}

}  // namespace

ModelTypeController::ModelTypeController(ModelType type)
    : DataTypeController(type) {}

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode)
    : ModelTypeController(type) {
  InitModelTypeController(std::move(delegate_for_full_sync_mode), nullptr);
}

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode)
    : ModelTypeController(type) {
  InitModelTypeController(std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode));
}

ModelTypeController::~ModelTypeController() = default;

void ModelTypeController::InitModelTypeController(
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode) {
  DCHECK(delegate_map_.empty());
  delegate_map_.emplace(SyncMode::kFull,
                        std::move(delegate_for_full_sync_mode));
  if (delegate_for_transport_mode) {
    delegate_map_.emplace(SyncMode::kTransportOnly,
                          std::move(delegate_for_transport_mode));
  }
}

void ModelTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(model_load_callback);
  DCHECK_EQ(NOT_RUNNING, state_);

  auto it = delegate_map_.find(configure_context.sync_mode);
  DCHECK(it != delegate_map_.end()) << ModelTypeToDebugString(type());
  delegate_ = it->second.get();
  DCHECK(delegate_);

  DVLOG(1) << "Sync starting for " << ModelTypeToDebugString(type());
  state_ = MODEL_STARTING;
  model_load_callback_ = model_load_callback;

  DataTypeActivationRequest request;
  request.error_handler = base::BindRepeating(
      &ReportErrorOnModelThread, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&ModelTypeController::ReportModelError,
                          base::AsWeakPtr(this), SyncError::DATATYPE_ERROR));
  request.authenticated_account_id = configure_context.authenticated_account_id;
  request.cache_guid = configure_context.cache_guid;
  request.sync_mode = configure_context.sync_mode;
  request.configuration_start_time = configure_context.configuration_start_time;

  // Note that |request.authenticated_account_id| may be empty for local sync.
  DCHECK(!request.cache_guid.empty());

  // Ask the delegate to actually start the datatype.
  delegate_->OnSyncStarting(
      request, base::BindOnce(&ModelTypeController::OnDelegateStarted,
                              base::AsWeakPtr(this)));
}

std::unique_ptr<DataTypeActivationResponse> ModelTypeController::Connect() {
  DCHECK(CalledOnValidThread());
  DCHECK(activation_response_);
  DCHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;
  DVLOG(1) << "Sync running for " << ModelTypeToDebugString(type());

  return std::move(activation_response_);
}

void ModelTypeController::Stop(ShutdownReason reason, StopCallback callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(delegate_ || state() == NOT_RUNNING || state() == FAILED);

  // Leave metadata if we do not disable sync completely.
  SyncStopMetadataFate metadata_fate = KEEP_METADATA;
  switch (reason) {
    case ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
      break;
    case ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
      metadata_fate = CLEAR_METADATA;
      break;
    case ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      break;
  }

  switch (state()) {
    case NOT_RUNNING:
    case FAILED:
      // Nothing to stop.
      std::move(callback).Run();
      // Clear metadata if needed.
      if (base::FeatureList::IsEnabled(
              kSyncAllowClearingMetadataWhenDataTypeIsStopped) &&
          metadata_fate == CLEAR_METADATA) {
        ClearMetadataWhileStopped();
      }
      return;

    case STOPPING:
      DCHECK(!model_stop_callbacks_.empty());
      model_stop_metadata_fate_ =
          TakeStrictestMetadataFate(model_stop_metadata_fate_, metadata_fate);
      model_stop_callbacks_.push_back(std::move(callback));
      // This just means stopping was requested while starting the data type.
      // Metadata will cleared (if CLEAR_METADATA) in OnSyncStopping.
      break;

    case MODEL_STARTING:
      DCHECK(model_load_callback_);
      DCHECK(model_stop_callbacks_.empty());
      DLOG(WARNING) << "Deferring stop for " << ModelTypeToDebugString(type())
                    << " because it's still starting";
      model_load_callback_.Reset();
      model_stop_metadata_fate_ = metadata_fate;
      model_stop_callbacks_.push_back(std::move(callback));
      // The actual stop will be executed when the starting process is finished.
      state_ = STOPPING;
      break;

    case MODEL_LOADED:
    case RUNNING:
      DVLOG(1) << "Stopping sync for " << ModelTypeToDebugString(type());
      model_load_callback_.Reset();
      state_ = NOT_RUNNING;
      delegate_->OnSyncStopping(metadata_fate);
      delegate_ = nullptr;
      std::move(callback).Run();
      break;
  }
}

DataTypeController::State ModelTypeController::state() const {
  return state_;
}

bool ModelTypeController::ShouldRunInTransportOnlyMode() const {
  // By default, running in transport-only mode is enabled if the corresponding
  // delegate exists, i.e. the controller is aware of transport-only mode and
  // supports it in principle. Subclass can still override this with more
  // specific logic.
  return delegate_map_.count(SyncMode::kTransportOnly) != 0;
}

void ModelTypeController::GetAllNodes(AllNodesCallback callback) {
  DCHECK(delegate_);
  delegate_->GetAllNodesForDebugging(std::move(callback));
}

void ModelTypeController::GetTypeEntitiesCount(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  if (delegate_) {
    delegate_->GetTypeEntitiesCountForDebugging(std::move(callback));
  } else {
    std::move(callback).Run(TypeEntitiesCount(type()));
  }
}

void ModelTypeController::RecordMemoryUsageAndCountsHistograms() {
  DCHECK(delegate_);
  delegate_->RecordMemoryUsageAndCountsHistograms();
}

ModelTypeControllerDelegate* ModelTypeController::GetDelegateForTesting(
    SyncMode sync_mode) {
  auto it = delegate_map_.find(sync_mode);
  return it != delegate_map_.end() ? it->second.get() : nullptr;
}

void ModelTypeController::ReportModelError(SyncError::ErrorType error_type,
                                           const ModelError& error) {
  DCHECK(CalledOnValidThread());

  switch (state_) {
    case MODEL_LOADED:
    // Fall through. Before state_ is flipped to RUNNING, we treat it as a
    // start failure. This is somewhat arbitrary choice.
    case STOPPING:
    // Fall through. We treat it the same as starting as this means stopping was
    // requested while starting the data type.
    case MODEL_STARTING:
      RecordStartFailure();
      break;
    case RUNNING:
      RecordRunFailure();
      break;
    case NOT_RUNNING:
      // Error could arrive too late, e.g. after the datatype has been stopped.
      // This is allowed for the delegate's convenience, so there's no
      // constraints around when exactly
      // DataTypeActivationRequest::error_handler is supposed to be used (it can
      // be used at any time). This also simplifies the implementation of
      // task-posting delegates.
      state_ = FAILED;
      return;
    case FAILED:
      // Do not record for the second time and exit early.
      return;
  }

  state_ = FAILED;

  TriggerCompletionCallbacks(
      SyncError(error.location(), error_type, error.message(), type()));
}

void ModelTypeController::RecordStartFailure() const {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures2",
                            ModelTypeHistogramValue(type()));
}

void ModelTypeController::RecordRunFailure() const {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures2",
                            ModelTypeHistogramValue(type()));
}

void ModelTypeController::OnDelegateStarted(
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(CalledOnValidThread());

  switch (state_) {
    case STOPPING:
      DCHECK(!model_stop_callbacks_.empty());
      DCHECK(!model_load_callback_);
      state_ = NOT_RUNNING;
      [[fallthrough]];
    case FAILED:
      DVLOG(1) << "Successful sync start completion received late for "
               << ModelTypeToDebugString(type())
               << ", it has been stopped meanwhile";
      delegate_->OnSyncStopping(model_stop_metadata_fate_);
      delegate_ = nullptr;
      break;
    case MODEL_STARTING:
      DCHECK(model_stop_callbacks_.empty());
      // Hold on to the activation context until Connect is called.
      activation_response_ = std::move(activation_response);
      state_ = MODEL_LOADED;
      DVLOG(1) << "Sync start completed for " << ModelTypeToDebugString(type());
      break;
    case MODEL_LOADED:
    case RUNNING:
    case NOT_RUNNING:
      NOTREACHED() << " type " << ModelTypeToDebugString(type()) << " state "
                   << StateToString(state_);
  }

  TriggerCompletionCallbacks(SyncError());
}

void ModelTypeController::TriggerCompletionCallbacks(const SyncError& error) {
  DCHECK(CalledOnValidThread());

  if (model_load_callback_) {
    DCHECK(model_stop_callbacks_.empty());
    DCHECK(state_ == MODEL_LOADED || state_ == FAILED);

    model_load_callback_.Run(type(), error);
  } else if (!model_stop_callbacks_.empty()) {
    // State FAILED is possible if an error occurred during STOPPING, either
    // because the load failed or because ReportModelError() was called
    // directly by a subclass.
    DCHECK(state_ == NOT_RUNNING || state_ == FAILED);

    // We make a copy in case running the callbacks has side effects and
    // modifies the vector, although we don't expect that in practice.
    std::vector<StopCallback> model_stop_callbacks =
        std::move(model_stop_callbacks_);
    DCHECK(model_stop_callbacks_.empty());
    for (StopCallback& stop_callback : model_stop_callbacks) {
      std::move(stop_callback).Run();
    }
  }
}

void ModelTypeController::ClearMetadataWhileStopped() {
  DCHECK(state_ == NOT_RUNNING || state_ == FAILED);
  for (auto& [sync_mode, delegate] : delegate_map_) {
    // `delegate` can be null during testing.
    // TODO(crbug.com/1418351): Remove test-only code-path.
    if (delegate) {
      delegate->ClearMetadataWhileStopped();
    }
  }
}

}  // namespace syncer
