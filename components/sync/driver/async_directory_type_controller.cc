// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/async_directory_type_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/generic_change_processor_factory.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model/syncable_service.h"

namespace syncer {

SharedChangeProcessor*
AsyncDirectoryTypeController::CreateSharedChangeProcessor() {
  return new SharedChangeProcessor(type());
}

AsyncDirectoryTypeController::AsyncDirectoryTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncClient* sync_client,
    ModelSafeGroup model_safe_group,
    scoped_refptr<base::SequencedTaskRunner> model_thread)
    : DirectoryDataTypeController(type,
                                  dump_stack,
                                  sync_client,
                                  model_safe_group),
      user_share_(nullptr),
      processor_factory_(new GenericChangeProcessorFactory()),
      state_(NOT_RUNNING),
      model_thread_(std::move(model_thread)) {}

void AsyncDirectoryTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(configure_context.storage_option, ConfigureContext::STORAGE_ON_DISK)
      << " for type " << ModelTypeToString(type());

  model_load_callback_ = model_load_callback;

  if (state() != NOT_RUNNING) {
    model_load_callback_.Run(type(),
                             SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                       "Model already running", type()));
    return;
  }

  state_ = MODEL_STARTING;
  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be null here.
  DCHECK(!shared_change_processor_);
  shared_change_processor_ = CreateSharedChangeProcessor();
  DCHECK(shared_change_processor_);
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early.
    DCHECK(state() == MODEL_STARTING || state() == NOT_RUNNING);
    return;
  }

  OnModelLoaded();
}

void AsyncDirectoryTypeController::OnModelLoaded() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), SyncError());
}

bool AsyncDirectoryTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void AsyncDirectoryTypeController::StopModels() {
  DCHECK(CalledOnValidThread());
}

bool AsyncDirectoryTypeController::PostTaskOnModelThread(
    const base::Location& from_here,
    const base::Closure& task) {
  DCHECK(CalledOnValidThread());
  return model_thread_->PostTask(from_here, task);
}

void AsyncDirectoryTypeController::StartAssociating(
    StartCallback start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);
  state_ = ASSOCIATING;

  // Store UserShare now while on UI thread to avoid potential race
  // condition in StartAssociationWithSharedChangeProcessor.
  DCHECK(sync_client_->GetSyncService());
  user_share_ = sync_client_->GetSyncService()->GetUserShare();

  start_callback_ = std::move(start_callback);
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                    "Failed to post StartAssociation", type());
    SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED, local_merge_result, SyncMergeResult(type()));
    // StartDone should have cleared the SharedChangeProcessor.
    DCHECK(!shared_change_processor_);
    return;
  }
}

void AsyncDirectoryTypeController::Stop(ShutdownReason shutdown_reason) {
  DCHECK(CalledOnValidThread());

  if (state() == NOT_RUNNING)
    return;

  // Disconnect the change processor. At this point, the
  // SyncableService can no longer interact with the Syncer, even if
  // it hasn't finished MergeDataAndStartSyncing.
  DisconnectSharedChangeProcessor();

  // If we haven't finished starting, we need to abort the start.
  bool service_started = state() == ASSOCIATING || state() == RUNNING;
  state_ = service_started ? STOPPING : NOT_RUNNING;
  StopModels();

  if (service_started)
    StopSyncableService();

  shared_change_processor_ = nullptr;
  state_ = NOT_RUNNING;
}

DataTypeController::State AsyncDirectoryTypeController::state() const {
  return state_;
}

void AsyncDirectoryTypeController::SetGenericChangeProcessorFactoryForTest(
    std::unique_ptr<GenericChangeProcessorFactory> factory) {
  DCHECK_EQ(state_, NOT_RUNNING);
  processor_factory_ = std::move(factory);
}

AsyncDirectoryTypeController::AsyncDirectoryTypeController()
    : DirectoryDataTypeController(UNSPECIFIED,
                                  base::Closure(),
                                  nullptr,
                                  GROUP_PASSIVE) {}

AsyncDirectoryTypeController::~AsyncDirectoryTypeController() {}

void AsyncDirectoryTypeController::StartDone(
    DataTypeController::ConfigureResult start_result,
    const SyncMergeResult& local_merge_result,
    const SyncMergeResult& syncer_merge_result) {
  DCHECK(CalledOnValidThread());

  DataTypeController::State new_state;
  if (IsSuccessfulResult(start_result)) {
    new_state = RUNNING;
  } else {
    new_state = (start_result == ASSOCIATION_FAILED ? FAILED : NOT_RUNNING);
  }

  // If we failed to start up, and we haven't been stopped yet, we need to
  // ensure we clean up the local service and shared change processor properly.
  if (new_state != RUNNING && state() != NOT_RUNNING && state() != STOPPING) {
    DisconnectSharedChangeProcessor();
    StopSyncableService();
    shared_change_processor_ = nullptr;
  }

  // It's possible to have StartDone called first from the UI thread
  // (due to Stop being called) and then posted from the non-UI thread. In
  // this case, we drop the second call because we've already been stopped.
  if (state_ == NOT_RUNNING) {
    return;
  }

  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    StopModels();
    RecordStartFailure(start_result);
  }

  std::move(start_callback_)
      .Run(start_result, local_merge_result, syncer_merge_result);
}

void AsyncDirectoryTypeController::RecordStartFailure(ConfigureResult result) {
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

void AsyncDirectoryTypeController::DisableImpl(const SyncError& error) {
  DCHECK(CalledOnValidThread());
  if (model_load_callback_) {
    model_load_callback_.Run(type(), error);
  }
}

bool AsyncDirectoryTypeController::StartAssociationAsync() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), ASSOCIATING);
  return PostTaskOnModelThread(
      FROM_HERE,
      base::Bind(
          &SharedChangeProcessor::StartAssociation, shared_change_processor_,
          BindToCurrentSequence(base::Bind(
              &AsyncDirectoryTypeController::StartDone, base::AsWeakPtr(this))),
          sync_client_, processor_factory_.get(), user_share_,
          base::Passed(CreateErrorHandler())));
}

ChangeProcessor* AsyncDirectoryTypeController::GetChangeProcessor() const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, RUNNING);
  return shared_change_processor_->generic_change_processor();
}

void AsyncDirectoryTypeController::DisconnectSharedChangeProcessor() {
  DCHECK(CalledOnValidThread());
  // |shared_change_processor_| can already be null if Stop() is
  // called after StartDone(_, FAILED, _).
  if (shared_change_processor_) {
    shared_change_processor_->Disconnect();
  }
}

void AsyncDirectoryTypeController::StopSyncableService() {
  DCHECK(CalledOnValidThread());
  if (shared_change_processor_) {
    PostTaskOnModelThread(FROM_HERE,
                          base::Bind(&SharedChangeProcessor::StopLocalService,
                                     shared_change_processor_));
  }
}

std::unique_ptr<DataTypeErrorHandler>
AsyncDirectoryTypeController::CreateErrorHandler() {
  DCHECK(CalledOnValidThread());
  return std::make_unique<DataTypeErrorHandlerImpl>(
      base::SequencedTaskRunnerHandle::Get(), dump_stack_,
      base::Bind(&AsyncDirectoryTypeController::DisableImpl,
                 base::AsWeakPtr(this)));
}

}  // namespace syncer
