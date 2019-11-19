// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_manager_impl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
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

}  // namespace

DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo() {}
DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo(
    const AssociationTypesInfo& other) = default;
DataTypeManagerImpl::AssociationTypesInfo::~AssociationTypesInfo() {}

DataTypeManagerImpl::DataTypeManagerImpl(
    ModelTypeSet initial_types,
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const DataTypeEncryptionHandler* encryption_handler,
    ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer)
    : downloaded_types_(initial_types),
      configurer_(configurer),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      debug_info_listener_(debug_info_listener),
      model_association_manager_(controllers, this),
      observer_(observer),
      encryption_handler_(encryption_handler),
      download_started_(false) {
  DCHECK(configurer_);
  DCHECK(observer_);

  // Check if any of the controllers are already in a FAILED state, and if so,
  // mark them accordingly in the status table.
  DataTypeStatusTable::TypeErrorMap existing_errors;
  for (const auto& kv : *controllers_) {
    ModelType type = kv.first;
    const DataTypeController* controller = kv.second.get();
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

DataTypeManagerImpl::~DataTypeManagerImpl() {}

void DataTypeManagerImpl::Configure(ModelTypeSet desired_types,
                                    const ConfigureContext& context) {
  desired_types.PutAll(ControlTypes());

  ModelTypeSet allowed_types = ControlTypes();
  // Add types with controllers.
  for (const auto& kv : *controllers_) {
    allowed_types.Put(kv.first);
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
      if (last_requested_types_.Has(type)) {
        // Only reconfigure if the type is both ready and desired. This will
        // internally also update ready state of all other requested types.
        ForceReconfiguration();
      }
      break;

    case DataTypeController::PreconditionState::kMustStopAndClearData:
      model_association_manager_.StopDatatype(
          type, DISABLE_SYNC,
          SyncError(FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                    "Datatype preconditions not met.", type));
      break;

    case DataTypeController::PreconditionState::kMustStopAndKeepData:
      model_association_manager_.StopDatatype(
          type, STOP_SYNC,
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
  ModelTypeSet remainder = Difference(last_requested_types_, undesired_types);
  last_requested_context_.reason = CONFIGURE_REASON_MIGRATION;
  ConfigureImpl(remainder, last_requested_context_);
}

void DataTypeManagerImpl::ConfigureImpl(ModelTypeSet desired_types,
                                        const ConfigureContext& context) {
  DCHECK_NE(context.reason, CONFIGURE_REASON_UNKNOWN);
  DVLOG(1) << "Configuring for " << ModelTypeSetToString(desired_types)
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

  last_requested_types_ = desired_types;
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

void DataTypeManagerImpl::RegisterTypesWithBackend() {
  for (ModelType type : last_enabled_types_) {
    const auto& dtc_iter = controllers_->find(type);
    if (dtc_iter == controllers_->end())
      continue;
    DataTypeController* dtc = dtc_iter->second.get();
    if (dtc->state() == DataTypeController::MODEL_LOADED) {
      // Only call RegisterWithBackend for types that completed LoadModels
      // successfully. Such types shouldn't be in an error state at the same
      // time.
      DCHECK(!data_type_status_table_.GetFailedTypes().Has(dtc->type()));
      switch (dtc->RegisterWithBackend(configurer_)) {
        case DataTypeController::REGISTRATION_IGNORED:
          break;
        case DataTypeController::TYPE_ALREADY_DOWNLOADED:
          downloaded_types_.Put(type);
          break;
        case DataTypeController::TYPE_NOT_YET_DOWNLOADED:
          downloaded_types_.Remove(type);
          break;
      }
      if (force_redownload_types_.Has(type)) {
        downloaded_types_.Remove(type);
      }
    }
  }
}

// static
ModelTypeSet DataTypeManagerImpl::GetDataTypesInState(
    DataTypeConfigState state,
    const DataTypeConfigStateMap& state_map) {
  ModelTypeSet types;
  for (const auto& kv : state_map) {
    if (kv.second == state)
      types.Put(kv.first);
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
  // 2. Add the difference between last_requested_types_ and the failed types
  //    as CONFIGURE_INACTIVE.
  // 3. Flip |types_being_configured| to CONFIGURE_ACTIVE.
  // 4. Set non-enabled user types as DISABLED.
  // 5. Set the fatal, crypto, and unready types to their respective states.
  ModelTypeSet fatal_types = data_type_status_table_.GetFatalErrorTypes();
  ModelTypeSet crypto_types = data_type_status_table_.GetCryptoErrorTypes();
  ModelTypeSet unready_types = data_type_status_table_.GetUnreadyErrorTypes();

  // Types with persistence errors are only purged/resynced when they're
  // actively being configured.
  ModelTypeSet clean_types = data_type_status_table_.GetPersistenceErrorTypes();
  clean_types.RetainAll(types_being_configured);

  // Types with unready errors do not count as unready if they've been disabled.
  unready_types.RetainAll(last_requested_types_);

  ModelTypeSet enabled_types = GetEnabledTypes();

  ModelTypeSet disabled_types =
      Difference(Union(UserTypes(), ControlTypes()), enabled_types);
  ModelTypeSet to_configure =
      Intersection(enabled_types, types_being_configured);
  DVLOG(1) << "Enabling: " << ModelTypeSetToString(enabled_types);
  DVLOG(1) << "Configuring: " << ModelTypeSetToString(to_configure);
  DVLOG(1) << "Disabling: " << ModelTypeSetToString(disabled_types);

  DataTypeConfigStateMap config_state_map;
  SetDataTypesState(CONFIGURE_INACTIVE, enabled_types, &config_state_map);
  SetDataTypesState(CONFIGURE_ACTIVE, to_configure, &config_state_map);
  SetDataTypesState(CONFIGURE_CLEAN, clean_types, &config_state_map);
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
    for (ModelType type : last_requested_types_) {
      // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
      UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureDataTypes",
                                ModelTypeHistogramValue(type));
    }
  }

  // Check for new or resolved data type crypto errors.
  if (encryption_handler_->HasCryptoError()) {
    ModelTypeSet encrypted_types = encryption_handler_->GetEncryptedDataTypes();
    encrypted_types.RetainAll(last_requested_types_);
    encrypted_types.RemoveAll(data_type_status_table_.GetCryptoErrorTypes());
    DataTypeStatusTable::TypeErrorMap crypto_errors =
        GenerateCryptoErrorsForTypes(encrypted_types);
    data_type_status_table_.UpdateFailedDataTypes(crypto_errors);
  } else {
    data_type_status_table_.ResetCryptoErrors();
  }

  UpdatePreconditionErrors(last_requested_types_);

  last_enabled_types_ = GetEnabledTypes();
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

  download_types_queue_ = PrioritizeTypes(last_enabled_types_);
  association_types_queue_ = base::queue<AssociationTypesInfo>();

  download_started_ = false;
  model_association_manager_.Initialize(
      /*desired_types=*/last_enabled_types_,
      /*preferred_types=*/last_requested_types_, last_requested_context_);
}

void DataTypeManagerImpl::OnAllDataTypesReadyForConfigure() {
  DCHECK(!download_started_);
  download_started_ = true;
  UMA_HISTOGRAM_LONG_TIMES("Sync.USSLoadModelsTime",
                           base::Time::Now() - last_restart_time_);
  // TODO(pavely): By now some of datatypes in |download_types_queue_| could
  // have failed loading and should be excluded from configuration. I need to
  // adjust |download_types_queue_| for such types.
  RegisterTypesWithBackend();
  StartNextDownload(ModelTypeSet());
}

ModelTypeSet DataTypeManagerImpl::GetPriorityTypes() const {
  ModelTypeSet high_priority_types;
  high_priority_types.PutAll(ControlTypes());
  high_priority_types.PutAll(PriorityUserTypes());
  return high_priority_types;
}

TypeSetPriorityList DataTypeManagerImpl::PrioritizeTypes(
    const ModelTypeSet& types) {
  ModelTypeSet high_priority_types = GetPriorityTypes();
  high_priority_types.RetainAll(types);

  ModelTypeSet low_priority_types = Difference(types, high_priority_types);

  TypeSetPriorityList result;
  if (!high_priority_types.Empty())
    result.push(high_priority_types);
  if (!low_priority_types.Empty())
    result.push(low_priority_types);

  // Could be empty in case of purging for migration, sync nothing, etc.
  // Configure empty set to purge data from backend.
  if (result.empty())
    result.push(ModelTypeSet());

  return result;
}

void DataTypeManagerImpl::UpdatePreconditionErrors(
    const ModelTypeSet& desired_types) {
  for (ModelType type : desired_types) {
    UpdatePreconditionError(type);
  }
}

bool DataTypeManagerImpl::UpdatePreconditionError(ModelType type) {
  const auto& iter = controllers_->find(type);
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

  // Wait for current download and association to finish.
  if (!download_types_queue_.empty() ||
      model_association_manager_.state() ==
          ModelAssociationManager::ASSOCIATING) {
    return;
  }

  association_types_queue_ = base::queue<AssociationTypesInfo>();

  // An attempt was made to reconfigure while we were already configuring.
  // This can be because a passphrase was accepted or the user changed the
  // set of desired types. Either way, |last_requested_types_| will contain
  // the most recent set of desired types, so we just call configure.
  // Note: we do this whether or not GetControllersNeedingStart is true,
  // because we may need to stop datatypes.
  DVLOG(1) << "Reconfiguring due to previous configure attempt occurring while"
           << " busy.";

  // Note: ConfigureImpl is called directly, rather than posted, in order to
  // ensure that any purging/unapplying/journaling happens while the set of
  // failed types is still up to date. If stack unwinding were to be done
  // via PostTask, the failed data types may be reset before the purging was
  // performed.
  state_ = RETRYING;
  needs_reconfigure_ = false;
  ConfigureImpl(last_requested_types_, last_requested_context_);
}

void DataTypeManagerImpl::DownloadReady(
    ModelTypeSet types_to_download,
    ModelTypeSet first_sync_types,
    ModelTypeSet failed_configuration_types) {
  DCHECK_EQ(CONFIGURING, state_);

  // Persistence errors are reset after each backend configuration attempt
  // during which they would have been purged.
  data_type_status_table_.ResetPersistenceErrorsFrom(types_to_download);

  if (!failed_configuration_types.Empty()) {
    DataTypeStatusTable::TypeErrorMap errors;
    for (ModelType type : failed_configuration_types) {
      SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                      "Backend failed to download type.", type);
      errors[type] = error;
    }
    data_type_status_table_.UpdateFailedDataTypes(errors);
    needs_reconfigure_ = true;
  }

  if (needs_reconfigure_) {
    download_types_queue_ = TypeSetPriorityList();
    ProcessReconfigure();
    return;
  }

  DCHECK(!download_types_queue_.empty());
  download_types_queue_.pop();

  // Those types that were already downloaded (non first sync/error types)
  // should already be associating. Just kick off association of the newly
  // downloaded types if necessary.
  if (!association_types_queue_.empty()) {
    association_types_queue_.back().first_sync_types = first_sync_types;
    association_types_queue_.back().download_ready_time = base::Time::Now();
    StartNextAssociation(UNREADY_AT_CONFIG);
  } else if (download_types_queue_.empty() &&
             model_association_manager_.state() !=
                 ModelAssociationManager::ASSOCIATING) {
    // There's nothing more to download or associate (implying either there were
    // no types to associate or they associated as part of |ready_types|).
    // If the model association manager is also finished, then we're done
    // configuring.
    state_ = CONFIGURED;
    ConfigureResult result(OK, last_requested_types_);
    NotifyDone(result);
    return;
  }

  StartNextDownload(types_to_download);
}

void DataTypeManagerImpl::StartNextDownload(
    ModelTypeSet high_priority_types_before) {
  if (download_types_queue_.empty())
    return;

  ModelTypeConfigurer::ConfigureParams config_params;
  ModelTypeSet ready_types = PrepareConfigureParams(&config_params);

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
  configurer_->ConfigureDataTypes(std::move(config_params));

  AssociationTypesInfo association_info;
  association_info.types = download_types_queue_.front();
  association_info.ready_types = ready_types;
  association_info.download_start_time = base::Time::Now();
  association_info.high_priority_types_before = high_priority_types_before;
  association_types_queue_.push(association_info);

  // Start associating those types that are already downloaded (does nothing
  // if model associator is busy).
  StartNextAssociation(READY_AT_CONFIG);
}

ModelTypeSet DataTypeManagerImpl::PrepareConfigureParams(
    ModelTypeConfigurer::ConfigureParams* params) {
  // Divide up the types into their corresponding actions:
  // - Types which are newly enabled are downloaded.
  // - Types which have encountered a fatal error (fatal_types) are deleted
  //   from the directory and journaled in the delete journal.
  // - Types which have encountered a cryptographer error (crypto_types) are
  //   unapplied (local state is purged but sync state is not).
  // - All other types not in the routing info (types just disabled) are deleted
  //   from the directory.
  // - Everything else (enabled types and already disabled types) is not
  //   touched.
  const DataTypeConfigStateMap config_state_map =
      BuildDataTypeConfigStateMap(download_types_queue_.front());
  const ModelTypeSet fatal_types = GetDataTypesInState(FATAL, config_state_map);
  const ModelTypeSet crypto_types =
      GetDataTypesInState(CRYPTO, config_state_map);
  const ModelTypeSet unready_types =
      GetDataTypesInState(UNREADY, config_state_map);
  const ModelTypeSet active_types =
      GetDataTypesInState(CONFIGURE_ACTIVE, config_state_map);
  const ModelTypeSet clean_types =
      GetDataTypesInState(CONFIGURE_CLEAN, config_state_map);
  const ModelTypeSet inactive_types =
      GetDataTypesInState(CONFIGURE_INACTIVE, config_state_map);

  ModelTypeSet enabled_types = Union(active_types, clean_types);
  ModelTypeSet disabled_types = GetDataTypesInState(DISABLED, config_state_map);
  disabled_types.PutAll(fatal_types);
  disabled_types.PutAll(crypto_types);
  disabled_types.PutAll(unready_types);

  DCHECK(Intersection(enabled_types, disabled_types).Empty());

  // The sync engine's enabled types will be updated by adding |enabled_types|
  // to the list then removing |disabled_types|. Any types which are not in
  // either of those sets will remain untouched. Types which were not in
  // |downloaded_types_| previously are not fully downloaded, so we must ask the
  // engine to download them. Any newly supported datatypes won't have been in
  // |downloaded_types_|, so they will also be downloaded if they are enabled.
  ModelTypeSet types_to_download = Difference(enabled_types, downloaded_types_);
  downloaded_types_.PutAll(enabled_types);
  downloaded_types_.RemoveAll(disabled_types);

  types_to_download.PutAll(clean_types);
  types_to_download.RemoveAll(ProxyTypes());
  types_to_download.RemoveAll(CommitOnlyTypes());
  if (!types_to_download.Empty())
    types_to_download.Put(NIGORI);

  force_redownload_types_.RemoveAll(types_to_download);

  // TODO(sync): crbug.com/137550.
  // It's dangerous to configure types that have progress markers. Types with
  // progress markers can trigger a MIGRATION_DONE response. We are not
  // prepared to handle a migration during a configure, so we must ensure that
  // all our types_to_download actually contain no data before we sync them.
  //
  // One common way to end up in this situation used to be types which
  // downloaded some or all of their data but have not applied it yet. We avoid
  // problems with those types by purging the data of any such partially synced
  // types soon after we load the directory.
  //
  // Another possible scenario is that we have newly supported or newly enabled
  // data types being downloaded here but the nigori type, which is always
  // included in any GetUpdates request, requires migration. The server has
  // code to detect this scenario based on the configure reason, the fact that
  // the nigori type is the only requested type which requires migration, and
  // that the requested types list includes at least one non-nigori type. It
  // will not send a MIGRATION_DONE response in that case. We still need to be
  // careful to not send progress markers for non-nigori types, though. If a
  // non-nigori type in the request requires migration, a MIGRATION_DONE
  // response will be sent.

  ModelTypeSet types_to_purge;
  // If we're using transport-only mode, don't clear any old data. The reason is
  // that if a user temporarily disables Sync, we don't want to wipe (and later
  // redownload) all their data, just because Sync restarted in transport-only
  // mode.
  if (last_requested_context_.sync_mode == SyncMode::kFull) {
    types_to_purge = Difference(ModelTypeSet::All(), downloaded_types_);
    // Include clean_types in types_to_purge, they are part of
    // |downloaded_types_|, but still need to be cleared.
    DCHECK(downloaded_types_.HasAll(clean_types));
    types_to_purge.PutAll(clean_types);
    types_to_purge.RemoveAll(inactive_types);
    types_to_purge.RemoveAll(unready_types);
  }

  // If a type has already been disabled and unapplied or journaled, it will
  // not be part of the |types_to_purge| set, and therefore does not need
  // to be acted on again.
  ModelTypeSet types_to_journal = Intersection(fatal_types, types_to_purge);
  ModelTypeSet unapply_types = Union(crypto_types, clean_types);
  unapply_types.RetainAll(types_to_purge);

  DCHECK(Intersection(downloaded_types_, types_to_journal).Empty());
  DCHECK(Intersection(downloaded_types_, crypto_types).Empty());
  // |downloaded_types_| was already updated to include all enabled types.
  DCHECK(downloaded_types_.HasAll(types_to_download));

  DVLOG(1) << "Types " << ModelTypeSetToString(types_to_download)
           << " added; calling ConfigureDataTypes";

  params->reason = last_requested_context_.reason;
  params->enabled_types = enabled_types;
  params->disabled_types = disabled_types;
  params->to_download = types_to_download;
  params->to_purge = types_to_purge;
  params->to_journal = types_to_journal;
  params->to_unapply = unapply_types;
  params->ready_task =
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr(), download_types_queue_.front());
  params->is_sync_feature_enabled =
      last_requested_context_.sync_mode == SyncMode::kFull;

  DCHECK(Intersection(active_types, types_to_purge).Empty());
  DCHECK(Intersection(active_types, fatal_types).Empty());
  DCHECK(Intersection(active_types, unapply_types).Empty());
  DCHECK(Intersection(active_types, inactive_types).Empty());
  return Difference(active_types, types_to_download);
}

void DataTypeManagerImpl::StartNextAssociation(AssociationGroup group) {
  DCHECK(!association_types_queue_.empty());

  // If the model association manager is already associating, let it finish.
  // The model association done event will result in associating any remaining
  // association groups.
  if (model_association_manager_.state() !=
      ModelAssociationManager::INITIALIZED) {
    return;
  }

  ModelTypeSet types_to_associate;
  if (group == READY_AT_CONFIG) {
    association_types_queue_.front().ready_association_request_time =
        base::Time::Now();
    types_to_associate = association_types_queue_.front().ready_types;
  } else {
    DCHECK_EQ(UNREADY_AT_CONFIG, group);
    // Only start associating the rest of the types if they have all finished
    // downloading.
    if (association_types_queue_.front().download_ready_time.is_null())
      return;
    association_types_queue_.front().full_association_request_time =
        base::Time::Now();
    // We request the full set of types here for completeness sake. All types
    // within the READY_AT_CONFIG set will already be started and should be
    // no-ops.
    types_to_associate = association_types_queue_.front().types;
  }

  DVLOG(1) << "Associating " << ModelTypeSetToString(types_to_associate);
  model_association_manager_.StartAssociationAsync(types_to_associate);
}

void DataTypeManagerImpl::OnSingleDataTypeWillStart(ModelType type) {
  DCHECK(controllers_->find(type) != controllers_->end());
  DataTypeController* dtc = controllers_->find(type)->second.get();
  dtc->BeforeLoadModels(configurer_);
}

void DataTypeManagerImpl::OnSingleDataTypeWillStop(ModelType type,
                                                   const SyncError& error) {
  auto c_it = controllers_->find(type);
  DCHECK(c_it != controllers_->end());
  // Delegate deactivation to the controller.
  c_it->second->DeactivateDataType(configurer_);

  if (error.IsSet()) {
    data_type_status_table_.UpdateFailedDataType(type, error);

    // Unrecoverable errors will shut down the entire backend, so no need to
    // reconfigure.
    if (error.error_type() != SyncError::UNRECOVERABLE_ERROR) {
      needs_reconfigure_ = true;
      last_requested_context_.reason =
          GetReasonForProgrammaticReconfigure(last_requested_context_.reason);
      // Do this asynchronously so the ModelAssociationManager has a chance to
      // finish stopping this type, otherwise DeactivateDataType() and Stop()
      // end up getting called twice on the controller.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&DataTypeManagerImpl::ProcessReconfigure,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void DataTypeManagerImpl::OnSingleDataTypeAssociationDone(
    ModelType type,
    const DataTypeAssociationStats& association_stats) {
  DCHECK(!association_types_queue_.empty());
  auto c_it = controllers_->find(type);
  DCHECK(c_it != controllers_->end());
  if (c_it->second->state() == DataTypeController::RUNNING) {
    // Delegate activation to the controller.
    c_it->second->ActivateDataType(configurer_);
  }

  if (!debug_info_listener_.IsInitialized())
    return;

  AssociationTypesInfo& info = association_types_queue_.front();
  configuration_stats_.push_back(DataTypeConfigurationStats());
  configuration_stats_.back().model_type = type;
  configuration_stats_.back().association_stats = association_stats;
  if (info.types.Has(type)) {
    // Times in |info| only apply to non-slow types.
    configuration_stats_.back().download_wait_time =
        info.download_start_time - last_restart_time_;
    if (info.first_sync_types.Has(type)) {
      configuration_stats_.back().download_time =
          info.download_ready_time - info.download_start_time;
    }
    if (info.ready_types.Has(type)) {
      configuration_stats_.back().association_wait_time_for_high_priority =
          info.ready_association_request_time - info.download_start_time;
    } else {
      configuration_stats_.back().association_wait_time_for_high_priority =
          info.full_association_request_time - info.download_ready_time;
    }
    configuration_stats_.back().high_priority_types_configured_before =
        info.high_priority_types_before;
    configuration_stats_.back().same_priority_types_configured_before =
        info.configured_types;
    info.configured_types.Put(type);
  }
}

void DataTypeManagerImpl::OnModelAssociationDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK(state_ == STOPPING || state_ == CONFIGURING);

  if (state_ == STOPPING)
    return;

  // Ignore abort/unrecoverable error if we need to reconfigure anyways.
  if (needs_reconfigure_) {
    ProcessReconfigure();
    return;
  }

  if (result.status == ABORTED || result.status == UNRECOVERABLE_ERROR) {
    Abort(result.status);
    return;
  }

  DCHECK(result.status == OK);
  DCHECK(!association_types_queue_.empty());

  // If this model association was for the full set of types, then this priority
  // set is done. Otherwise it was just the ready types and the unready types
  // still need to be associated.
  if (result.requested_types == association_types_queue_.front().types) {
    association_types_queue_.pop();
    if (!association_types_queue_.empty()) {
      StartNextAssociation(READY_AT_CONFIG);
    } else if (download_types_queue_.empty()) {
      state_ = CONFIGURED;
      NotifyDone(result);
    }
  } else {
    DCHECK_EQ(association_types_queue_.front().ready_types,
              result.requested_types);
    // Will do nothing if the types are still downloading.
    StartNextAssociation(UNREADY_AT_CONFIG);
  }
}

void DataTypeManagerImpl::Stop(ShutdownReason reason) {
  if (state_ == STOPPED)
    return;

  bool need_to_notify = state_ == CONFIGURING;
  StopImpl(reason);

  if (need_to_notify) {
    ConfigureResult result(ABORTED, last_requested_types_);
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::Abort(ConfigureStatus status) {
  DCHECK_EQ(CONFIGURING, state_);

  StopImpl(STOP_SYNC);

  DCHECK_NE(OK, status);
  ConfigureResult result(status, last_requested_types_);
  NotifyDone(result);
}

void DataTypeManagerImpl::StopImpl(ShutdownReason reason) {
  state_ = STOPPING;

  // Invalidate weak pointer to drop download callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop all data types. This may trigger association callback but the
  // callback will do nothing because state is set to STOPPING above.
  model_association_manager_.Stop(reason);

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
        debug_info_listener_.Call(
            FROM_HERE, &DataTypeDebugInfoListener::OnDataTypeConfigureComplete,
            configuration_stats_);
      }
      configuration_stats_.clear();
      break;
    case DataTypeManager::ABORTED:
      DVLOG(1) << "NotifyDone called with result: ABORTED";
      base::UmaHistogramLongTimes(prefix_uma + ".ABORTED", configure_time);
      break;
    case DataTypeManager::UNRECOVERABLE_ERROR:
      DVLOG(1) << "NotifyDone called with result: UNRECOVERABLE_ERROR";
      base::UmaHistogramLongTimes(prefix_uma + ".UNRECOVERABLE_ERROR",
                                  configure_time);
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

bool DataTypeManagerImpl::IsNigoriEnabled() const {
  return downloaded_types_.Has(NIGORI);
}

DataTypeManager::State DataTypeManagerImpl::state() const {
  return state_;
}

ModelTypeSet DataTypeManagerImpl::GetEnabledTypes() const {
  return Difference(last_requested_types_,
                    data_type_status_table_.GetFailedTypes());
}

}  // namespace syncer
