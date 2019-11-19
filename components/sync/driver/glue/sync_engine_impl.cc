// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/glue/sync_engine_backend.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/sync_backend_registrar.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/syncable/base_transaction.h"

namespace syncer {

SyncEngineImpl::SyncEngineImpl(const std::string& name,
                               invalidation::InvalidationService* invalidator,
                               const base::WeakPtr<SyncPrefs>& sync_prefs,
                               const base::FilePath& sync_data_folder)
    : name_(name), sync_prefs_(sync_prefs), invalidator_(invalidator) {
  backend_ = base::MakeRefCounted<SyncEngineBackend>(
      name_, sync_data_folder, weak_ptr_factory_.GetWeakPtr());
}

SyncEngineImpl::~SyncEngineImpl() {
  DCHECK(!backend_ && !host_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_);
}

void SyncEngineImpl::Initialize(InitParams params) {
  DCHECK(params.sync_task_runner);
  DCHECK(params.host);
  DCHECK(params.registrar);

  sync_task_runner_ = params.sync_task_runner;
  host_ = params.host;
  registrar_ = params.registrar.get();

  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoInitialize, backend_,
                                std::move(params)));
}

bool SyncEngineImpl::IsInitialized() const {
  return initialized_;
}

void SyncEngineImpl::TriggerRefresh(const ModelTypeSet& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoRefreshTypes, backend_, types));
}

void SyncEngineImpl::UpdateCredentials(const SyncCredentials& credentials) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoUpdateCredentials,
                                backend_, credentials));
}

void SyncEngineImpl::InvalidateCredentials() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoInvalidateCredentials, backend_));
}

void SyncEngineImpl::StartConfiguration() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoStartConfiguration, backend_));
}

void SyncEngineImpl::StartSyncingWithServer() {
  DVLOG(1) << name_ << ": SyncEngineImpl::StartSyncingWithServer called.";
  base::Time last_poll_time = sync_prefs_->GetLastPollTime();
  // If there's no known last poll time (e.g. on initial start-up), we treat
  // this as if a poll just happened.
  if (last_poll_time.is_null()) {
    last_poll_time = base::Time::Now();
    sync_prefs_->SetLastPollTime(last_poll_time);
  }
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoStartSyncing, backend_,
                                last_poll_time));
}

void SyncEngineImpl::SetEncryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoSetEncryptionPassphrase,
                                backend_, passphrase));
}

void SyncEngineImpl::SetDecryptionPassphrase(const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoSetDecryptionPassphrase,
                                backend_, passphrase));
}

void SyncEngineImpl::AddTrustedVaultDecryptionKeys(
    const std::vector<std::string>& keys,
    base::OnceClosure done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoAddTrustedVaultDecryptionKeys,
                     backend_, keys),
      std::move(done_cb));
}

void SyncEngineImpl::StopSyncingForShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop getting messages from the sync thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Immediately stop sending messages to the host.
  host_ = nullptr;

  registrar_->RequestWorkerStopOnUIThread();

  backend_->ShutdownOnUIThread();
}

void SyncEngineImpl::Shutdown(ShutdownReason reason) {
  // StopSyncingForShutdown() (which nulls out |host_|) should be
  // called first.
  DCHECK(!host_);

  if (invalidation_handler_registered_) {
    if (reason != BROWSER_SHUTDOWN) {
      bool success =
          invalidator_->UpdateRegisteredInvalidationIds(this, ObjectIdSet());
      DCHECK(success);
    }
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = nullptr;
  }
  last_enabled_types_.Clear();
  invalidation_handler_registered_ = false;

  model_type_connector_.reset();

  // Shut down and destroy SyncManager. SyncManager holds a pointer to
  // |registrar_| so its destruction must be sequenced before the destruction of
  // |registrar_|.
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoShutdown, backend_, reason));

  // Ensure that |backend_| destroyed inside Sync sequence, not inside current
  // one.
  sync_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));
  DCHECK(!backend_);
  registrar_ = nullptr;
}

void SyncEngineImpl::ConfigureDataTypes(ConfigureParams params) {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoPurgeDisabledTypes, backend_,
                     params.to_purge, params.to_journal, params.to_unapply));
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoConfigureSyncer, backend_,
                                std::move(params)));
}

void SyncEngineImpl::RegisterDirectoryDataType(ModelType type,
                                               ModelSafeGroup group) {
  model_type_connector_->RegisterDirectoryType(type, group);
}

void SyncEngineImpl::UnregisterDirectoryDataType(ModelType type) {
  model_type_connector_->UnregisterDirectoryType(type);
}

void SyncEngineImpl::EnableEncryptEverything() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoEnableEncryptEverything, backend_));
}

void SyncEngineImpl::ActivateDirectoryDataType(
    ModelType type,
    ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncEngineImpl::DeactivateDirectoryDataType(ModelType type) {
  registrar_->DeactivateDataType(type);
}

void SyncEngineImpl::ActivateNonBlockingDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  registrar_->RegisterNonBlockingType(type);
  if (activation_response->model_type_state.initial_sync_done())
    registrar_->AddRestoredNonBlockingType(type);
  model_type_connector_->ConnectNonBlockingType(type,
                                                std::move(activation_response));
}

void SyncEngineImpl::DeactivateNonBlockingDataType(ModelType type) {
  model_type_connector_->DisconnectNonBlockingType(type);
}

UserShare* SyncEngineImpl::GetUserShare() const {
  return backend_->sync_manager()->GetUserShare();
}

SyncEngineImpl::Status SyncEngineImpl::GetDetailedStatus() {
  DCHECK(IsInitialized());
  return backend_->sync_manager()->GetDetailedStatus();
}

void SyncEngineImpl::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {
  DCHECK(IsInitialized());
  base::PostTaskAndReplyWithResult(
      sync_task_runner_.get(), FROM_HERE,
      base::BindOnce(&SyncEngineBackend::HasUnsyncedItemsForTest, backend_),
      std::move(cb));
}

void SyncEngineImpl::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const {
  if (IsInitialized()) {
    registrar_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

void SyncEngineImpl::FlushDirectory() const {
  DCHECK(IsInitialized());
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::SaveChanges, backend_));
}

void SyncEngineImpl::RequestBufferedProtocolEventsAndEnableForwarding() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncEngineBackend::SendBufferedProtocolEventsAndEnableForwarding,
          backend_));
}

void SyncEngineImpl::DisableProtocolEventForwarding() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DisableProtocolEventForwarding,
                     backend_));
}

void SyncEngineImpl::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(IsInitialized());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::EnableDirectoryTypeDebugInfoForwarding,
                     backend_));
}

void SyncEngineImpl::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(IsInitialized());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncEngineBackend::DisableDirectoryTypeDebugInfoForwarding,
          backend_));
}

void SyncEngineImpl::FinishConfigureDataTypesOnFrontendLoop(
    const ModelTypeSet enabled_types,
    const ModelTypeSet succeeded_configuration_types,
    const ModelTypeSet failed_configuration_types,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task) {
  last_enabled_types_ = enabled_types;
  if (invalidator_) {
    ModelTypeSet invalidation_enabled_types(enabled_types);
#if defined(OS_ANDROID)
    if (!sessions_invalidation_enabled_) {
      invalidation_enabled_types.Remove(syncer::SESSIONS);
      invalidation_enabled_types.Remove(syncer::FAVICON_IMAGES);
      invalidation_enabled_types.Remove(syncer::FAVICON_TRACKING);
    }
#endif
    bool success = invalidator_->UpdateRegisteredInvalidationIds(
        this, ModelTypeSetToObjectIdSet(invalidation_enabled_types));
    DCHECK(success);
  }

  if (!ready_task.is_null())
    ready_task.Run(succeeded_configuration_types, failed_configuration_types);
}

void SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop(
    ModelTypeSet initial_types,
    const WeakHandle<JsBackend> js_backend,
    const WeakHandle<DataTypeDebugInfoListener> debug_info_listener,
    std::unique_ptr<ModelTypeConnector> model_type_connector,
    const std::string& cache_guid,
    const std::string& birthday,
    const std::string& bag_of_chips,
    const std::string& last_keystore_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  model_type_connector_ = std::move(model_type_connector);

  initialized_ = true;

  if (invalidator_) {
    invalidator_->RegisterInvalidationHandler(this);
    invalidation_handler_registered_ = true;

    // Fake a state change to initialize the SyncManager's cached invalidator
    // state.
    OnInvalidatorStateChange(invalidator_->GetInvalidatorState());
  }

  host_->OnEngineInitialized(initial_types, js_backend, debug_info_listener,
                             cache_guid, birthday, bag_of_chips,
                             last_keystore_key, /*success=*/true);
}

void SyncEngineImpl::HandleInitializationFailureOnFrontendLoop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                             WeakHandle<DataTypeDebugInfoListener>(),
                             /*cache_guid=*/"",
                             /*birthday=*/"", /*bag_of_chips=*/"",
                             /*last_keystore_key=*/"",
                             /*success=*/false);
}

void SyncEngineImpl::HandleSyncCycleCompletedOnFrontendLoop(
    const SyncCycleSnapshot& snapshot,
    const std::string& last_keystore_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (IsInitialized()) {
    host_->OnSyncCycleCompleted(snapshot, last_keystore_key);
  }
}

void SyncEngineImpl::HandleActionableErrorEventOnFrontendLoop(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnActionableError(sync_error);
}

void SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop(
    ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnMigrationNeededForTypes(types);
}

void SyncEngineImpl::OnInvalidatorStateChange(InvalidatorState state) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoOnInvalidatorStateChange,
                                backend_, state));
}

void SyncEngineImpl::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoOnIncomingInvalidation,
                                backend_, invalidation_map));
}

std::string SyncEngineImpl::GetOwnerName() const {
  return "SyncEngineImpl";
}

void SyncEngineImpl::HandleConnectionStatusChangeOnFrontendLoop(
    ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Connection status changed: " << ConnectionStatusToString(status);
  host_->OnConnectionStatusChange(status);
}

void SyncEngineImpl::HandleProtocolEventOnFrontendLoop(
    std::unique_ptr<ProtocolEvent> event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnProtocolEvent(*event);
}

void SyncEngineImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
    ModelType type,
    const CommitCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnDirectoryTypeCommitCounterUpdated(type, counters);
}

void SyncEngineImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
    ModelType type,
    const UpdateCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnDirectoryTypeUpdateCounterUpdated(type, counters);
}

void SyncEngineImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
    ModelType type,
    const StatusCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnDatatypeStatusCounterUpdated(type, counters);
}

void SyncEngineImpl::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_prefs_->UpdateInvalidationVersions(invalidation_versions);
}

void SyncEngineImpl::OnCookieJarChanged(bool account_mismatch,
                                        bool empty_jar,
                                        const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoOnCookieJarChanged, backend_,
                     account_mismatch, empty_jar, callback));
}

void SyncEngineImpl::SetInvalidationsForSessionsEnabled(bool enabled) {
  sessions_invalidation_enabled_ = enabled;
  // |last_enabled_types_| contains all datatypes, for which user
  // has enabled Sync. So by construction, it cointains also noisy datatypes
  // if nessesary.
  ModelTypeSet enabled_for_invalidation(last_enabled_types_);
  if (!enabled) {
    enabled_for_invalidation.Remove(syncer::SESSIONS);
    enabled_for_invalidation.Remove(syncer::FAVICON_IMAGES);
    enabled_for_invalidation.Remove(syncer::FAVICON_TRACKING);
  }
  bool success = invalidator_->UpdateRegisteredInvalidationIds(
      this, ModelTypeSetToObjectIdSet(enabled_for_invalidation));
  DCHECK(success);
}

void SyncEngineImpl::GetNigoriNodeForDebugging(AllNodesCallback callback) {
  DCHECK(backend_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::GetNigoriNodeForDebugging, backend_,
                     BindToCurrentSequence(std::move(callback))));
}

void SyncEngineImpl::OnInvalidatorClientIdChange(const std::string& client_id) {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoOnInvalidatorClientIdChange,
                     backend_, client_id));
}

void SyncEngineImpl::OnCookieJarChangedDoneOnFrontendLoop(
    const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback.Run();
}

}  // namespace syncer
