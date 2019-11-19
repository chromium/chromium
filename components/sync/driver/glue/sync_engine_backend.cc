// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_backend.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/invalidation_adapter.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/directory_data_type_controller.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/sync_backend_registrar.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/engine_impl/sync_encryption_handler_impl.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/nigori/nigori_model_type_processor.h"
#include "components/sync/nigori/nigori_storage_impl.h"
#include "components/sync/nigori/nigori_sync_bridge_impl.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/nigori_handler_proxy.h"
#include "components/sync/syncable/user_share.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

static const int kSaveChangesIntervalSeconds = 10;

namespace net {
class URLFetcher;
}

namespace syncer {

class EngineComponentsFactory;

namespace {

const base::FilePath::CharType kNigoriStorageFilename[] =
    FILE_PATH_LITERAL("Nigori.bin");

bool ShouldEnableUSSNigori() {
  // USS implementation of Nigori is not compatible with Directory
  // implementations of Passwords and Bookmarks.
  // |kSyncUSSNigori| should be checked last, since check itself has side
  // effect (only clients, which have feature-flag checked participate in the
  // study). Otherwise, we will have different amount of clients in control and
  // experiment groups.
  return base::FeatureList::IsEnabled(switches::kSyncUSSPasswords) &&
         base::FeatureList::IsEnabled(switches::kSyncUSSNigori);
}

}  // namespace

SyncEngineBackend::SyncEngineBackend(const std::string& name,
                                     const base::FilePath& sync_data_folder,
                                     const base::WeakPtr<SyncEngineImpl>& host)
    : name_(name), sync_data_folder_(sync_data_folder), host_(host) {
  DCHECK(host);
  // This is constructed on the UI thread but used from another thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SyncEngineBackend::~SyncEngineBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool SyncEngineBackend::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sync_manager_)
    return false;
  sync_manager_->OnMemoryDump(pmd);
  return true;
}

void SyncEngineBackend::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleSyncCycleCompletedOnFrontendLoop,
             snapshot,
             sync_manager_->GetEncryptionHandler()->GetLastKeystoreKey());
}

void SyncEngineBackend::DoRefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->RefreshTypes(types);
}

void SyncEngineBackend::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    DoDestroySyncManager();
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Sync manager initialization is complete, so we can schedule recurring
  // SaveChanges.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::StartSavingChanges,
                                weak_ptr_factory_.GetWeakPtr()));

  // Hang on to these for a while longer.  We're not ready to hand them back to
  // the UI thread yet.
  js_backend_ = js_backend;
  debug_info_listener_ = debug_info_listener;

  ModelTypeConnector* model_type_connector =
      sync_manager_->GetModelTypeConnector();
  if (nigori_controller_) {
    // Having non-null |nigori_controller_| means that USS implementation of
    // Nigori is enabled.
    LoadAndConnectNigoriController();
  } else {
    // Control types don't have DataTypeControllers, but they need to have
    // update handlers registered in ModelTypeRegistry.
    for (ModelType control_type : ControlTypes()) {
      model_type_connector->RegisterDirectoryType(control_type, GROUP_PASSIVE);
    }
  }

  // Before proceeding any further, we need to download the control types and
  // purge any partial data (ie. data downloaded for a type that was on its way
  // to being initially synced, but didn't quite make it.).  The following
  // configure cycle will take care of this.  It depends on the registrar state
  // which we initialize below to ensure that we don't perform any downloads if
  // all control types have already completed their initial sync.
  registrar_->SetInitialTypes(sync_manager_->InitialSyncEndedTypes());

  ConfigureReason reason = sync_manager_->InitialSyncEndedTypes().Empty()
                               ? CONFIGURE_REASON_NEW_CLIENT
                               : CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;

  ModelTypeSet new_control_types = registrar_->ConfigureDataTypes(
      /*types_to_add=*/ControlTypes(),
      /*types_to_remove=*/ModelTypeSet());

  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  SDVLOG(1) << "Control Types " << ModelTypeSetToString(new_control_types)
            << " added; calling ConfigureSyncer";

  ModelTypeSet types_to_purge =
      Difference(ModelTypeSet::All(), GetRoutingInfoTypes(routing_info));

  sync_manager_->PurgeDisabledTypes(types_to_purge, ModelTypeSet(),
                                    ModelTypeSet());
  sync_manager_->ConfigureSyncer(
      reason, new_control_types, SyncManager::SyncFeatureState::INITIALIZING,
      base::Bind(&SyncEngineBackend::DoInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncEngineBackend::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleConnectionStatusChangeOnFrontendLoop,
             status);
}

void SyncEngineBackend::OnCommitCountersUpdated(
    ModelType type,
    const CommitCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncEngineBackend::OnUpdateCountersUpdated(
    ModelType type,
    const UpdateCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncEngineBackend::OnStatusCountersUpdated(
    ModelType type,
    const StatusCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncEngineBackend::OnActionableError(const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleActionableErrorEventOnFrontendLoop,
             sync_error);
}

void SyncEngineBackend::OnMigrationRequested(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop,
             types);
}

void SyncEngineBackend::OnProtocolEvent(const ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (forward_protocol_events_) {
    std::unique_ptr<ProtocolEvent> event_clone(event.Clone());
    host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
               base::Passed(std::move(event_clone)));
  }
}

void SyncEngineBackend::DoOnInvalidatorStateChange(InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->SetInvalidatorEnabled(state == INVALIDATIONS_ENABLED);
}

bool SyncEngineBackend::ShouldIgnoreRedundantInvalidation(
    const Invalidation& invalidation,
    ModelType type) {
  bool fcm_invalidation = base::FeatureList::IsEnabled(
      invalidation::switches::kFCMInvalidationsForSyncDontCheckVersion);
  bool redundant_invalidation = false;
  auto last_invalidation = last_invalidation_versions_.find(type);
  if (!invalidation.is_unknown_version() &&
      last_invalidation != last_invalidation_versions_.end() &&
      invalidation.version() <= last_invalidation->second) {
    DVLOG(1) << "Ignoring redundant invalidation for "
             << ModelTypeToString(type) << " with version "
             << invalidation.version() << ", last seen version was "
             << last_invalidation->second;
    redundant_invalidation = true;
    UMA_HISTOGRAM_ENUMERATION("Sync.RedundantInvalidationPerModelType", type,
                              static_cast<int>(syncer::ModelType::NUM_ENTRIES));
  } else {
    UMA_HISTOGRAM_ENUMERATION("Sync.NonRedundantInvalidationPerModelType", type,
                              static_cast<int>(syncer::ModelType::NUM_ENTRIES));
  }

  return !fcm_invalidation && redundant_invalidation;
}

void SyncEngineBackend::DoOnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (const invalidation::ObjectId& object_id : ids) {
    ModelType type;
    if (!NotificationTypeToRealModelType(object_id.name(), &type)) {
      DLOG(WARNING) << "Notification has invalid id: "
                    << ObjectIdToString(object_id);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Sync.InvalidationPerModelType",
                                ModelTypeHistogramValue(type));
      SingleObjectInvalidationSet invalidation_set =
          invalidation_map.ForObject(object_id);
      for (Invalidation invalidation : invalidation_set) {
        if (ShouldIgnoreRedundantInvalidation(invalidation, type)) {
          continue;
        }

        std::unique_ptr<InvalidationInterface> inv_adapter(
            new InvalidationAdapter(invalidation));
        sync_manager_->OnIncomingInvalidation(type, std::move(inv_adapter));
        if (!invalidation.is_unknown_version())
          last_invalidation_versions_[type] = invalidation.version();
      }
    }
  }

  host_.Call(FROM_HERE, &SyncEngineImpl::UpdateInvalidationVersions,
             last_invalidation_versions_);
}

void SyncEngineBackend::DoInitialize(SyncEngine::InitParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  if (!base::CreateDirectory(sync_data_folder_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  // Load the previously persisted set of invalidation versions into memory.
  last_invalidation_versions_ = params.invalidation_versions;

  authenticated_account_id_ = params.authenticated_account_id;

  DCHECK(!registrar_);
  DCHECK(params.registrar);
  registrar_ = std::move(params.registrar);

  syncable::NigoriHandler* nigori_handler = nullptr;
  if (ShouldEnableUSSNigori()) {
    auto nigori_processor = std::make_unique<NigoriModelTypeProcessor>();
    nigori_controller_ = std::make_unique<ModelTypeController>(
        NIGORI, std::make_unique<ForwardingModelTypeControllerDelegate>(
                    nigori_processor->GetControllerDelegate().get()));
    sync_encryption_handler_ = std::make_unique<NigoriSyncBridgeImpl>(
        std::move(nigori_processor),
        std::make_unique<NigoriStorageImpl>(
            sync_data_folder_.Append(kNigoriStorageFilename), &encryptor_),
        &encryptor_, base::BindRepeating(&Nigori::GenerateScryptSalt),
        params.restored_key_for_bootstrapping);
    nigori_handler_proxy_ =
        std::make_unique<syncable::NigoriHandlerProxy>(&user_share_);
    sync_encryption_handler_->AddObserver(nigori_handler_proxy_.get());
    nigori_handler = nigori_handler_proxy_.get();
  } else {
    auto sync_encryption_handler_impl =
        std::make_unique<SyncEncryptionHandlerImpl>(
            &user_share_, &encryptor_, params.restored_key_for_bootstrapping,
            params.restored_keystore_key_for_bootstrapping,
            base::BindRepeating(&Nigori::GenerateScryptSalt));
    nigori_handler = sync_encryption_handler_impl.get();
    sync_encryption_handler_ = std::move(sync_encryption_handler_impl);
  }

  sync_manager_ = params.sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);

  SyncManager::InitArgs args;
  args.database_location = sync_data_folder_;
  args.event_handler = params.event_handler;
  args.service_url = params.service_url;
  args.enable_local_sync_backend = params.enable_local_sync_backend;
  args.local_sync_backend_folder = params.local_sync_backend_folder;
  args.post_factory = std::move(params.http_factory_getter).Run();
  registrar_->GetWorkers(&args.workers);
  args.encryption_observer_proxy = std::move(params.encryption_observer_proxy);
  args.extensions_activity = params.extensions_activity.get();
  args.change_delegate = registrar_.get();  // as SyncManager::ChangeDelegate
  args.authenticated_account_id = params.authenticated_account_id;
  args.invalidator_client_id = params.invalidator_client_id;
  args.engine_components_factory = std::move(params.engine_components_factory);
  args.user_share = &user_share_;
  args.encryption_handler = sync_encryption_handler_.get();
  args.nigori_handler = nigori_handler;
  args.unrecoverable_error_handler = params.unrecoverable_error_handler;
  args.report_unrecoverable_error_function =
      params.report_unrecoverable_error_function;
  args.cancelation_signal = &stop_syncing_signal_;
  args.poll_interval = params.poll_interval;
  args.cache_guid = params.cache_guid;
  args.birthday = params.birthday;
  args.bag_of_chips = params.bag_of_chips;
  sync_manager_->Init(&args);
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "SyncDirectory", base::SequencedTaskRunnerHandle::Get(), {});
}

void SyncEngineBackend::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on another
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncEngineBackend::DoInvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_manager_) {
    sync_manager_->InvalidateCredentials();
  }
}

void SyncEngineBackend::DoStartConfiguration() {
  sync_manager_->StartConfiguration();
}

void SyncEngineBackend::DoStartSyncing(base::Time last_poll_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->StartSyncingNormally(last_poll_time);
}

void SyncEngineBackend::DoSetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(passphrase);
}

void SyncEngineBackend::DoAddTrustedVaultDecryptionKeys(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->AddTrustedVaultDecryptionKeys(keys);
}

void SyncEngineBackend::DoInitialProcessControlTypes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Initilalizing Control Types";

  // Initialize encryption.
  if (!sync_manager_->GetEncryptionHandler()->Init()) {
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Note: experiments are currently handled via SBH::AddExperimentalTypes,
  // which is called at the end of every sync cycle.
  // TODO(zea): eventually add an experiment handler and initialize it here.

  const UserShare* user_share = sync_manager_->GetUserShare();
  if (!user_share) {  // Null in some tests.
    DVLOG(1) << "Skipping initialization of DeviceInfo";
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  DCHECK_EQ(user_share->directory->cache_guid(), sync_manager_->cache_guid());
  host_.Call(
      FROM_HERE, &SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop,
      registrar_->GetLastConfiguredTypes(), js_backend_, debug_info_listener_,
      base::Passed(sync_manager_->GetModelTypeConnectorProxy()),
      sync_manager_->cache_guid(), sync_manager_->birthday(),
      sync_manager_->bag_of_chips(),
      sync_manager_->GetEncryptionHandler()->GetLastKeystoreKey());

  js_backend_.Reset();
  debug_info_listener_.Reset();
}

void SyncEngineBackend::DoSetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetDecryptionPassphrase(passphrase);
}

void SyncEngineBackend::DoEnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->EnableEncryptEverything();
}

void SyncEngineBackend::ShutdownOnUIThread() {
  // This will cut short any blocking network tasks, cut short any in-progress
  // sync cycles, and prevent the creation of new blocking network tasks and new
  // sync cycles.  If there was an in-progress network request, it would have
  // had a reference to the RequestContextGetter.  This reference will be
  // dropped by the time this function returns.
  //
  // It is safe to call this even if Sync's backend classes have not been
  // initialized yet.  Those classes will receive the message when the sync
  // thread finally getes around to constructing them.
  stop_syncing_signal_.Signal();
}

void SyncEngineBackend::DoShutdown(ShutdownReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (nigori_handler_proxy_) {
    sync_encryption_handler_->RemoveObserver(nigori_handler_proxy_.get());
  }
  // Having no |sync_manager_| means that initialization was failed and NIGORI
  // wasn't connected and started.
  // TODO(crbug.com/922900): this logic seems fragile, maybe initialization and
  // connecting of NIGORI needs refactoring.
  if (nigori_controller_ && sync_manager_) {
    sync_manager_->GetModelTypeConnector()->DisconnectNonBlockingType(NIGORI);
    nigori_controller_->Stop(reason, base::DoNothing());
  }
  DoDestroySyncManager();

  registrar_ = nullptr;

  if (reason == DISABLE_SYNC) {
    // TODO(crbug.com/922900): We may want to remove Nigori data from the
    // storage if USS Nigori implementation is enabled.
    syncable::Directory::DeleteDirectoryFiles(sync_data_folder_);
  }

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncEngineBackend::DoDestroySyncManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (sync_manager_) {
    DisableDirectoryTypeDebugInfoForwarding();
    save_changes_timer_.reset();
    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread();
    sync_manager_.reset();
  }
}

void SyncEngineBackend::DoPurgeDisabledTypes(const ModelTypeSet& to_purge,
                                             const ModelTypeSet& to_journal,
                                             const ModelTypeSet& to_unapply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->PurgeDisabledTypes(to_purge, to_journal, to_unapply);
  if (to_purge.Has(NIGORI) && nigori_controller_) {
    // We are using USS implementation of Nigori and someone asked us to purge
    // it's data. For regular datatypes it's controlled DataTypeManager, but
    // for Nigori we need to do it here.
    // TODO(crbug.com/922900): try to find better way to implement this logic,
    // it's likely happen only due to BackendMigrator.
    sync_manager_->GetModelTypeConnector()->DisconnectNonBlockingType(NIGORI);
    nigori_controller_->Stop(ShutdownReason::DISABLE_SYNC, base::DoNothing());
    LoadAndConnectNigoriController();
  }
}

void SyncEngineBackend::DoConfigureSyncer(
    ModelTypeConfigurer::ConfigureParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params.ready_task.is_null());

  registrar_->ConfigureDataTypes(params.enabled_types, params.disabled_types);

  base::Closure chained_ready_task(base::Bind(
      &SyncEngineBackend::DoFinishConfigureDataTypes,
      weak_ptr_factory_.GetWeakPtr(), params.to_download, params.ready_task));

  sync_manager_->ConfigureSyncer(params.reason, params.to_download,
                                 params.is_sync_feature_enabled
                                     ? SyncManager::SyncFeatureState::ON
                                     : SyncManager::SyncFeatureState::OFF,
                                 chained_ready_task);
}

void SyncEngineBackend::DoFinishConfigureDataTypes(
    ModelTypeSet types_to_config,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update the enabled types for the bridge and sync manager.
  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);
  enabled_types.RemoveAll(ProxyTypes());

  const ModelTypeSet failed_configuration_types =
      Difference(types_to_config, sync_manager_->InitialSyncEndedTypes());
  const ModelTypeSet succeeded_configuration_types =
      Difference(types_to_config, failed_configuration_types);
  host_.Call(FROM_HERE, &SyncEngineImpl::FinishConfigureDataTypesOnFrontendLoop,
             enabled_types, succeeded_configuration_types,
             failed_configuration_types, ready_task);
}

void SyncEngineBackend::SendBufferedProtocolEventsAndEnableForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = true;

  if (sync_manager_) {
    // Grab our own copy of the buffered events.
    // The buffer is not modified by this operation.
    std::vector<std::unique_ptr<ProtocolEvent>> buffered_events =
        sync_manager_->GetBufferedProtocolEvents();

    // Send them all over the fence to the host.
    for (auto& event : buffered_events) {
      host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
                 base::Passed(std::move(event)));
    }
  }
}

void SyncEngineBackend::DisableProtocolEventForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = false;
}

void SyncEngineBackend::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);

  forward_type_info_ = true;

  if (!sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->RegisterDirectoryTypeDebugInfoObserver(this);
  sync_manager_->RequestEmitDebugInfo();
}

void SyncEngineBackend::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);

  if (!forward_type_info_)
    return;

  forward_type_info_ = false;

  if (sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->UnregisterDirectoryTypeDebugInfoObserver(this);
}

void SyncEngineBackend::StartSavingChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!save_changes_timer_);
  save_changes_timer_ = std::make_unique<base::RepeatingTimer>();
  save_changes_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &SyncEngineBackend::SaveChanges);
}

void SyncEngineBackend::SaveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->SaveChanges();
}

void SyncEngineBackend::DoOnCookieJarChanged(bool account_mismatch,
                                             bool empty_jar,
                                             const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->OnCookieJarChanged(account_mismatch, empty_jar);
  if (!callback.is_null()) {
    host_.Call(FROM_HERE, &SyncEngineImpl::OnCookieJarChangedDoneOnFrontendLoop,
               callback);
  }
}

void SyncEngineBackend::DoOnInvalidatorClientIdChange(
    const std::string& client_id) {
  if (base::FeatureList::IsEnabled(switches::kSyncE2ELatencyMeasurement)) {
    // Don't populate the ID, if client participates in latency measurement
    // experiment.
    return;
  }
  sync_manager_->UpdateInvalidationClientId(client_id);
}

void SyncEngineBackend::GetNigoriNodeForDebugging(AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (nigori_controller_) {
    // USS implementation of Nigori.
    nigori_controller_->GetAllNodes(std::move(callback));
  } else {
    // Directory implementation of Nigori.
    DCHECK(user_share_.directory);
    std::move(callback).Run(
        NIGORI, DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
                    NIGORI, user_share_.directory.get()));
  }
}

bool SyncEngineBackend::HasUnsyncedItemsForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);
  return sync_manager_->HasUnsyncedItemsForTest();
}

void SyncEngineBackend::LoadAndConnectNigoriController() {
  // The controller for Nigori is not exposed to the UI thread or the
  // DataTypeManager, so we need to start it here manually.
  ConfigureContext configure_context;
  configure_context.authenticated_account_id = authenticated_account_id_;
  configure_context.cache_guid = sync_manager_->cache_guid();
  // TODO(crbug.com/922900): investigate whether we want to use
  // kTransportOnly in Butter mode.
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.configuration_start_time = base::Time::Now();
  nigori_controller_->LoadModels(configure_context, base::DoNothing());
  DCHECK_EQ(nigori_controller_->state(), DataTypeController::MODEL_LOADED);
  // TODO(crbug.com/922900): Do we need to call RegisterNonBlockingType() for
  // Nigori?
  sync_manager_->GetModelTypeConnector()->ConnectNonBlockingType(
      NIGORI, nigori_controller_->ActivateManuallyForNigori());
}

}  // namespace syncer
