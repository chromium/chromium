// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/model_load_manager.h"

#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/timer/elapsed_timer.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_error.h"

namespace syncer {

namespace {

bool ModelIsLoadedOrFailed(const DataTypeController& mtc) {
  switch (mtc.state()) {
    case DataTypeController::NOT_RUNNING:
    case DataTypeController::MODEL_STARTING:
    case DataTypeController::STOPPING:
      return false;
    case DataTypeController::MODEL_LOADED:
    case DataTypeController::RUNNING:
    case DataTypeController::FAILED:
      return true;
  }
}

}  // namespace

const base::TimeDelta kSyncLoadModelsTimeoutDuration = base::Seconds(30);

ModelLoadManager::ModelLoadManager(
    const DataTypeController::TypeMap* controllers,
    ModelLoadManagerDelegate* processor)
    : controllers_(controllers), delegate_(processor) {}

ModelLoadManager::~ModelLoadManager() = default;

void ModelLoadManager::Configure(DataTypeSet preferred_types_without_errors,
                                 DataTypeSet preferred_types,
                                 const ConfigureContext& context) {
  // |preferred_types_without_errors| must be a subset of |preferred_types|.
  DCHECK(preferred_types.HasAll(preferred_types_without_errors))
      << " desired: "
      << DataTypeSetToDebugString(preferred_types_without_errors)
      << ", preferred: " << DataTypeSetToDebugString(preferred_types);

  const bool sync_mode_changed =
      configure_context_.has_value() &&
      configure_context_->sync_mode != context.sync_mode;

  configure_context_ = context;

  // Only keep types that have controllers.
  preferred_types_without_errors_.Clear();
  for (DataType type : preferred_types_without_errors) {
    auto dtc_iter = controllers_->find(type);
    if (dtc_iter != controllers_->end()) {
      const DataTypeController* dtc = dtc_iter->second.get();
      // Controllers in a FAILED state or with preconditions not met should have
      // been filtered out by the DataTypeManager.
      CHECK_NE(dtc->state(), DataTypeController::FAILED);
      preferred_types_without_errors_.Put(type);
    }
  }

  DVLOG(1) << "ModelLoadManager: Initializing for "
           << DataTypeSetToDebugString(preferred_types_without_errors_);

  delegate_waiting_for_ready_for_configure_ = true;

  if (sync_mode_changed) {
    // When the sync mode changes (between full-sync and transport mode),
    // restart all data types so that they can re-wire to the correct storage.
    DVLOG(1) << "ModelLoadManager: Stopping all types because the sync mode "
                "changed.";

    for (const auto& [type, dtc] : *controllers_) {
      // Use CLEAR_METADATA in this case to avoid that two independent model
      // instances maintain their own copy of sync metadata.
      StopDatatypeImpl(/*error=*/std::nullopt,
                       SyncStopMetadataFate::CLEAR_METADATA, dtc.get(),
                       base::DoNothing());
    }
  } else {
    // If the sync mode hasn't changed, stop only the types that are not
    // preferred anymore.
    DVLOG(1) << "ModelLoadManager: Stopping disabled types.";
    for (const auto& [type, dtc] : *controllers_) {
      if (!preferred_types_without_errors_.Has(dtc->type())) {
        // Call Stop() even on types not running to allow clearing metadata.
        // This is useful to clear metadata for types which were disabled during
        // configuration. Also clear metadata depending on the precondition
        // state.
        SyncStopMetadataFate metadata_fate =
            SyncStopMetadataFate::KEEP_METADATA;
        if (!preferred_types.Has(dtc->type()) ||
            dtc->GetPreconditionState() ==
                DataTypeController::PreconditionState::kMustStopAndClearData) {
          metadata_fate = SyncStopMetadataFate::CLEAR_METADATA;
        }
        DVLOG(1) << "ModelLoadManager: stop " << dtc->name()
                 << " with metadata fate " << static_cast<int>(metadata_fate);
        StopDatatypeImpl(/*error=*/std::nullopt, metadata_fate, dtc.get(),
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

void ModelLoadManager::StopDatatype(DataType type,
                                    SyncStopMetadataFate metadata_fate,
                                    SyncError error) {
  preferred_types_without_errors_.Remove(type);

  DataTypeController* dtc = controllers_->find(type)->second.get();
  // Call stop on data types even if they are
  // already stopped since we may still want to clear the metadata.
  StopDatatypeImpl(error, metadata_fate, dtc, base::DoNothing());

  // Removing a desired type may mean all models are now loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::StopDatatypeImpl(
    const std::optional<SyncError>& error,
    SyncStopMetadataFate metadata_fate,
    DataTypeController* dtc,
    DataTypeController::StopCallback callback) {
  const DataType data_type = dtc->type();

  // Avoid that the local variable is optimized away, motivated by
  // crbug.com/1456872.
  base::debug::Alias(&data_type);

  delegate_->OnSingleDataTypeWillStop(data_type, error);

  // Note: Depending on |metadata_fate|, data types will clear their metadata
  // in response to Stop().
  dtc->Stop(metadata_fate, std::move(callback));
}

void ModelLoadManager::LoadDesiredTypes() {
  // Note: |preferred_types_without_errors_| might be modified during iteration
  // (e.g. in ModelLoadCallback()), so make a copy.
  const DataTypeSet types = preferred_types_without_errors_;

  // Start timer to measure time for loading to complete.
  load_models_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();

  for (DataType type : types) {
    auto dtc_iter = controllers_->find(type);
    CHECK(dtc_iter != controllers_->end(), base::NotFatalUntil::M130);
    DataTypeController* dtc = dtc_iter->second.get();
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      LoadModelsForType(dtc);
    } else if (dtc->state() == DataTypeController::STOPPING) {
      // If the datatype is already STOPPING, we wait for it to stop before
      // starting it up again.
      auto stop_callback =
          base::BindRepeating(&ModelLoadManager::LoadModelsForType,
                              weak_ptr_factory_.GetWeakPtr(), dtc);
      dtc->Stop(SyncStopMetadataFate::KEEP_METADATA, std::move(stop_callback));
    }
  }

  // Start a timeout timer for load.
  load_models_timeout_timer_.Start(FROM_HERE, kSyncLoadModelsTimeoutDuration,
                                   this,
                                   &ModelLoadManager::OnLoadModelsTimeout);

  // It's possible that all models are already loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::Stop(SyncStopMetadataFate metadata_fate) {
  // Ignore callbacks from controllers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop all data types. Note that stop is also called on data types that are
  // already stopped to allow clearing the metadata.
  for (const auto& [type, dtc] : *controllers_) {
    // We don't really wait until all datatypes have been fully stopped, which
    // is only required (and in fact waited for) when Configure() is called.
    StopDatatypeImpl(/*error=*/std::nullopt, metadata_fate, dtc.get(),
                     base::DoNothing());
    DVLOG(1) << "ModelLoadManager: Stopped " << dtc->name();
  }

  load_models_timeout_timer_.Stop();
  delegate_waiting_for_ready_for_configure_ = false;

  preferred_types_without_errors_.Clear();
}

void ModelLoadManager::ModelLoadCallback(
    DataType type,
    const std::optional<ModelError>& error) {
  DVLOG(1) << "ModelLoadManager: ModelLoadCallback for "
           << DataTypeToDebugString(type);

  if (error.has_value()) {
    DVLOG(1) << "ModelLoadManager: Type encountered an error.";
    preferred_types_without_errors_.Remove(type);
    DataTypeController* dtc = controllers_->find(type)->second.get();
    StopDatatypeImpl(
        SyncError(error->location(), SyncError::MODEL_ERROR, error->message()),
        SyncStopMetadataFate::KEEP_METADATA, dtc, base::DoNothing());
    NotifyDelegateIfReadyForConfigure();
    return;
  }

  // This happens when slow loading type is disabled by new configuration or
  // the model came unready during loading.
  if (!preferred_types_without_errors_.Has(type)) {
    return;
  }

  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::NotifyDelegateIfReadyForConfigure() {
  if (!delegate_waiting_for_ready_for_configure_) {
    return;
  }

  // Check (and early-return) if any type is not ready.
  for (DataType type : preferred_types_without_errors_) {
    if (!ModelIsLoadedOrFailed(*controllers_->find(type)->second)) {
      return;
    }
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

  delegate_waiting_for_ready_for_configure_ = false;
  delegate_->OnAllDataTypesReadyForConfigure();
}

void ModelLoadManager::OnLoadModelsTimeout() {
  const DataTypeSet types = preferred_types_without_errors_;
  for (DataType type : types) {
    if (!ModelIsLoadedOrFailed(*controllers_->find(type)->second)) {
      base::UmaHistogramEnumeration("Sync.ModelLoadManager.LoadModelsTimeout",
                                    DataTypeHistogramValue(type));
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

void ModelLoadManager::LoadModelsForType(DataTypeController* dtc) {
  // FAILED is possible if the type was STOPPING but then encountered an error
  // before the type actually stopped.
  if (dtc->state() == DataTypeController::FAILED) {
    ModelLoadCallback(dtc->type(),
                      ModelError(FROM_HERE, "Data type in FAILED state."));
    return;
  }

  // TODO(crbug.com/41492467): Avoid calling LoadModelsForType() multiple times
  // upon stop, and re-introduce a CHECK for state to be NOT_RUNNING only.
  if (dtc->state() == DataTypeController::NOT_RUNNING) {
    dtc->LoadModels(
        *configure_context_,
        base::BindRepeating(&ModelLoadManager::ModelLoadCallback,
                            weak_ptr_factory_.GetWeakPtr(), dtc->type()));
  }
}

}  // namespace syncer
