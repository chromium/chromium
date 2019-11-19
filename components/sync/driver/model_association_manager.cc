// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_association_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

namespace {

static const ModelType kStartOrder[] = {
    NIGORI,  //  Listed for completeness.
    DEVICE_INFO,
    DEPRECATED_EXPERIMENTS,  //  Listed for completeness.
    PROXY_TABS,              //  Listed for completeness.

    // Kick off the association of the non-UI types first so they can associate
    // in parallel with the UI types.
    PASSWORDS, AUTOFILL, AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA,
    AUTOFILL_WALLET_METADATA, EXTENSION_SETTINGS, APP_SETTINGS, TYPED_URLS,
    HISTORY_DELETE_DIRECTIVES,

    // Chrome OS settings affect the initial desktop appearance before the
    // browser window opens, so start them before browser data types.
    OS_PRIORITY_PREFERENCES, OS_PREFERENCES,

    // UI thread data types.
    BOOKMARKS, PREFERENCES, PRIORITY_PREFERENCES, EXTENSIONS, APPS, APP_LIST,
    ARC_PACKAGE, READING_LIST, THEMES, SEARCH_ENGINES, SESSIONS, DICTIONARY,
    FAVICON_IMAGES, FAVICON_TRACKING, PRINTERS, USER_CONSENTS, USER_EVENTS,
    SUPERVISED_USER_SETTINGS, SUPERVISED_USER_WHITELISTS, SEND_TAB_TO_SELF,
    SECURITY_EVENTS, WEB_APPS, WIFI_CONFIGURATIONS};

static_assert(base::size(kStartOrder) ==
                  ModelType::NUM_ENTRIES - FIRST_REAL_MODEL_TYPE,
              "When adding a new type, update kStartOrder.");

// The amount of time we wait for association to finish. If some types haven't
// finished association by the time, DataTypeManager is notified of the
// unfinished types.
const int64_t kAssociationTimeOutInSeconds = 600;

DataTypeAssociationStats BuildAssociationStatsFromMergeResults(
    const SyncMergeResult& local_merge_result,
    const SyncMergeResult& syncer_merge_result,
    const base::TimeDelta& association_wait_time,
    const base::TimeDelta& association_time) {
  DCHECK_EQ(local_merge_result.model_type(), syncer_merge_result.model_type());
  DataTypeAssociationStats stats;
  stats.had_error =
      local_merge_result.error().IsSet() || syncer_merge_result.error().IsSet();
  stats.num_local_items_before_association =
      local_merge_result.num_items_before_association();
  stats.num_sync_items_before_association =
      syncer_merge_result.num_items_before_association();
  stats.num_local_items_after_association =
      local_merge_result.num_items_after_association();
  stats.num_sync_items_after_association =
      syncer_merge_result.num_items_after_association();
  stats.num_local_items_added = local_merge_result.num_items_added();
  stats.num_local_items_deleted = local_merge_result.num_items_deleted();
  stats.num_local_items_modified = local_merge_result.num_items_modified();
  stats.local_version_pre_association =
      local_merge_result.pre_association_version();
  stats.num_sync_items_added = syncer_merge_result.num_items_added();
  stats.num_sync_items_deleted = syncer_merge_result.num_items_deleted();
  stats.num_sync_items_modified = syncer_merge_result.num_items_modified();
  stats.sync_version_pre_association =
      syncer_merge_result.pre_association_version();
  stats.association_wait_time = association_wait_time;
  stats.association_time = association_time;
  return stats;
}

}  // namespace

ModelAssociationManager::ModelAssociationManager(
    const DataTypeController::TypeMap* controllers,
    ModelAssociationManagerDelegate* processor)
    : state_(IDLE),
      controllers_(controllers),
      delegate_(processor),
      configure_status_(DataTypeManager::UNKNOWN),
      notified_about_ready_for_configure_(false) {}

ModelAssociationManager::~ModelAssociationManager() {}

void ModelAssociationManager::Initialize(ModelTypeSet desired_types,
                                         ModelTypeSet preferred_types,
                                         const ConfigureContext& context) {
  // state_ can be INITIALIZED if types are reconfigured when
  // data is being downloaded, so StartAssociationAsync() is never called for
  // the first configuration.
  DCHECK_NE(ASSOCIATING, state_);

  // |desired_types| must be a subset of |preferred_types|.
  DCHECK(preferred_types.HasAll(desired_types));

  bool sync_mode_changed = configure_context_.sync_mode != context.sync_mode;

  configure_context_ = context;

  // Only keep types that have controllers.
  desired_types_.Clear();
  for (ModelType type : desired_types) {
    auto dtc_iter = controllers_->find(type);
    if (dtc_iter != controllers_->end()) {
      DataTypeController* dtc = dtc_iter->second.get();
      // Controllers in a FAILED state should have been filtered out by the
      // DataTypeManager.
      DCHECK_NE(dtc->state(), DataTypeController::FAILED);
      desired_types_.Put(type);
    }
  }

  DVLOG(1) << "ModelAssociationManager: Initializing for "
           << ModelTypeSetToString(desired_types_);

  state_ = INITIALIZED;
  notified_about_ready_for_configure_ = false;

  DVLOG(1) << "ModelAssociationManager: Stopping disabled types.";
  std::map<DataTypeController*, ShutdownReason> types_to_stop;
  for (const auto& type_and_dtc : *controllers_) {
    DataTypeController* dtc = type_and_dtc.second.get();
    // We generally stop all data types which are not desired. When the storage
    // option changes, we need to restart all data types so that they can
    // re-wire to the correct storage.
    bool should_stop = !desired_types_.Has(dtc->type()) || sync_mode_changed;
    // If the datatype is already STOPPING, we also wait for it to stop, to make
    // sure it's ready to start again (if appropriate).
    if ((should_stop && dtc->state() != DataTypeController::NOT_RUNNING) ||
        dtc->state() == DataTypeController::STOPPING) {
      // Note: STOP_SYNC means we'll keep the Sync data around; DISABLE_SYNC
      // means we'll clear it.
      ShutdownReason reason =
          preferred_types.Has(dtc->type()) ? STOP_SYNC : DISABLE_SYNC;
      // If we're switching to transport-only mode, don't clear any old data.
      // The reason is that if a user temporarily disables Sync, we don't want
      // to wipe (and later redownload) all their data, just because Sync
      // restarted in transport-only mode.
      if (sync_mode_changed &&
          configure_context_.sync_mode == SyncMode::kTransportOnly) {
        reason = STOP_SYNC;
      }
      types_to_stop[dtc] = reason;
    }
  }

  // Run LoadEnabledTypes() only after all relevant types are stopped.
  // TODO(mastiz): Add test coverage to this waiting logic, including the
  // case where the datatype is STOPPING when this function is called.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      types_to_stop.size(),
      base::BindOnce(&ModelAssociationManager::LoadEnabledTypes,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const auto& dtc_and_reason : types_to_stop) {
    DataTypeController* dtc = dtc_and_reason.first;
    const ShutdownReason reason = dtc_and_reason.second;
    DVLOG(1) << "ModelAssociationManager: stop " << dtc->name() << " due to "
             << ShutdownReasonToString(reason);
    StopDatatypeImpl(SyncError(), reason, dtc, barrier_closure);
  }
}

void ModelAssociationManager::StopDatatype(ModelType type,
                                           ShutdownReason shutdown_reason,
                                           SyncError error) {
  DCHECK(error.IsSet());
  desired_types_.Remove(type);

  DataTypeController* dtc = controllers_->find(type)->second.get();
  if (dtc->state() != DataTypeController::NOT_RUNNING &&
      dtc->state() != DataTypeController::STOPPING) {
    StopDatatypeImpl(error, shutdown_reason, dtc, base::DoNothing());
  }

  // Removing a desired type may mean all models are now loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelAssociationManager::StopDatatypeImpl(
    const SyncError& error,
    ShutdownReason shutdown_reason,
    DataTypeController* dtc,
    DataTypeController::StopCallback callback) {
  loaded_types_.Remove(dtc->type());
  associated_types_.Remove(dtc->type());
  associating_types_.Remove(dtc->type());

  DCHECK(error.IsSet() || (dtc->state() != DataTypeController::NOT_RUNNING));

  delegate_->OnSingleDataTypeWillStop(dtc->type(), error);

  // Note: Depending on |shutdown_reason|, USS types might clear their metadata
  // in response to Stop(). For directory types, the clearing happens in
  // SyncManager::PurgeDisabledTypes() instead.
  dtc->Stop(shutdown_reason, std::move(callback));
}

void ModelAssociationManager::LoadEnabledTypes() {
  for (ModelType type : desired_types_) {
    auto dtc_iter = controllers_->find(type);
    DCHECK(dtc_iter != controllers_->end());
    DataTypeController* dtc = dtc_iter->second.get();
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      DCHECK(!loaded_types_.Has(dtc->type()));
      DCHECK(!associated_types_.Has(dtc->type()));
      delegate_->OnSingleDataTypeWillStart(dtc->type());
    }
  }
  // Load in kStartOrder.
  for (size_t i = 0; i < base::size(kStartOrder); i++) {
    ModelType type = kStartOrder[i];
    if (!desired_types_.Has(type))
      continue;

    auto dtc_iter = controllers_->find(type);
    DCHECK(dtc_iter != controllers_->end());
    DataTypeController* dtc = dtc_iter->second.get();
    DCHECK_NE(DataTypeController::STOPPING, dtc->state());
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      DCHECK(!loaded_types_.Has(dtc->type()));
      DCHECK(!associated_types_.Has(dtc->type()));
      dtc->LoadModels(configure_context_,
                      base::Bind(&ModelAssociationManager::ModelLoadCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
    }
  }
  NotifyDelegateIfReadyForConfigure();
}

void ModelAssociationManager::StartAssociationAsync(
    const ModelTypeSet& types_to_associate) {
  DCHECK_EQ(INITIALIZED, state_);
  DVLOG(1) << "Starting association for "
           << ModelTypeSetToString(types_to_associate);
  state_ = ASSOCIATING;

  association_start_time_ = base::TimeTicks::Now();

  requested_types_ = types_to_associate;

  associating_types_ = types_to_associate;
  associating_types_.RetainAll(desired_types_);
  associating_types_.RemoveAll(associated_types_);

  // Assume success.
  configure_status_ = DataTypeManager::OK;

  // Done if no types to associate.
  if (associating_types_.Empty()) {
    ModelAssociationDone(INITIALIZED);
    return;
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAssociationTimeOutInSeconds),
               base::BindOnce(&ModelAssociationManager::ModelAssociationDone,
                              weak_ptr_factory_.GetWeakPtr(), INITIALIZED));

  // Start association of types that are loaded in specified order.
  for (size_t i = 0; i < base::size(kStartOrder); i++) {
    ModelType type = kStartOrder[i];
    if (!associating_types_.Has(type) || !loaded_types_.Has(type))
      continue;

    DataTypeController* dtc = controllers_->find(type)->second.get();
    DCHECK(DataTypeController::MODEL_LOADED == dtc->state() ||
           DataTypeController::ASSOCIATING == dtc->state());
    if (dtc->state() == DataTypeController::MODEL_LOADED) {
      TRACE_EVENT_ASYNC_BEGIN1("sync", "ModelAssociation", dtc, "DataType",
                               ModelTypeToString(type));

      dtc->StartAssociating(base::Bind(
          &ModelAssociationManager::TypeStartCallback,
          weak_ptr_factory_.GetWeakPtr(), type, base::TimeTicks::Now()));
    }
  }
}

void ModelAssociationManager::Stop(ShutdownReason shutdown_reason) {
  // Ignore callbacks from controllers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop started data types.
  for (const auto& type_and_dtc : *controllers_) {
    DataTypeController* dtc = type_and_dtc.second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      // We don't really wait until all datatypes have been fully stopped, which
      // is only required (and in fact waited for) when Initialize() is called.
      StopDatatypeImpl(SyncError(), shutdown_reason, dtc, base::DoNothing());
      DVLOG(1) << "ModelAssociationManager: Stopped " << dtc->name();
    }
  }

  desired_types_.Clear();
  loaded_types_.Clear();
  associated_types_.Clear();

  if (state_ == ASSOCIATING) {
    if (configure_status_ == DataTypeManager::OK)
      configure_status_ = DataTypeManager::ABORTED;
    DVLOG(1) << "ModelAssociationManager: Calling OnModelAssociationDone";
    ModelAssociationDone(IDLE);
  } else {
    DCHECK(associating_types_.Empty());
    DCHECK(requested_types_.Empty());
    state_ = IDLE;
  }
}

void ModelAssociationManager::ModelLoadCallback(ModelType type,
                                                const SyncError& error) {
  DVLOG(1) << "ModelAssociationManager: ModelLoadCallback for "
           << ModelTypeToString(type);

  if (error.IsSet()) {
    SyncMergeResult local_merge_result(type);
    local_merge_result.set_error(error);
    TypeStartCallback(type, base::TimeTicks::Now(),
                      DataTypeController::ASSOCIATION_FAILED,
                      local_merge_result, SyncMergeResult(type));
    return;
  }

  // This happens when slow loading type is disabled by new configuration or
  // the model came unready during loading.
  if (!desired_types_.Has(type))
    return;

  DCHECK(!loaded_types_.Has(type));
  loaded_types_.Put(type);
  NotifyDelegateIfReadyForConfigure();
  if (associating_types_.Has(type)) {
    DataTypeController* dtc = controllers_->find(type)->second.get();
    // If initial sync was done for this datatype then
    // NotifyDelegateIfReadyForConfigure possibly already triggered model
    // association and StartAssociating was already called for this type. To
    // ensure StartAssociating is called only once only make a call if state is
    // MODEL_LOADED.
    // TODO(pavely): Add test for this scenario in DataTypeManagerImpl
    // unittests.
    if (dtc->state() == DataTypeController::MODEL_LOADED) {
      dtc->StartAssociating(base::Bind(
          &ModelAssociationManager::TypeStartCallback,
          weak_ptr_factory_.GetWeakPtr(), type, base::TimeTicks::Now()));
    }
  }
}

void ModelAssociationManager::TypeStartCallback(
    ModelType type,
    base::TimeTicks type_start_time,
    DataTypeController::ConfigureResult start_result,
    const SyncMergeResult& local_merge_result,
    const SyncMergeResult& syncer_merge_result) {
  if (desired_types_.Has(type) &&
      !DataTypeController::IsSuccessfulResult(start_result)) {
    DVLOG(1) << "ModelAssociationManager: Type encountered an error.";
    desired_types_.Remove(type);
    DataTypeController* dtc = controllers_->find(type)->second.get();
    StopDatatypeImpl(local_merge_result.error(), STOP_SYNC, dtc,
                     base::DoNothing());
    NotifyDelegateIfReadyForConfigure();

    // Update configuration result.
    if (start_result == DataTypeController::UNRECOVERABLE_ERROR)
      configure_status_ = DataTypeManager::UNRECOVERABLE_ERROR;
  }

  // This happens when a slow associating type is disabled or if a type
  // disables itself after initial configuration.
  if (!desired_types_.Has(type)) {
    // It's possible all types failed to associate, in which case association
    // is complete.
    if (state_ == ASSOCIATING && associating_types_.Empty())
      ModelAssociationDone(INITIALIZED);
    return;
  }

  DCHECK(!associated_types_.Has(type));
  DCHECK(DataTypeController::IsSuccessfulResult(start_result));
  associated_types_.Put(type);

  if (state_ != ASSOCIATING)
    return;

  TRACE_EVENT_ASYNC_END1("sync", "ModelAssociation",
                         controllers_->find(type)->second.get(), "DataType",
                         ModelTypeToString(type));

  // Track the merge results if we succeeded or an association failure
  // occurred.
  if (ProtocolTypes().Has(type)) {
    base::TimeDelta association_wait_time =
        std::max(base::TimeDelta(), type_start_time - association_start_time_);
    base::TimeDelta association_time = base::TimeTicks::Now() - type_start_time;
    DataTypeAssociationStats stats = BuildAssociationStatsFromMergeResults(
        local_merge_result, syncer_merge_result, association_wait_time,
        association_time);
    delegate_->OnSingleDataTypeAssociationDone(type, stats);
  }

  associating_types_.Remove(type);

  if (associating_types_.Empty())
    ModelAssociationDone(INITIALIZED);
}

void ModelAssociationManager::ModelAssociationDone(State new_state) {
  DCHECK_NE(IDLE, state_);

  if (state_ == INITIALIZED) {
    // No associations are currently happening. Just reset the state.
    state_ = new_state;
    return;
  }

  DVLOG(1) << "Model association complete for "
           << ModelTypeSetToString(requested_types_);

  timer_.Stop();

  // Treat any unfinished types as having errors.
  desired_types_.RemoveAll(associating_types_);
  for (const auto& type_and_dtc : *controllers_) {
    DataTypeController* dtc = type_and_dtc.second.get();
    if (associating_types_.Has(dtc->type()) &&
        dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
      UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                                ModelTypeHistogramValue(dtc->type()));
      StopDatatypeImpl(SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                 "Association timed out.", dtc->type()),
                       STOP_SYNC, dtc, base::DoNothing());
    }
  }

  DataTypeManager::ConfigureResult result(configure_status_, requested_types_);

  // Need to reset state before invoking delegate in order to avoid re-entrancy
  // issues (delegate may trigger a reconfiguration).
  associating_types_.Clear();
  requested_types_.Clear();
  state_ = new_state;

  delegate_->OnModelAssociationDone(result);
}

base::OneShotTimer* ModelAssociationManager::GetTimerForTesting() {
  return &timer_;
}

void ModelAssociationManager::NotifyDelegateIfReadyForConfigure() {
  if (notified_about_ready_for_configure_)
    return;
  for (const auto& type_dtc_pair : *controllers_) {
    ModelType type = type_dtc_pair.first;
    if (!desired_types_.Has(type))
      continue;
    DataTypeController* dtc = type_dtc_pair.second.get();
    if (dtc->ShouldLoadModelBeforeConfigure() && !loaded_types_.Has(type)) {
      // At least one type is not ready.
      return;
    }
  }

  notified_about_ready_for_configure_ = true;
  delegate_->OnAllDataTypesReadyForConfigure();
}

}  // namespace syncer
