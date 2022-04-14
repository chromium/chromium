// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_debug_info_listener.h"

namespace syncer {

namespace {

DataTypeStatusTable::TypeErrorMap GenerateCryptoErrorsForTypes(
    ModelTypeSet encrypted_types) {
  DataTypeStatusTable::TypeErrorMap crypto_errors;
  for (ModelType type : encrypted_types) {
    crypto_errors[type] =
        SyncError(FROM_HERE, SyncError::CRYPTO_ERROR, "", type);
  }
  return crypto_errors;
}

ConfigureReason GetReasonForProgrammaticReconfigure(
    ConfigureReason original_reason) {
  // This reconfiguration can happen within the first configure cycle and in
  // this case we want to stick to the original reason -- doing the first sync
  // cycle.
  return (original_reason == ConfigureReason::CONFIGURE_REASON_NEW_CLIENT)
             ? ConfigureReason::CONFIGURE_REASON_NEW_CLIENT
             : ConfigureReason::CONFIGURE_REASON_PROGRAMMATIC;
}

// Divides |types| into sets by their priorities and return the sets from
// high priority to low priority.
base::queue<ModelTypeSet> PrioritizeTypes(const ModelTypeSet& types) {
  // Control types are usually configured before all other types during
  // initialization of sync engine even before data type manager gets
  // constructed. However, listing control types here with the highest priority
  // makes the behavior consistent also for various flows for restarting sync
  // such as migrating all data types or reconfiguring sync in ephemeral mode
  // when all local data is wiped.
  ModelTypeSet control_types = ControlTypes();
  control_types.RetainAll(types);

  ModelTypeSet priority_types = PriorityUserTypes();
  priority_types.RetainAll(types);

  ModelTypeSet regular_types =
      Difference(types, Union(control_types, priority_types));

  base::queue<ModelTypeSet> result;
  if (!control_types.Empty())
    result.push(control_types);
  if (!priority_types.Empty())
    result.push(priority_types);
  if (!regular_types.Empty())
    result.push(regular_types);

  // Could be empty in case of purging for migration, sync nothing, etc.
  // Configure empty set to purge data from backend.
  if (result.empty())
    result.push(ModelTypeSet());

  return result;
}

}  // namespace

DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo() = default;
DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo(
    const AssociationTypesInfo& other) = default;
DataTypeManagerImpl::AssociationTypesInfo::~AssociationTypesInfo() = default;

DataTypeManagerImpl::DataTypeManagerImpl(
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const DataTypeEncryptionHandler* encryption_handler,
    ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer)
    : configurer_(configurer),
      controllers_(controllers),
      debug_info_listener_(debug_info_listener),
      model_load_manager_(controllers, this),
      observer_(observer),
      encryption_handler_(encryption_handler) {
  DCHECK(configurer_);
  DCHECK(observer_);

  // This class does not really handle NIGORI (whose controller lives on a
  // different thread).
  DCHECK_EQ(controllers_->count(NIGORI), 0u);

  // Check if any of the controllers are already in a FAILED state, and if so,
  // mark them accordingly in the status table.
  DataTypeStatusTable::TypeErrorMap existing_errors;
  for (const auto& [type, controller] : *controllers_) {
    DataTypeController::State state = controller->state();
    DCHECK(state == DataTypeController::NOT_RUNNING ||
           state == DataTypeController::STOPPING ||
           state == DataTypeController::FAILED);
    if (state == DataTypeController::FAILED) {
      existing_errors[type] =
          SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                    "Preexisting controller error on Sync startup", type);
    }
  }
  data_type_status_table_.UpdateFailedDataTypes(existing_errors);
}

DataTypeManagerImpl::~DataTypeManagerImpl() = default;

void DataTypeManagerImpl::Configure(ModelTypeSet desired_types,
                                    const ConfigureContext& context) {
  desired_types.PutAll(ControlTypes());

  ModelTypeSet allowed_types = ControlTypes();
  // Add types with controllers.
  for (const auto& [type, controller] : *controllers_) {
    allowed_types.Put(type);
  }

  ConfigureImpl(Intersection(desired_types, allowed_types), context);
}

void DataTypeManagerImpl::DataTypePreconditionChanged(ModelType type) {
  if (!UpdatePreconditionError(type)) {
    // Nothing changed.
    return;
  }

  switch (controllers_->find(type)->second->GetPreconditionState()) {
    case DataTypeController::PreconditionState::kPreconditionsMet:
      if (preferred_types_.Has(type)) {
        // Only reconfigure if the type is both ready and desired. This will
        // internally also update ready state of all other requested types.
        ForceReconfiguration();
      }
      break;

    case DataTypeController::PreconditionState::kMustStopAndClearData:
      model_load_manager_.StopDatatype(
          type, ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
          SyncError(FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                    "Datatype preconditions not met.", type));
      break;

    case DataTypeController::PreconditionState::kMustStopAndKeepData:
      model_load_manager_.StopDatatype(
          type, ShutdownReason::STOP_SYNC_AND_KEEP_DATA,
          SyncError(FROM_HERE, syncer::SyncError::UNREADY_ERROR,
                    "Data type is unready.", type));
      break;
  }
}

void DataTypeManagerImpl::ForceReconfiguration() {
  needs_reconfigure_ = true;
  last_requested_context_.reason =
      GetReasonForProgrammaticReconfigure(last_requested_context_.reason);
  ProcessReconfigure();
}

void DataTypeManagerImpl::ResetDataTypeErrors() {
  data_type_status_table_.Reset();
}

void DataTypeManagerImpl::PurgeForMigration(ModelTypeSet undesired_types) {
  ModelTypeSet remainder = Difference(preferred_types_, undesired_types);
  last_requested_context_.reason = CONFIGURE_REASON_MIGRATION;
  ConfigureImpl(remainder, last_requested_context_);
}

void DataTypeManagerImpl::ConfigureImpl(ModelTypeSet desired_types,
                                        const ConfigureContext& context) {
  DCHECK_NE(context.reason, CONFIGURE_REASON_UNKNOWN);
  DVLOG(1) << "Configuring for " << ModelTypeSetToDebugString(desired_types)
           << " with reason " << context.reason;
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  if (state_ != STOPPED) {
    DCHECK_EQ(context.authenticated_account_id,
              last_requested_context_.authenticated_account_id);
    DCHECK_EQ(context.cache_guid, last_requested_context_.cache_guid);
  }

  // TODO(zea): consider not performing a full configuration once there's a
  // reliable way to determine if the requested set of enabled types matches the
  // current set.

  preferred_types_ = desired_types;
  last_requested_context_ = context;

  // Only proceed if we're in a steady state or retrying.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != RETRYING) {
    DVLOG(1) << "Received configure request while configuration in flight. "
             << "Postponing until current configuration complete.";
    needs_reconfigure_ = true;
    return;
  }

  Restart();
}

void DataTypeManagerImpl::ConnectDataTypes() {
  for (ModelType type : last_enabled_types_) {
    auto dtc_iter = controllers_->find(type);
    if (dtc_iter == controllers_->end()) {
      continue;
    }
    DataTypeController* dtc = dtc_iter->second.get();
    if (dtc->state() != DataTypeController::MODEL_LOADED) {
      continue;
    }
    // Only call Connect() for types that completed LoadModels()
    // successfully. Such types shouldn't be in an error state at the same
    // time.
    DCHECK(!data_type_status_table_.GetFailedTypes().Has(dtc->type()));

    std::unique_ptr<DataTypeActivationResponse> activation_response =
        dtc->Connect();
    DCHECK(activation_response);
    DCHECK_EQ(dtc->state(), DataTypeController::RUNNING);

    if (activation_response->skip_engine_connection) {
      // |skip_engine_connection| means ConnectDataType() shouldn't be invoked
      // because the datatype has some alternative way to sync changes to the
      // server, without relying on this instance of the sync engine. This is
      // currently possible for PROXY_TABS and, on Android, for PASSWORDS.
      DCHECK(!activation_response->type_processor);
      downloaded_types_.Put(type);
      configured_proxy_types_.Put(type);
      continue;
    }

    if (activation_response->model_type_state.initial_sync_done()) {
      downloaded_types_.Put(type);
    } else {
      downloaded_types_.Remove(type);
    }
    if (force_redownload_types_.Has(type)) {
      downloaded_types_.Remove(type);
    }

    configurer_->ConnectDataType(type, std::move(activation_response));
  }
}

// static
ModelTypeSet DataTypeManagerImpl::GetDataTypesInState(
    DataTypeConfigState state,
    const DataTypeConfigStateMap& state_map) {
  ModelTypeSet types;
  for (const auto& [type, config_state] : state_map) {
    if (config_state == state)
      types.Put(type);
  }
  return types;
}

// static
void DataTypeManagerImpl::SetDataTypesState(DataTypeConfigState state,
                                            ModelTypeSet types,
                                            DataTypeConfigStateMap* state_map) {
  for (ModelType type : types) {
    (*state_map)[type] = state;
  }
}

DataTypeManagerImpl::DataTypeConfigStateMap
DataTypeManagerImpl::BuildDataTypeConfigStateMap(
    const ModelTypeSet& types_being_configured) const {
  // 1. Get the failed types (due to fatal, crypto, and unready errors).
  // 2. Add the difference between preferred_types_ and the failed types as
  //    CONFIGURE_INACTIVE.
  // 3. Flip |types_being_configured| to CONFIGURE_ACTIVE.
  // 4. Set non-enabled user types as DISABLED.
  // 5. Set the fatal, crypto, and unready types to their respective states.
  const ModelTypeSet fatal_types = data_type_status_table_.GetFatalErrorTypes();
  const ModelTypeSet crypto_types =
      data_type_status_table_.GetCryptoErrorTypes();
  // Types with unready errors do not count as unready if they've been disabled.
  const ModelTypeSet unready_types = Intersection(
      data_type_status_table_.GetUnreadyErrorTypes(), preferred_types_);

  const ModelTypeSet enabled_types = GetEnabledTypes();

  const ModelTypeSet disabled_types =
      Difference(Union(UserTypes(), ControlTypes()), enabled_types);
  const ModelTypeSet to_configure =
      Intersection(enabled_types, types_being_configured);
  DVLOG(1) << "Enabling: " << ModelTypeSetToDebugString(enabled_types);
  DVLOG(1) << "Configuring: " << ModelTypeSetToDebugString(to_configure);
  DVLOG(1) << "Disabling: " << ModelTypeSetToDebugString(disabled_types);

  DataTypeConfigStateMap config_state_map;
  SetDataTypesState(CONFIGURE_INACTIVE, enabled_types, &config_state_map);
  SetDataTypesState(CONFIGURE_ACTIVE, to_configure, &config_state_map);
  SetDataTypesState(DISABLED, disabled_types, &config_state_map);
  SetDataTypesState(FATAL, fatal_types, &config_state_map);
  SetDataTypesState(CRYPTO, crypto_types, &config_state_map);
  SetDataTypesState(UNREADY, unready_types, &config_state_map);
  return config_state_map;
}

void DataTypeManagerImpl::Restart() {
  DVLOG(1) << "Restarting...";
  const ConfigureReason reason = last_requested_context_.reason;

  // Only record the type histograms for user-triggered configurations or
  // restarts.
  if (reason == CONFIGURE_REASON_RECONFIGURATION ||
      reason == CONFIGURE_REASON_NEW_CLIENT ||
      reason == CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE) {
    for (ModelType type : preferred_types_) {
      UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureDataTypes",
                                ModelTypeHistogramValue(type));
    }
  }

  // Check for new or resolved data type crypto errors.
  if (encryption_handler_->HasCryptoError()) {
    ModelTypeSet encrypted_types = encryption_handler_->GetEncryptedDataTypes();
    encrypted_types.RetainAll(preferred_types_);
    encrypted_types.RemoveAll(data_type_status_table_.GetCryptoErrorTypes());
    DataTypeStatusTable::TypeErrorMap crypto_errors =
        GenerateCryptoErrorsForTypes(encrypted_types);
    data_type_status_table_.UpdateFailedDataTypes(crypto_errors);
  } else {
    data_type_status_table_.ResetCryptoErrors();
  }

  UpdatePreconditionErrors(preferred_types_);

  last_restart_time_ = base::Time::Now();
  configuration_stats_.clear();

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == RETRYING);

  const State old_state = state_;
  state_ = CONFIGURING;

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  // Note: NotifyStart() must be called with the updated (non-idle) state,
  // otherwise logic listening for the configuration start might not be aware
  // of the fact that the DTM is in a configuration state.
  if (old_state == STOPPED || old_state == CONFIGURED)
    NotifyStart();

  // Compute `last_enabled_types_` after NotifyStart() to be sure to provide
  // consistent values to ModelLoadManager. (Namely, observers may trigger
  // another reconfiguration which may change the value of `preferred_types_`.)
  last_enabled_types_ = GetEnabledTypes();
  configuration_types_queue_ = PrioritizeTypes(last_enabled_types_);

  model_load_manager_.Initialize(
      /*desired_types=*/last_enabled_types_,
      /*preferred_types=*/preferred_types_, last_requested_context_);
}

void DataTypeManagerImpl::OnAllDataTypesReadyForConfigure() {
  // If a reconfigure was requested while the data types were loading, process
  // it now.
  if (needs_reconfigure_) {
    configuration_types_queue_ = base::queue<ModelTypeSet>();
    ProcessReconfigure();
    return;
  }
  // TODO(pavely): By now some of datatypes in |configuration_types_queue_|
  // could have failed loading and should be excluded from configuration. I need
  // to adjust |configuration_types_queue_| for such types.
  ConnectDataTypes();

  // Propagate the state of PROXY_TABS to the sync engine.
  auto dtc_iter = controllers_->find(PROXY_TABS);
  if (dtc_iter != controllers_->end()) {
    configurer_->SetProxyTabsDatatypeEnabled(dtc_iter->second->state() ==
                                             DataTypeController::RUNNING);
  }

  StartNextConfiguration(/*higher_priority_types_before=*/ModelTypeSet());
}

void DataTypeManagerImpl::UpdatePreconditionErrors(
    const ModelTypeSet& desired_types) {
  for (ModelType type : desired_types) {
    UpdatePreconditionError(type);
  }
}

bool DataTypeManagerImpl::UpdatePreconditionError(ModelType type) {
  auto iter = controllers_->find(type);
  if (iter == controllers_->end())
    return false;

  switch (iter->second->GetPreconditionState()) {
    case DataTypeController::PreconditionState::kPreconditionsMet: {
      const bool data_type_policy_error_changed =
          data_type_status_table_.ResetDataTypePolicyErrorFor(type);
      const bool unready_status_changed =
          data_type_status_table_.ResetUnreadyErrorFor(type);
      if (!data_type_policy_error_changed && !unready_status_changed) {
        // Nothing changed.
        return false;
      }
      // If preconditions are newly met, the datatype should be immediately
      // redownloaded as part of the datatype configuration (most relevant for
      // the UNREADY_ERROR case which usually won't clear sync metadata).
      force_redownload_types_.Put(type);
      return true;
    }

    case DataTypeController::PreconditionState::kMustStopAndClearData: {
      return data_type_status_table_.UpdateFailedDataType(
          type, SyncError(FROM_HERE, SyncError::DATATYPE_POLICY_ERROR,
                          "Datatype preconditions not met.", type));
    }

    case DataTypeController::PreconditionState::kMustStopAndKeepData: {
      return data_type_status_table_.UpdateFailedDataType(
          type, SyncError(FROM_HERE, SyncError::UNREADY_ERROR,
                          "Datatype not ready at config time.", type));
    }
  }

  NOTREACHED();
  return false;
}

void DataTypeManagerImpl::ProcessReconfigure() {
  // This may have been called asynchronously; no-op if it is no longer needed.
  if (!needs_reconfigure_) {
    return;
  }

  // Wait for current configuration to finish.
  if (!configuration_types_queue_.empty()) {
    return;
  }

  // An attempt was made to reconfigure while we were already configuring.
  // This can be because a passphrase was accepted or the user changed the
  // set of desired types. Either way, |preferred_types_| will contain the most
  // recent set of desired types, so we just call configure.
  // Note: we do this whether or not GetControllersNeedingStart is true,
  // because we may need to stop datatypes.
  DVLOG(1) << "Reconfiguring due to previous configure attempt occurring while"
           << " busy.";

  // Note: ConfigureImpl is called directly, rather than posted, in order to
  // ensure that any purging happens while the set of failed types is still up
  // to date. If stack unwinding were to be done via PostTask, the failed data
  // types may be reset before the purging was performed.
  state_ = RETRYING;
  needs_reconfigure_ = false;
  ConfigureImpl(preferred_types_, last_requested_context_);
}

void DataTypeManagerImpl::ConfigurationCompleted(
    AssociationTypesInfo association_types_info,
    ModelTypeSet configured_types,
    ModelTypeSet succeeded_configuration_types,
    ModelTypeSet failed_configuration_types) {
  // Note: |configured_types| are the types we requested to configure. Some of
  // them might have been downloaded already. |succeeded_configuration_types|
  // are the ones that were actually downloaded just now.
  DCHECK_EQ(CONFIGURING, state_);

  if (!failed_configuration_types.Empty()) {
    DataTypeStatusTable::TypeErrorMap errors;
    for (ModelType type : failed_configuration_types) {
      SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                      "Backend failed to download and configure type.", type);
      errors[type] = error;
    }
    data_type_status_table_.UpdateFailedDataTypes(errors);
    needs_reconfigure_ = true;
  }

  // If a reconfigure was requested while this configuration was ongoing,
  // process it now.
  if (needs_reconfigure_) {
    configuration_types_queue_ = base::queue<ModelTypeSet>();
    ProcessReconfigure();
    return;
  }

  DCHECK(!configuration_types_queue_.empty());
  DCHECK(configuration_types_queue_.front() == configured_types);
  configuration_types_queue_.pop();

  association_types_info.first_sync_types = succeeded_configuration_types;
  association_types_info.download_ready_time = base::Time::Now();
  RecordConfigurationStats(association_types_info);

  if (configuration_types_queue_.empty()) {
    state_ = CONFIGURED;
    NotifyDone(ConfigureResult(OK, preferred_types_));
    return;
  }

  StartNextConfiguration(/*higher_priority_types_before=*/configured_types);
}

void DataTypeManagerImpl::StartNextConfiguration(
    ModelTypeSet higher_priority_types_before) {
  if (configuration_types_queue_.empty())
    return;

  AssociationTypesInfo association_types_info;
  association_types_info.types = configuration_types_queue_.front();
  association_types_info.download_start_time = base::Time::Now();
  association_types_info.higher_priority_types_before =
      higher_priority_types_before;

  // The engine's state was initially derived from the types detected to have
  // been downloaded in the database. Afterwards it is modified only by this
  // function. We expect |downloaded_types_| to remain consistent because
  // configuration requests are never aborted; they are retried until they
  // succeed or the engine is shut down.
  //
  // Only one configure is allowed at a time. This is guaranteed by our callers.
  // The sync engine requests one configure as it is initializing and waits for
  // it to complete. After engine initialization, all configurations pass
  // through the DataTypeManager, and we are careful to never send a new
  // configure request until the current request succeeds.
  configurer_->ConfigureDataTypes(
      PrepareConfigureParams(association_types_info));
}

ModelTypeConfigurer::ConfigureParams
DataTypeManagerImpl::PrepareConfigureParams(
    const AssociationTypesInfo& association_types_info) {
  // Divide up the types into their corresponding actions:
  // - Types which are newly enabled are downloaded.
  // - Types which have encountered a cryptographer error (crypto_types) are
  //   unapplied (local state is purged but sync state is not).
  // - All types not in the routing info (types just disabled) are deleted.
  // - Everything else (enabled types and already disabled types) is not
  //   touched.
  const DataTypeConfigStateMap config_state_map =
      BuildDataTypeConfigStateMap(configuration_types_queue_.front());
  const ModelTypeSet fatal_types = GetDataTypesInState(FATAL, config_state_map);
  const ModelTypeSet crypto_types =
      GetDataTypesInState(CRYPTO, config_state_map);
  const ModelTypeSet unready_types =
      GetDataTypesInState(UNREADY, config_state_map);
  const ModelTypeSet active_types =
      GetDataTypesInState(CONFIGURE_ACTIVE, config_state_map);
  const ModelTypeSet inactive_types =
      GetDataTypesInState(CONFIGURE_INACTIVE, config_state_map);

  ModelTypeSet disabled_types = GetDataTypesInState(DISABLED, config_state_map);
  disabled_types.PutAll(fatal_types);
  disabled_types.PutAll(crypto_types);
  disabled_types.PutAll(unready_types);

  DCHECK(Intersection(active_types, disabled_types).Empty());

  ModelTypeSet types_to_download = Difference(active_types, downloaded_types_);
  // Commit-only types never require downloading.
  types_to_download.RemoveAll(CommitOnlyTypes());
  if (!types_to_download.Empty()) {
    types_to_download.PutAll(ControlTypes());
  }

  // All types to download are expected to be protocol types (proxy types should
  // have skipped full activation via
  // |DataTypeActivationResponse::skip_engine_connection|).
  DCHECK(ProtocolTypes().HasAll(types_to_download));

  // Already (optimistically) update the |downloaded_types_|, so that the next
  // time we get here, it has the correct value.
  downloaded_types_.PutAll(active_types);
  // Assume that disabled types are not downloaded anymore - if they get
  // re-enabled, we'll want to re-download them as well.
  downloaded_types_.RemoveAll(disabled_types);
  force_redownload_types_.RemoveAll(types_to_download);

  ModelTypeSet types_to_purge;
  // If we're using transport-only mode, don't clear any old data. The reason is
  // that if a user temporarily disables Sync, we don't want to wipe (and later
  // redownload) all their data, just because Sync restarted in transport-only
  // mode.
  // TODO(crbug.com/1142771): "Purging" logic is only implemented for NIGORI -
  // verify whether it is actually needed at all.
  if (last_requested_context_.sync_mode == SyncMode::kFull) {
    types_to_purge = Difference(ModelTypeSet::All(), downloaded_types_);
    types_to_purge.RemoveAll(inactive_types);
    types_to_purge.RemoveAll(unready_types);
  }
  DCHECK(Intersection(active_types, types_to_purge).Empty());

  DCHECK(Intersection(downloaded_types_, crypto_types).Empty());
  // |downloaded_types_| was already updated to include all enabled types.
  DCHECK(downloaded_types_.HasAll(types_to_download));

  DVLOG(1) << "Types " << ModelTypeSetToDebugString(types_to_download)
           << " added; calling ConfigureDataTypes";

  ModelTypeConfigurer::ConfigureParams params;
  params.reason = last_requested_context_.reason;
  params.to_download = types_to_download;
  params.to_purge = types_to_purge;
  params.ready_task =
      base::BindOnce(&DataTypeManagerImpl::ConfigurationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), association_types_info,
                     configuration_types_queue_.front());
  params.is_sync_feature_enabled =
      last_requested_context_.sync_mode == SyncMode::kFull;

  return params;
}

void DataTypeManagerImpl::RecordConfigurationStats(
    const AssociationTypesInfo& association_types_info) {
  DCHECK(state_ == CONFIGURING);

  ModelTypeSet same_priority_types_configured_before;
  for (ModelType type : association_types_info.types) {
    if (ProtocolTypes().Has(type)) {
      RecordConfigurationStatsImpl(association_types_info, type,
                                   same_priority_types_configured_before);
      same_priority_types_configured_before.Put(type);
    }
  }
}

void DataTypeManagerImpl::OnSingleDataTypeWillStop(ModelType type,
                                                   const SyncError& error) {
  // No-op if the type is not connected.
  configurer_->DisconnectDataType(type);
  configured_proxy_types_.Remove(type);

  if (error.IsSet()) {
    data_type_status_table_.UpdateFailedDataType(type, error);
    needs_reconfigure_ = true;
    last_requested_context_.reason =
        GetReasonForProgrammaticReconfigure(last_requested_context_.reason);
    // Do this asynchronously so the ModelLoadManager has a chance to
    // finish stopping this type, otherwise Disconnect() and Stop()
    // end up getting called twice on the controller.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DataTypeManagerImpl::ProcessReconfigure,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void DataTypeManagerImpl::RecordConfigurationStatsImpl(
    const AssociationTypesInfo& association_types_info,
    ModelType type,
    ModelTypeSet same_priority_types_configured_before) {
  if (!debug_info_listener_.IsInitialized())
    return;

  DCHECK_EQ(configuration_stats_.count(type), 0u);

  configuration_stats_[type].model_type = type;
  if (association_types_info.types.Has(type)) {
    configuration_stats_[type].download_wait_time =
        association_types_info.download_start_time - last_restart_time_;
    if (association_types_info.first_sync_types.Has(type)) {
      configuration_stats_[type].download_time =
          association_types_info.download_ready_time -
          association_types_info.download_start_time;
    }
    configuration_stats_[type].high_priority_types_configured_before =
        association_types_info.higher_priority_types_before;
    configuration_stats_[type].same_priority_types_configured_before =
        same_priority_types_configured_before;
  }
}

void DataTypeManagerImpl::Stop(ShutdownReason reason) {
  if (state_ == STOPPED)
    return;

  bool need_to_notify = state_ == CONFIGURING;
  StopImpl(reason);

  if (need_to_notify) {
    ConfigureResult result(ABORTED, preferred_types_);
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::StopImpl(ShutdownReason reason) {
  state_ = STOPPING;

  // Invalidate weak pointer to drop configuration callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop all data types.
  model_load_manager_.Stop(reason);

  // Individual data type controllers might still be STOPPING, but we don't
  // reflect that in |state_| because, for all practical matters, the manager is
  // in a ready state and reconfguration can be triggered.
  // TODO(mastiz): Reconsider waiting in STOPPING state until all datatypes have
  // stopped.
  state_ = STOPPED;
}

void DataTypeManagerImpl::NotifyStart() {
  observer_->OnConfigureStart();
}

void DataTypeManagerImpl::NotifyDone(const ConfigureResult& raw_result) {
  DCHECK(!last_restart_time_.is_null());
  base::TimeDelta configure_time = base::Time::Now() - last_restart_time_;

  ConfigureResult result = raw_result;
  result.data_type_status_table = data_type_status_table_;

  const std::string prefix_uma =
      (last_requested_context_.reason == CONFIGURE_REASON_NEW_CLIENT)
          ? "Sync.ConfigureTime_Initial"
          : "Sync.ConfigureTime_Subsequent";

  DVLOG(1) << "Total time spent configuring: " << configure_time.InSecondsF()
           << "s";
  switch (result.status) {
    case DataTypeManager::OK:
      DVLOG(1) << "NotifyDone called with result: OK";
      base::UmaHistogramLongTimes(prefix_uma + ".OK", configure_time);
      if (debug_info_listener_.IsInitialized() &&
          !configuration_stats_.empty()) {
        std::vector<DataTypeConfigurationStats> stats;
        for (auto& [type, stat] : configuration_stats_) {
          // Note: |configuration_stats_| gets cleared below, so it's okay to
          // destroy its contents here.
          stats.push_back(std::move(stat));
        }
        debug_info_listener_.Call(
            FROM_HERE, &DataTypeDebugInfoListener::OnDataTypeConfigureComplete,
            stats);
      }
      configuration_stats_.clear();
      break;
    case DataTypeManager::ABORTED:
      DVLOG(1) << "NotifyDone called with result: ABORTED";
      base::UmaHistogramLongTimes(prefix_uma + ".ABORTED", configure_time);
      break;
    case DataTypeManager::UNKNOWN:
      NOTREACHED();
      break;
  }
  observer_->OnConfigureDone(result);
}

ModelTypeSet DataTypeManagerImpl::GetActiveDataTypes() const {
  if (state_ != CONFIGURED)
    return ModelTypeSet();
  return GetEnabledTypes();
}

ModelTypeSet DataTypeManagerImpl::GetPurgedDataTypes() const {
  ModelTypeSet purged_types;

  for (const auto& [type, controller] : *controllers_) {
    // TODO(crbug.com/897628): NOT_RUNNING doesn't necessarily mean the sync
    // metadata was cleared, if KEEP_METADATA was used when stopping.
    if (controller->state() == DataTypeController::NOT_RUNNING) {
      purged_types.Put(type);
    }
  }

  return purged_types;
}

ModelTypeSet DataTypeManagerImpl::GetActiveProxyDataTypes() const {
  if (state_ != CONFIGURED) {
    return ModelTypeSet();
  }
  return configured_proxy_types_;
}

DataTypeManager::State DataTypeManagerImpl::state() const {
  return state_;
}

ModelTypeSet DataTypeManagerImpl::GetEnabledTypes() const {
  return Difference(preferred_types_, data_type_status_table_.GetFailedTypes());
}

}  // namespace syncer
