// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/sync_merge_result.h"

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

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode)
    : DataTypeController(type) {
  delegate_map_.emplace(SyncMode::kFull,
                        std::move(delegate_for_full_sync_mode));
}

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode)
    : ModelTypeController(type, std::move(delegate_for_full_sync_mode)) {
  delegate_map_.emplace(SyncMode::kTransportOnly,
                        std::move(delegate_for_transport_mode));
}

ModelTypeController::~ModelTypeController() {}

std::unique_ptr<DataTypeActivationResponse>
ModelTypeController::ActivateManuallyForNigori() {
  // To avoid abuse of this temporary API, we restrict it to NIGORI.
  DCHECK_EQ(NIGORI, type());
  DCHECK_EQ(MODEL_LOADED, state_);
  DCHECK(activation_response_);
  state_ = RUNNING;
  activated_ = true;  // Not relevant, but for consistency.
  return std::move(activation_response_);
}

bool ModelTypeController::ShouldLoadModelBeforeConfigure() const {
  // USS datatypes require loading models because model controls storage where
  // data type context and progress marker are persisted.
  return true;
}

void ModelTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(model_load_callback);
  DCHECK_EQ(NOT_RUNNING, state_);

  auto it = delegate_map_.find(configure_context.sync_mode);
  DCHECK(it != delegate_map_.end());
  delegate_ = it->second.get();
  DCHECK(delegate_);

  DVLOG(1) << "Sync starting for " << ModelTypeToString(type());
  state_ = MODEL_STARTING;
  model_load_callback_ = model_load_callback;

  DataTypeActivationRequest request;
  request.error_handler = base::BindRepeating(
      &ReportErrorOnModelThread, base::SequencedTaskRunnerHandle::Get(),
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

void ModelTypeController::BeforeLoadModels(ModelTypeConfigurer* configurer) {}

DataTypeController::RegisterWithBackendResult
ModelTypeController::RegisterWithBackend(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  if (activated_)
    return REGISTRATION_IGNORED;
  DCHECK(configurer);
  DCHECK(activation_response_);
  DCHECK_EQ(MODEL_LOADED, state_);
  bool initial_sync_done =
      activation_response_->model_type_state.initial_sync_done();
  // Pass activation context to ModelTypeRegistry, where ModelTypeWorker gets
  // created and connected with the delegate (processor).
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_response_));
  activated_ = true;
  return initial_sync_done ? TYPE_ALREADY_DOWNLOADED : TYPE_NOT_YET_DOWNLOADED;
}

void ModelTypeController::StartAssociating(StartCallback start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(start_callback);
  DCHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;
  DVLOG(1) << "Sync running for " << ModelTypeToString(type());

  // There is no association, just call back promptly.
  SyncMergeResult merge_result(type());
  std::move(start_callback).Run(OK, merge_result, merge_result);
}

void ModelTypeController::ActivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  DCHECK_EQ(RUNNING, state_);
  // In contrast with directory datatypes, non-blocking data types should be
  // activated in RegisterWithBackend. activation_response_ should be
  // passed to backend before call to ActivateDataType.
  DCHECK(!activation_response_);
}

void ModelTypeController::DeactivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  if (activated_) {
    configurer->DeactivateNonBlockingDataType(type());
    activated_ = false;
  }
}

void ModelTypeController::Stop(ShutdownReason shutdown_reason,
                               StopCallback callback) {
  DCHECK(CalledOnValidThread());

  // Leave metadata if we do not disable sync completely.
  SyncStopMetadataFate metadata_fate = KEEP_METADATA;
  switch (shutdown_reason) {
    case STOP_SYNC:
      break;
    case DISABLE_SYNC:
      metadata_fate = CLEAR_METADATA;
      break;
    case BROWSER_SHUTDOWN:
      break;
  }

  switch (state()) {
    case ASSOCIATING:
      // We don't really use this state in this class.
      NOTREACHED();
      break;

    case NOT_RUNNING:
    case FAILED:
      // Nothing to stop. |metadata_fate| might require CLEAR_METADATA,
      // which could lead to leaking sync metadata, but it doesn't seem a
      // realistic scenario (disable sync during shutdown?).
      std::move(callback).Run();
      return;

    case STOPPING:
      DCHECK(!model_stop_callbacks_.empty());
      model_stop_metadata_fate_ =
          TakeStrictestMetadataFate(model_stop_metadata_fate_, metadata_fate);
      model_stop_callbacks_.push_back(std::move(callback));
      break;

    case MODEL_STARTING:
      DCHECK(model_load_callback_);
      DCHECK(model_stop_callbacks_.empty());
      DLOG(WARNING) << "Deferring stop for " << ModelTypeToString(type())
                    << " because it's still starting";
      model_load_callback_.Reset();
      model_stop_metadata_fate_ = metadata_fate;
      model_stop_callbacks_.push_back(std::move(callback));
      // The actual stop will be executed when the starting process is finished.
      state_ = STOPPING;
      break;

    case MODEL_LOADED:
    case RUNNING:
      DVLOG(1) << "Stopping sync for " << ModelTypeToString(type());
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

void ModelTypeController::GetAllNodes(AllNodesCallback callback) {
  DCHECK(delegate_);
  delegate_->GetAllNodesForDebugging(std::move(callback));
}

void ModelTypeController::GetStatusCounters(StatusCountersCallback callback) {
  DCHECK(delegate_);
  delegate_->GetStatusCountersForDebugging(std::move(callback));
}

void ModelTypeController::RecordMemoryUsageAndCountsHistograms() {
  DCHECK(delegate_);
  delegate_->RecordMemoryUsageAndCountsHistograms();
}

void ModelTypeController::ReportModelError(SyncError::ErrorType error_type,
                                           const ModelError& error) {
  DCHECK(CalledOnValidThread());

  // TODO(crbug.com/890729): This is obviously misplaced/misnamed as we report
  // run-time failures as well. Rename the histogram to ConfigureResult and
  // report it only after startup (also for success).
  if (state_ != NOT_RUNNING) {
#define PER_DATA_TYPE_MACRO(type_str)                            \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", \
                            UNRECOVERABLE_ERROR, MAX_CONFIGURE_RESULT);
    SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
  }

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
    case ASSOCIATING:
      // Not possible, we do not use associating in this class.
      NOTREACHED();
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
      FALLTHROUGH;
    case FAILED:
      DVLOG(1) << "Successful sync start completion received late for "
               << ModelTypeToString(type())
               << ", it has been stopped meanwhile";
      delegate_->OnSyncStopping(model_stop_metadata_fate_);
      delegate_ = nullptr;
      break;
    case MODEL_STARTING:
      DCHECK(model_stop_callbacks_.empty());
      // Hold on to the activation context until ActivateDataType is called.
      activation_response_ = std::move(activation_response);
      state_ = MODEL_LOADED;
      DVLOG(1) << "Sync start completed for " << ModelTypeToString(type());
      break;
    case MODEL_LOADED:
    case RUNNING:
    case NOT_RUNNING:
    case ASSOCIATING:
      NOTREACHED() << " type " << ModelTypeToString(type()) << " state "
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

}  // namespace syncer
