// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_load_manager.h"

#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

ModelLoadManager::ModelLoadManager(
    const DataTypeController::TypeMap* controllers,
    ModelLoadManagerDelegate* processor)
    : controllers_(controllers), delegate_(processor) {}

ModelLoadManager::~ModelLoadManager() = default;

void ModelLoadManager::Configure(ModelTypeSet preferred_types_without_errors,
                                 ModelTypeSet preferred_types,
                                 const ConfigureContext& context) {
  // |preferred_types_without_errors| must be a subset of |preferred_types|.
  DCHECK(preferred_types.HasAll(preferred_types_without_errors))
      << " desired: "
      << ModelTypeSetToDebugString(preferred_types_without_errors)
      << ", preferred: " << ModelTypeSetToDebugString(preferred_types);

  const bool sync_mode_changed =
      configure_context_.has_value() &&
      configure_context_->sync_mode != context.sync_mode;

  configure_context_ = context;

  // Only keep types that have controllers.
  preferred_types_without_errors_.Clear();
  for (ModelType type : preferred_types_without_errors) {
    auto dtc_iter = controllers_->find(type);
    if (dtc_iter != controllers_->end()) {
      const DataTypeController* dtc = dtc_iter->second.get();
      // Controllers in a FAILED state should have been filtered out by the
      // DataTypeManager.
      DCHECK_NE(dtc->state(), DataTypeController::FAILED);
      preferred_types_without_errors_.Put(type);
    }
  }

  DVLOG(1) << "ModelLoadManager: Initializing for "
           << ModelTypeSetToDebugString(preferred_types_without_errors_);

  notified_about_ready_for_configure_ = false;

  if (sync_mode_changed) {
    // When the sync mode changes (between full-sync and transport mode),
    // restart all data types so that they can re-wire to the correct storage.
    DVLOG(1) << "ModelLoadManager: Stopping all types because the sync mode "
                "changed.";

    for (const auto& [type, dtc] : *controllers_) {
      // Use CLEAR_METADATA in this case to avoid that two independent model
      // instances maintain their own copy of sync metadata.
      if (dtc->state() != DataTypeController::NOT_RUNNING ||
          base::FeatureList::IsEnabled(
              kSyncAllowClearingMetadataWhenDataTypeIsStopped)) {
        StopDatatypeImpl(SyncError(), SyncStopMetadataFate::CLEAR_METADATA,
                         dtc.get(), base::DoNothing());
      }
    }
  } else {
    // If the sync mode hasn't changed, stop only the types that are not
    // preferred anymore.
    DVLOG(1) << "ModelLoadManager: Stopping disabled types.";
    for (const auto& [type, dtc] : *controllers_) {
      if (!preferred_types_without_errors_.Has(dtc->type()) &&
          dtc->state() != DataTypeController::NOT_RUNNING) {
        SyncStopMetadataFate metadata_fate =
            preferred_types.Has(dtc->type())
                ? SyncStopMetadataFate::KEEP_METADATA
                : SyncStopMetadataFate::CLEAR_METADATA;
        DVLOG(1) << "ModelLoadManager: stop " << dtc->name()
                 << " with metadata fate " << static_cast<int>(metadata_fate);
        StopDatatypeImpl(SyncError(), metadata_fate, dtc.get(),
                         base::DoNothing());
      }
    }
  }

  // Note: At this point, some types may still be in the STOPPING state, i.e.
  // they cannot be loaded right now. LoadDesiredTypes() takes care to wait for
  // the desired types to finish stopping before starting them again. And for
  // undesired types, it doesn't matter in what state they are.
  LoadDesiredTypes();
}

void ModelLoadManager::StopDatatype(ModelType type,
                                    SyncStopMetadataFate metadata_fate,
                                    SyncError error) {
  DCHECK(error.IsSet());
  preferred_types_without_errors_.Remove(type);

  DataTypeController* dtc = controllers_->find(type)->second.get();
  // If the feature flag is enabled, call stop on data types even if they are
  // already stopped since we may still want to clear the metadata.
  if (base::FeatureList::IsEnabled(
          kSyncAllowClearingMetadataWhenDataTypeIsStopped) ||
      (dtc->state() != DataTypeController::NOT_RUNNING &&
       dtc->state() != DataTypeController::STOPPING)) {
    StopDatatypeImpl(error, metadata_fate, dtc, base::DoNothing());
  }

  // Removing a desired type may mean all models are now loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::StopDatatypeImpl(
    const SyncError& error,
    SyncStopMetadataFate metadata_fate,
    DataTypeController* dtc,
    DataTypeController::StopCallback callback) {
  loaded_types_.Remove(dtc->type());

  DCHECK(base::FeatureList::IsEnabled(
             syncer::kSyncAllowClearingMetadataWhenDataTypeIsStopped) ||
         error.IsSet() || (dtc->state() != DataTypeController::NOT_RUNNING));

  delegate_->OnSingleDataTypeWillStop(dtc->type(), error);

  // Note: Depending on |metadata_fate|, data types will clear their metadata
  // in response to Stop().
  dtc->Stop(metadata_fate, std::move(callback));
}

void ModelLoadManager::LoadDesiredTypes() {
  // Note: |preferred_types_without_errors_| might be modified during iteration
  // (e.g. in ModelLoadCallback()), so make a copy.
  const ModelTypeSet types = preferred_types_without_errors_;

  // Start timer to measure time for loading to complete.
  load_models_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();

  for (ModelType type : types) {
    auto dtc_iter = controllers_->find(type);
    DCHECK(dtc_iter != controllers_->end());
    DataTypeController* dtc = dtc_iter->second.get();
    auto model_load_callback = base::BindRepeating(
        &ModelLoadManager::ModelLoadCallback, weak_ptr_factory_.GetWeakPtr());
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      DCHECK(!loaded_types_.Has(dtc->type()));
      dtc->LoadModels(*configure_context_, std::move(model_load_callback));
    } else if (dtc->state() == DataTypeController::STOPPING) {
      // If the datatype is already STOPPING, we wait for it to stop before
      // starting it up again.
      auto stop_callback =
          base::BindRepeating(&DataTypeController::LoadModels,
                              // This should be safe since the stop callback is
                              // called from the DataTypeController.
                              base::Unretained(dtc), *configure_context_,
                              std::move(model_load_callback));
      DCHECK(!loaded_types_.Has(dtc->type()));
      dtc->Stop(SyncStopMetadataFate::KEEP_METADATA, std::move(stop_callback));
    }
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncEnableLoadModelsTimeout)) {
    // Start a timeout timer for load.
    load_models_timeout_timer_.Start(FROM_HERE,
                                     kSyncLoadModelsTimeoutDuration.Get(), this,
                                     &ModelLoadManager::OnLoadModelsTimeout);
  }
  // It's possible that all models are already loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::Stop(SyncStopMetadataFate metadata_fate) {
  // Ignore callbacks from controllers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop all data types. Note that if the feature flag is enabled, we are also
  // calling stop on data types that are already stopped since we may still want
  // to clear the metadata.
  for (const auto& [type, dtc] : *controllers_) {
    if (base::FeatureList::IsEnabled(
            kSyncAllowClearingMetadataWhenDataTypeIsStopped) ||
        (dtc->state() != DataTypeController::NOT_RUNNING &&
         dtc->state() != DataTypeController::STOPPING)) {
      // We don't really wait until all datatypes have been fully stopped, which
      // is only required (and in fact waited for) when Configure() is called.
      StopDatatypeImpl(SyncError(), metadata_fate, dtc.get(),
                       base::DoNothing());
      DVLOG(1) << "ModelLoadManager: Stopped " << dtc->name();
    }
  }

  preferred_types_without_errors_.Clear();
  loaded_types_.Clear();
}

void ModelLoadManager::ModelLoadCallback(ModelType type,
                                         const SyncError& error) {
  DVLOG(1) << "ModelLoadManager: ModelLoadCallback for "
           << ModelTypeToDebugString(type);

  if (error.IsSet()) {
    DVLOG(1) << "ModelLoadManager: Type encountered an error.";
    preferred_types_without_errors_.Remove(type);
    DataTypeController* dtc = controllers_->find(type)->second.get();
    StopDatatypeImpl(error, SyncStopMetadataFate::KEEP_METADATA, dtc,
                     base::DoNothing());
    NotifyDelegateIfReadyForConfigure();
    return;
  }

  // This happens when slow loading type is disabled by new configuration or
  // the model came unready during loading.
  if (!preferred_types_without_errors_.Has(type)) {
    return;
  }

  DCHECK(!loaded_types_.Has(type));
  loaded_types_.Put(type);
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::NotifyDelegateIfReadyForConfigure() {
  if (notified_about_ready_for_configure_)
    return;

  if (!loaded_types_.HasAll(preferred_types_without_errors_)) {
    // At least one type is not ready.
    return;
  }

  // It may be possible that `load_models_elapsed_timer_` was never set, e.g.
  // if StopDatatype() was called before Configure().
  if (load_models_elapsed_timer_) {
    base::UmaHistogramMediumTimes("Sync.ModelLoadManager.LoadModelsElapsedTime",
                                  load_models_elapsed_timer_->Elapsed());
    // Needs to be measured only when NotifyDelegateIfReadyForConfigure() is
    // called for the first time after all types have been loaded.
    load_models_elapsed_timer_.reset();
  }

  // Cancel the timer since all the desired types are now loaded.
  load_models_timeout_timer_.Stop();

  notified_about_ready_for_configure_ = true;
  delegate_->OnAllDataTypesReadyForConfigure();
}

void ModelLoadManager::OnLoadModelsTimeout() {
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncEnableLoadModelsTimeout));
  // TODO(crbug.com/1420553): Investigate why the following DCHECK fails.
  // DCHECK(!loaded_types_.HasAll(preferred_types_without_errors_));

  const ModelTypeSet types = preferred_types_without_errors_;
  for (ModelType type : types) {
    if (!loaded_types_.Has(type)) {
      base::UmaHistogramEnumeration("Sync.ModelLoadManager.LoadModelsTimeout",
                                    ModelTypeHistogramValue(type));
      // All the types which have not loaded yet are removed from
      // `preferred_types_without_errors_`. This will cause ModelLoadCallback()
      // to stop these types when they finish loading. The intention here is to
      // not wait for these types and continue with connecting the loaded data
      // types, while also ensuring the DataTypeManager does not think the
      // datatype is stopped before the controller actually comes to a stopped
      // state.
      preferred_types_without_errors_.Remove(type);
    }
  }
  // Stop waiting for the data types to load and go ahead with connecting the
  // loaded types.
  NotifyDelegateIfReadyForConfigure();
}

}  // namespace syncer
