// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/configure_context.h"

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
  NOTREACHED_IN_MIGRATION();
  return KEEP_METADATA;
}

}  // namespace

// static
std::string DataTypeController::StateToString(State state) {
  switch (state) {
    case NOT_RUNNING:
      return "Not Running";
    case MODEL_STARTING:
      return "Model Starting";
    case MODEL_LOADED:
      return "Model Loaded";
    case RUNNING:
      return "Running";
    case STOPPING:
      return "Stopping";
    case FAILED:
      return "Failed";
  }
  NOTREACHED_IN_MIGRATION();
  return "Invalid";
}

DataTypeController::DataTypeController(
    DataType type,
    std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader)
    : type_(type), batch_uploader_(std::move(batch_uploader)) {}

DataTypeController::DataTypeController(
    DataType type,
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode,
    std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader)
    : DataTypeController(type, std::move(batch_uploader)) {
  InitDataTypeController(std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode));
}

DataTypeController::~DataTypeController() = default;

void DataTypeController::InitDataTypeController(
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode) {
  DCHECK(delegate_map_.empty());
  delegate_map_.emplace(SyncMode::kFull,
                        std::move(delegate_for_full_sync_mode));
  if (delegate_for_transport_mode) {
    delegate_map_.emplace(SyncMode::kTransportOnly,
                          std::move(delegate_for_transport_mode));
  } else {
    // New Sync data types must generally support kTransportOnly mode. Only
    // legacy types are still allowed to *not* support it for historical reasons
    // (they will all be either migrated or retired eventually).
    // There are two general ways to go about this:
    // * Single storage: The feature is available to all signed-in users (no
    //   distinction between syncing or non-syncing users), and doesn't support
    //   signed-out users at all - so only a single storage is required.
    //   Examples: SEND_TAB_TO_SELF, SHARING_MESSAGE.
    // * Dual storage: The feature is also available to signed-out users (and
    //   its behavior may or may not differ for syncing vs signed-in-not-syncing
    //   users). Two storages are required to distinguish "local data" from
    //   "account data" and keep them separate. Examples: PASSWORDS, BOOKMARKS.

    // Notes on individual data types:
    // * NIGORI *does* actually support transport-mode, but avoids a separate
    //   delegate, see SyncEngineBackend::LoadAndConnectNigoriController().
    // * BOOKMARKS and READING_LIST: Support is WIP.
    // * PASSWORDS: Already supported on desktop; mobile is WIP.
    // * PREFERENCES in all variants: Support is WIP.
    // * History-related types (HISTORY, HISTORY_DELETE_DIRECTIVES, SESSIONS)
    //   are okay to *not* support transport mode.
    // * APPS/APP_SETTINGS: Deprecated and will eventually be removed.
    // * AUTOFILL/AUTOFILL_PROFILE: Semi-deprecated; will eventually be removed
    //   or replaced by CONTACT_INFO.
    //
    // Note on ChromeOS-Ash: On this platform, the sync machinery always runs in
    // full-sync mode, never transport-mode. So for data types that only exist
    // on this platform, it doesn't matter if they support transport mode or not
    // (this includes PRINTERS, WIFI_CONFIGURATIONS, OS_PREFERENCES,
    // OS_PRIORITY_PREFERENCES, WORKSPACE_DESK, PRINTERS_AUTHORIZATION_SERVERS,
    // COOKIES).
    //
    // All other data types listed here will likely have to be migrated.
    static constexpr DataTypeSet kLegacyTypes = {BOOKMARKS,
                                                 PREFERENCES,
                                                 PASSWORDS,
                                                 AUTOFILL_PROFILE,
                                                 AUTOFILL,
                                                 AUTOFILL_WALLET_METADATA,
                                                 AUTOFILL_WALLET_OFFER,
                                                 THEMES,
                                                 EXTENSIONS,
                                                 SEARCH_ENGINES,
                                                 SESSIONS,
                                                 APPS,
                                                 APP_SETTINGS,
                                                 EXTENSION_SETTINGS,
                                                 HISTORY_DELETE_DIRECTIVES,
                                                 DICTIONARY,
                                                 PRIORITY_PREFERENCES,
                                                 PRINTERS,
                                                 READING_LIST,
                                                 USER_EVENTS,
                                                 WIFI_CONFIGURATIONS,
                                                 WEB_APPS,
                                                 OS_PREFERENCES,
                                                 OS_PRIORITY_PREFERENCES,
                                                 WORKSPACE_DESK,
                                                 HISTORY,
                                                 PRINTERS_AUTHORIZATION_SERVERS,
                                                 POWER_BOOKMARK,
                                                 NIGORI,
                                                 COOKIES};
    CHECK(kLegacyTypes.Has(type()))
        << DataTypeToDebugString(type())
        << " must support running in transport mode!";
  }
}

void DataTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(model_load_callback);
  CHECK_EQ(NOT_RUNNING, state_);

  auto it = delegate_map_.find(configure_context.sync_mode);
  CHECK(it != delegate_map_.end()) << DataTypeToDebugString(type());
  delegate_ = it->second.get();
  CHECK(delegate_);

  DVLOG(1) << "Sync starting for " << DataTypeToDebugString(type());
  state_ = MODEL_STARTING;
  model_load_callback_ = model_load_callback;

  DataTypeActivationRequest request;
  request.error_handler = base::BindRepeating(
      &ReportErrorOnModelThread, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&DataTypeController::ReportModelError,
                          weak_ptr_factory_.GetWeakPtr()));
  request.authenticated_account_id = configure_context.authenticated_account_id;
  request.cache_guid = configure_context.cache_guid;
  request.sync_mode = configure_context.sync_mode;
  request.configuration_start_time = configure_context.configuration_start_time;

  // Note that |request.authenticated_account_id| may be empty for local sync.
  DCHECK(!request.cache_guid.empty());

  // Ask the delegate to actually start the datatype.
  delegate_->OnSyncStarting(
      request, base::BindOnce(&DataTypeController::OnDelegateStarted,
                              weak_ptr_factory_.GetWeakPtr()));

  // Ensure that the metadata for any *other* delegate is cleared. Note that
  // this is a no-op for the delegate that was just started. Note^2 that that
  // also covers the case of data types using the same underlying delegate for
  // both modes.
  ClearMetadataIfStopped();
}

std::unique_ptr<DataTypeActivationResponse> DataTypeController::Connect() {
  DCHECK(CalledOnValidThread());
  DCHECK(activation_response_);
  CHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;
  DVLOG(1) << "Sync running for " << DataTypeToDebugString(type());

  return std::move(activation_response_);
}

void DataTypeController::Stop(SyncStopMetadataFate fate,
                              StopCallback callback) {
  DCHECK(CalledOnValidThread());
  CHECK(delegate_ || state() == NOT_RUNNING || state() == FAILED);

  switch (state()) {
    case NOT_RUNNING:
      // Clear metadata if needed.
      if (fate == CLEAR_METADATA) {
        ClearMetadataIfStopped();
      }
      // Nothing to stop.
      std::move(callback).Run();
      return;

    case FAILED:
      // Nothing to stop.
      std::move(callback).Run();
      return;

    case STOPPING:
      DCHECK(!model_stop_callbacks_.empty());
      model_stop_metadata_fate_ =
          TakeStrictestMetadataFate(model_stop_metadata_fate_, fate);
      model_stop_callbacks_.push_back(std::move(callback));
      // This just means stopping was requested while starting the data type.
      // Metadata will cleared (if CLEAR_METADATA) in OnSyncStopping.
      break;

    case MODEL_STARTING:
      DCHECK(model_load_callback_);
      DCHECK(model_stop_callbacks_.empty());
      DLOG(WARNING) << "Deferring stop for " << DataTypeToDebugString(type())
                    << " because it's still starting";
      model_load_callback_.Reset();
      model_stop_metadata_fate_ = fate;
      model_stop_callbacks_.push_back(std::move(callback));
      // The actual stop will be executed when the starting process is finished.
      state_ = STOPPING;
      break;

    case MODEL_LOADED:
    case RUNNING:
      DVLOG(1) << "Stopping sync for " << DataTypeToDebugString(type());
      model_load_callback_.Reset();
      state_ = NOT_RUNNING;
      delegate_->OnSyncStopping(fate);
      delegate_ = nullptr;
      std::move(callback).Run();
      break;
  }
}

DataTypeController::State DataTypeController::state() const {
  return state_;
}

DataTypeController::PreconditionState DataTypeController::GetPreconditionState()
    const {
  return PreconditionState::kPreconditionsMet;
}

bool DataTypeController::ShouldRunInTransportOnlyMode() const {
  // By default, running in transport-only mode is enabled if the corresponding
  // delegate exists, i.e. the controller is aware of transport-only mode and
  // supports it in principle. Subclass can still override this with more
  // specific logic.
  return delegate_map_.count(SyncMode::kTransportOnly) != 0;
}

void DataTypeController::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  if (!delegate_) {
    std::move(callback).Run(false);
    return;
  }
  delegate_->HasUnsyncedData(std::move(callback));
}

void DataTypeController::GetAllNodes(AllNodesCallback callback) {
  CHECK(delegate_);
  delegate_->GetAllNodesForDebugging(std::move(callback));
}

void DataTypeController::GetTypeEntitiesCount(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  if (delegate_) {
    delegate_->GetTypeEntitiesCountForDebugging(std::move(callback));
  } else {
    std::move(callback).Run(TypeEntitiesCount(type()));
  }
}

void DataTypeController::RecordMemoryUsageAndCountsHistograms() {
  DCHECK(CalledOnValidThread());
  if (delegate_) {
    delegate_->RecordMemoryUsageAndCountsHistograms();
  }
}

DataTypeLocalDataBatchUploader*
DataTypeController::GetLocalDataBatchUploader() {
  return batch_uploader_.get();
}

void DataTypeController::ReportBridgeErrorForTest() {
  DCHECK(CalledOnValidThread());

  // Tests are supposed to call this method when `delegate_` exists.
  CHECK(delegate_);
  delegate_->ReportBridgeErrorForTest();  // IN-TEST
}

DataTypeControllerDelegate* DataTypeController::GetDelegateForTesting(
    SyncMode sync_mode) {
  auto it = delegate_map_.find(sync_mode);
  return it != delegate_map_.end() ? it->second.get() : nullptr;
}

void DataTypeController::ReportModelError(const ModelError& error) {
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

  TriggerCompletionCallbacks(error);
}

bool DataTypeController::CalledOnValidThread() const {
  return sequence_checker_.CalledOnValidSequence();
}

void DataTypeController::ClearDelegateMap() {
  delegate_ = nullptr;
  delegate_map_.clear();
}

void DataTypeController::RecordStartFailure() const {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures2",
                            DataTypeHistogramValue(type()));
}

void DataTypeController::RecordRunFailure() const {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures2",
                            DataTypeHistogramValue(type()));
}

void DataTypeController::OnDelegateStarted(
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
               << DataTypeToDebugString(type())
               << ", it has been stopped meanwhile";
      delegate_->OnSyncStopping(model_stop_metadata_fate_);
      delegate_ = nullptr;
      break;
    case MODEL_STARTING:
      DCHECK(model_stop_callbacks_.empty());
      // Hold on to the activation context until Connect is called.
      activation_response_ = std::move(activation_response);
      state_ = MODEL_LOADED;
      DVLOG(1) << "Sync start completed for " << DataTypeToDebugString(type());
      break;
    case MODEL_LOADED:
    case RUNNING:
    case NOT_RUNNING:
      NOTREACHED_IN_MIGRATION() << " type " << DataTypeToDebugString(type())
                                << " state " << StateToString(state_);
  }

  TriggerCompletionCallbacks(/*error=*/std::nullopt);
}

void DataTypeController::TriggerCompletionCallbacks(
    const std::optional<ModelError>& error) {
  DCHECK(CalledOnValidThread());

  if (model_load_callback_) {
    DCHECK(model_stop_callbacks_.empty());
    CHECK(state_ == MODEL_LOADED || state_ == FAILED);

    model_load_callback_.Run(error);
  } else if (!model_stop_callbacks_.empty()) {
    // State FAILED is possible if an error occurred during STOPPING, either
    // because the load failed or because ReportModelError() was called
    // directly by a subclass.
    CHECK(state_ == NOT_RUNNING || state_ == FAILED);

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

void DataTypeController::ClearMetadataIfStopped() {
  for (auto& [sync_mode, delegate] : delegate_map_) {
    // `delegate` can be null during testing.
    // TODO(crbug.com/40894683): Remove test-only code-path.
    if (delegate) {
      delegate->ClearMetadataIfStopped();
    }
  }
}

}  // namespace syncer
