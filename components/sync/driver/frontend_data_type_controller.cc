// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/frontend_data_type_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/model_associator.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

FrontendDataTypeController::FrontendDataTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncClient* sync_client)
    : DirectoryDataTypeController(type, dump_stack, sync_client, GROUP_UI),
      state_(NOT_RUNNING) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_client);
}

void FrontendDataTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(configure_context.storage_option,
            ConfigureContext::STORAGE_ON_DISK);

  model_load_callback_ = model_load_callback;

  if (state_ != NOT_RUNNING) {
    model_load_callback_.Run(type(),
                             SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                       "Model already running", type()));
    return;
  }

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  OnModelLoaded();
}

void FrontendDataTypeController::OnModelLoaded() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);

  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), SyncError());
}

void FrontendDataTypeController::StartAssociating(
    StartCallback start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(start_callback);
  DCHECK_EQ(state_, MODEL_LOADED);

  start_callback_ = std::move(start_callback);
  state_ = ASSOCIATING;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FrontendDataTypeController::Associate,
                                base::AsWeakPtr(this)));
}

void FrontendDataTypeController::Stop(ShutdownReason shutdown_reason) {
  DCHECK(CalledOnValidThread());

  if (state_ == NOT_RUNNING)
    return;

  State prev_state = state_;
  state_ = STOPPING;

  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (prev_state == MODEL_STARTING) {
    AbortModelLoad();
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }

  CleanUpState();

  if (model_associator()) {
    SyncError error;  // Not used.
    error = model_associator()->DisassociateModels();
  }

  set_model_associator(nullptr);
  change_processor_.reset();

  state_ = NOT_RUNNING;
}

DataTypeController::State FrontendDataTypeController::state() const {
  return state_;
}

FrontendDataTypeController::FrontendDataTypeController()
    : DirectoryDataTypeController(UNSPECIFIED,
                                  base::Closure(),
                                  nullptr,
                                  GROUP_UI),
      state_(NOT_RUNNING) {}

FrontendDataTypeController::~FrontendDataTypeController() {}

bool FrontendDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void FrontendDataTypeController::Associate() {
  DCHECK(CalledOnValidThread());
  if (state_ != ASSOCIATING) {
    // Stop() must have been called while Associate() task have been waiting.
    DCHECK_EQ(state_, NOT_RUNNING);
    return;
  }

  SyncMergeResult local_merge_result(type());
  SyncMergeResult syncer_merge_result(type());
  CreateSyncComponents();
  if (!model_associator()->CryptoReadyIfNecessary()) {
    StartDone(NEEDS_CRYPTO, local_merge_result, syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator()->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    SyncError error(FROM_HERE, SyncError::UNRECOVERABLE_ERROR,
                    "Failed to load sync nodes", type());
    local_merge_result.set_error(error);
    StartDone(UNRECOVERABLE_ERROR, local_merge_result, syncer_merge_result);
    return;
  }

  // TODO(zea): Have AssociateModels fill the local and syncer merge results.
  base::TimeTicks start_time = base::TimeTicks::Now();
  SyncError error;
  error = model_associator()->AssociateModels(&local_merge_result,
                                              &syncer_merge_result);
  // TODO(lipalani): crbug.com/122690 - handle abort.
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED, local_merge_result, syncer_merge_result);
    return;
  }

  state_ = RUNNING;
  // FinishStart() invokes the DataTypeManager callback, which can lead to a
  // call to Stop() if one of the other data types being started generates an
  // error.
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, local_merge_result,
            syncer_merge_result);
}

void FrontendDataTypeController::CleanUpState() {
  // Do nothing by default.
}

void FrontendDataTypeController::CleanUp() {
  CleanUpState();
  set_model_associator(nullptr);
  change_processor_.reset();
}

void FrontendDataTypeController::AbortModelLoad() {
  DCHECK(CalledOnValidThread());
  CleanUp();
  state_ = NOT_RUNNING;
}

void FrontendDataTypeController::StartDone(
    ConfigureResult start_result,
    const SyncMergeResult& local_merge_result,
    const SyncMergeResult& syncer_merge_result) {
  DCHECK(CalledOnValidThread());
  if (!IsSuccessfulResult(start_result)) {
    CleanUp();
    if (start_result == ASSOCIATION_FAILED) {
      state_ = FAILED;
    } else {
      state_ = NOT_RUNNING;
    }
    RecordStartFailure(start_result);
  }

  std::move(start_callback_)
      .Run(start_result, local_merge_result, syncer_merge_result);
}

std::unique_ptr<DataTypeErrorHandler>
FrontendDataTypeController::CreateErrorHandler() {
  return std::make_unique<DataTypeErrorHandlerImpl>(
      base::SequencedTaskRunnerHandle::Get(), dump_stack_,
      base::Bind(&FrontendDataTypeController::OnUnrecoverableError,
                 base::AsWeakPtr(this)));
}

void FrontendDataTypeController::OnUnrecoverableError(const SyncError& error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type(), error.model_type());
  if (model_load_callback_) {
    model_load_callback_.Run(type(), error);
  }
}

void FrontendDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(CalledOnValidThread());
#define PER_DATA_TYPE_MACRO(type_str) \
  UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void FrontendDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK(CalledOnValidThread());
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures2",
                            ModelTypeToHistogramInt(type()),
                            static_cast<int>(MODEL_TYPE_COUNT));
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

AssociatorInterface* FrontendDataTypeController::model_associator() const {
  return model_associator_.get();
}

void FrontendDataTypeController::set_model_associator(
    std::unique_ptr<AssociatorInterface> model_associator) {
  model_associator_ = std::move(model_associator);
}

ChangeProcessor* FrontendDataTypeController::GetChangeProcessor() const {
  return change_processor_.get();
}

void FrontendDataTypeController::set_change_processor(
    std::unique_ptr<ChangeProcessor> change_processor) {
  change_processor_ = std::move(change_processor);
}

}  // namespace syncer
