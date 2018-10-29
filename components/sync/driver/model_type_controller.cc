// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/signin/core/browser/account_info.h"
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
    std::unique_ptr<ModelTypeControllerDelegate> delegate_on_disk)
    : DataTypeController(type) {
  delegate_map_.emplace(ConfigureContext::STORAGE_ON_DISK,
                        std::move(delegate_on_disk));
}

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_on_disk,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_in_memory)
    : ModelTypeController(type, std::move(delegate_on_disk)) {
  delegate_map_.emplace(ConfigureContext::STORAGE_IN_MEMORY,
                        std::move(delegate_in_memory));
}

ModelTypeController::~ModelTypeController() {}

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

  auto it = delegate_map_.find(configure_context.storage_option);
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

  // Note that |request.authenticated_account_id| may be empty for local sync.
  DCHECK(!request.cache_guid.empty());

  // Ask the delegate to actually start the datatype.
  delegate_->OnSyncStarting(
      request, base::BindOnce(&ModelTypeController::OnProcessorStarted,
                              base::AsWeakPtr(this)));
}

void ModelTypeController::BeforeLoadModels(ModelTypeConfigurer* configurer) {}

void ModelTypeController::LoadModelsDone(ConfigureResult result,
                                         const SyncError& error) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(NOT_RUNNING, state_);

  if (state_ == STOPPING) {
    DCHECK(!model_stop_callbacks_.empty());

    // We make a copy in case running the callbacks has side effects and
    // modifies the vector, although we don't expect that in practice.
    std::vector<StopCallback> model_stop_callbacks =
        std::move(model_stop_callbacks_);
    DCHECK(model_stop_callbacks_.empty());

    if (IsSuccessfulResult(result)) {
      state_ = NOT_RUNNING;
      DVLOG(1) << "Successful sync start completion received late for "
               << ModelTypeToString(type())
               << ", it has been stopped meanwhile";
      delegate_->OnSyncStopping(model_stop_metadata_fate_);
    } else {
      state_ = FAILED;
      DVLOG(1) << "Sync start completion error received late for "
               << ModelTypeToString(type())
               << ", it has been stopped meanwhile";
    }

    delegate_ = nullptr;

    for (StopCallback& stop_callback : model_stop_callbacks) {
      std::move(stop_callback).Run();
    }
    return;
  }

  if (IsSuccessfulResult(result)) {
    DCHECK_EQ(MODEL_STARTING, state_);
    state_ = MODEL_LOADED;
    DVLOG(1) << "Sync start completed for " << ModelTypeToString(type());
  } else {
    state_ = FAILED;
  }

  if (model_load_callback_) {
    model_load_callback_.Run(type(), error);
  }
}

void ModelTypeController::OnProcessorStarted(
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(CalledOnValidThread());
  // Hold on to the activation context until ActivateDataType is called.
  if (state_ == MODEL_STARTING) {
    activation_response_ = std::move(activation_response);
  }
  LoadModelsDone(OK, SyncError());
}

void ModelTypeController::RegisterWithBackend(
    base::OnceCallback<void(bool)> set_downloaded,
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  if (activated_)
    return;
  DCHECK(configurer);
  DCHECK(activation_response_);
  DCHECK_EQ(MODEL_LOADED, state_);
  // Inform the DataTypeManager whether our initial download is complete.
  std::move(set_downloaded)
      .Run(activation_response_->model_type_state.initial_sync_done());
  // Pass activation context to ModelTypeRegistry, where ModelTypeWorker gets
  // created and connected with ModelTypeProcessor.
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_response_));
  activated_ = true;
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
      // Special case: For AUTOFILL_WALLET_DATA, we want to clear all data even
      // when Sync is stopped temporarily.
      // TODO(crbug.com/890361,crbug.com/890737): Move this into the
      // Wallet-specific ModelTypeController once we have one.
      if (type() == AUTOFILL_WALLET_DATA) {
        metadata_fate = CLEAR_METADATA;
      }
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
      model_stop_metadata_fate_ = metadata_fate;
      model_stop_callbacks_.push_back(std::move(callback));
      // The actual stop will be executed in LoadModelsDone(), when the starting
      // process is finished.
      state_ = STOPPING;
      break;

    case MODEL_LOADED:
    case RUNNING:
      DVLOG(1) << "Stopping sync for " << ModelTypeToString(type());
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

  // TODO(jkrcal, mastiz): We should make it more strict and call
  // LoadModelsDone() only if the model is actually starting as treat more cases
  // above with an early return (as NOT_RUNNING).
  LoadModelsDone(UNRECOVERABLE_ERROR, SyncError(error.location(), error_type,
                                                error.message(), type()));
  DCHECK_EQ(state_, FAILED);
}

void ModelTypeController::RecordStartFailure() const {
  DCHECK(CalledOnValidThread());
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  // This is not strongly typed because historically, ModelTypeToHistogramInt()
  // defines quite a different order from the type() enum.
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures2",
                            ModelTypeToHistogramInt(type()),
                            static_cast<int>(MODEL_TYPE_COUNT));
}

void ModelTypeController::RecordRunFailure() const {
  DCHECK(CalledOnValidThread());
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  // This is not strongly typed because historically, ModelTypeToHistogramInt()
  // defines quite a different order from the type() enum.
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures2",
                            ModelTypeToHistogramInt(type()),
                            static_cast<int>(MODEL_TYPE_COUNT));
}

}  // namespace syncer
