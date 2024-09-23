// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/data_type_connector_proxy.h"
#include "components/sync/engine/data_type_worker.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/loopback_server/loopback_connection_manager.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/sync_server_connection_manager.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_scheduler.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace syncer {
namespace {

sync_pb::SyncEnums::GetUpdatesOrigin GetOriginFromReason(
    ConfigureReason reason) {
  switch (reason) {
    case CONFIGURE_REASON_RECONFIGURATION:
      return sync_pb::SyncEnums::RECONFIGURATION;
    case CONFIGURE_REASON_MIGRATION:
      return sync_pb::SyncEnums::MIGRATION;
    case CONFIGURE_REASON_NEW_CLIENT:
      return sync_pb::SyncEnums::NEW_CLIENT;
    case CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
    case CONFIGURE_REASON_CRYPTO:
      return sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE;
    case CONFIGURE_REASON_PROGRAMMATIC:
      return sync_pb::SyncEnums::PROGRAMMATIC;
    case CONFIGURE_REASON_UNKNOWN:
      NOTREACHED_IN_MIGRATION();
  }
  return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
}

const char kSyncServerSyncPath[] = "/command/";

std::string StripTrailingSlash(const std::string& s) {
  int stripped_end_pos = s.size();
  if (s.at(stripped_end_pos - 1) == '/') {
    stripped_end_pos = stripped_end_pos - 1;
  }

  return s.substr(0, stripped_end_pos);
}

GURL MakeConnectionURL(const GURL& sync_server, const std::string& client_id) {
  DCHECK_EQ(kSyncServerSyncPath[0], '/');
  std::string full_path =
      StripTrailingSlash(sync_server.path()) + kSyncServerSyncPath;

  GURL::Replacements path_replacement;
  path_replacement.SetPathStr(full_path);
  return AppendSyncQueryString(sync_server.ReplaceComponents(path_replacement),
                               client_id);
}

}  // namespace

SyncManagerImpl::SyncManagerImpl(
    const std::string& name,
    network::NetworkConnectionTracker* network_connection_tracker)
    : name_(name), network_connection_tracker_(network_connection_tracker) {}

SyncManagerImpl::~SyncManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
}

DataTypeSet SyncManagerImpl::InitialSyncEndedTypes() {
  DCHECK(initialized_);
  return data_type_registry_->GetInitialSyncEndedTypes();
}

DataTypeSet SyncManagerImpl::GetConnectedTypes() {
  DCHECK(initialized_);
  return data_type_registry_->GetConnectedTypes();
}

void SyncManagerImpl::ConfigureSyncer(ConfigureReason reason,
                                      DataTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      base::OnceClosure ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ready_task.is_null());
  DCHECK(initialized_);

  DVLOG(1) << "Configuring -" << "\n\t"
           << "types to download: " << DataTypeSetToDebugString(to_download);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  scheduler_->ScheduleConfiguration(GetOriginFromReason(reason), to_download,
                                    std::move(ready_task));
  if (sync_feature_state != SyncFeatureState::INITIALIZING) {
    cycle_context_->set_is_sync_feature_enabled(sync_feature_state ==
                                                SyncFeatureState::ON);
  }
}

void SyncManagerImpl::Init(InitArgs* args) {
  DCHECK(!initialized_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!args->cache_guid.empty());
  DCHECK(args->post_factory);
  DCHECK(!args->poll_interval.is_zero());
  DCHECK(args->cancelation_signal);
  DVLOG(1) << "SyncManager starting Init...";

  DCHECK(args->encryption_observer_proxy);
  encryption_observer_proxy_ = std::move(args->encryption_observer_proxy);

  AddObserver(&debug_info_event_listener_);

  DCHECK(args->encryption_handler);
  sync_encryption_handler_ = args->encryption_handler;

  // Register for encryption related changes now. We have to do this before
  // the initial download of control types or initializing the encryption
  // handler in order to receive notifications triggered during encryption
  // startup.
  sync_encryption_handler_->AddObserver(this);
  sync_encryption_handler_->AddObserver(encryption_observer_proxy_.get());
  sync_encryption_handler_->AddObserver(&debug_info_event_listener_);

  // base::Unretained() is safe here because SyncManagerImpl outlives
  // sync_status_tracker_.
  sync_status_tracker_ =
      std::make_unique<SyncStatusTracker>(base::BindRepeating(
          &SyncManagerImpl::NotifySyncStatusChanged, base::Unretained(this)));
  sync_status_tracker_->SetHasKeystoreKey(
      !sync_encryption_handler_->GetKeystoreKeysHandler()->NeedKeystoreKey());

  if (args->enable_local_sync_backend) {
    VLOG(1) << "Running against local sync backend.";
    sync_status_tracker_->SetLocalBackendFolder(
        args->local_sync_backend_folder.AsUTF8Unsafe());
    connection_manager_ = std::make_unique<LoopbackConnectionManager>(
        args->local_sync_backend_folder);
  } else {
    connection_manager_ = std::make_unique<SyncServerConnectionManager>(
        MakeConnectionURL(args->service_url, args->cache_guid),
        std::move(args->post_factory), args->cancelation_signal);
  }
  connection_manager_->AddListener(this);

  DVLOG(1) << "Setting sync client ID: " << args->cache_guid;
  sync_status_tracker_->SetCacheGuid(args->cache_guid);

  data_type_registry_ = std::make_unique<DataTypeRegistry>(
      this, args->cancelation_signal, sync_encryption_handler_);

  // Build a SyncCycleContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncCycleContext.";
  std::vector<SyncEngineEventListener*> listeners = {
      this, sync_status_tracker_.get()};
  cycle_context_ = args->engine_components_factory->BuildContext(
      connection_manager_.get(), args->extensions_activity, listeners,
      &debug_info_event_listener_, data_type_registry_.get(), args->cache_guid,
      args->birthday, args->bag_of_chips, args->poll_interval);
  scheduler_ = args->engine_components_factory->BuildScheduler(
      name_, cycle_context_.get(), args->cancelation_signal,
      args->enable_local_sync_backend);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());

  initialized_ = true;

  if (!args->enable_local_sync_backend) {
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  } else {
    scheduler_->OnCredentialsUpdated();
  }

  debug_info_event_listener_.InitializationComplete();
}

void SyncManagerImpl::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  // Does nothing.
}

void SyncManagerImpl::OnPassphraseAccepted() {
  // Does nothing.
}

void SyncManagerImpl::OnTrustedVaultKeyRequired() {
  // Does nothing.
}

void SyncManagerImpl::OnTrustedVaultKeyAccepted() {
  // Does nothing.
}

void SyncManagerImpl::OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                                              bool encrypt_everything) {
  sync_status_tracker_->SetEncryptedTypes(encrypted_types);
}

void SyncManagerImpl::OnCryptographerStateChanged(Cryptographer* cryptographer,
                                                  bool has_pending_keys) {
  sync_status_tracker_->SetCryptographerCanEncrypt(cryptographer->CanEncrypt());
  sync_status_tracker_->SetCryptoHasPendingKeys(has_pending_keys);
  sync_status_tracker_->SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
  sync_status_tracker_->SetTrustedVaultDebugInfo(
      sync_encryption_handler_->GetTrustedVaultDebugInfo());
  sync_status_tracker_->SetHasKeystoreKey(
      !sync_encryption_handler_->GetKeystoreKeysHandler()->NeedKeystoreKey());
}

void SyncManagerImpl::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  sync_status_tracker_->SetPassphraseType(type);
  sync_status_tracker_->SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
}

void SyncManagerImpl::StartSyncingNormally(base::Time last_poll_time) {
  // Start the sync scheduler.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
}

void SyncManagerImpl::StartConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
}

void SyncManagerImpl::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  cycle_context_->set_account_name(credentials.email);

  observing_network_connectivity_changes_ = true;
  if (!connection_manager_->SetAccessToken(credentials.access_token)) {
    return;  // Auth token is known to be invalid, so exit early.
  }

  scheduler_->OnCredentialsUpdated();

  // TODO(zea): pass the credential age to the debug info event listener.
}

void SyncManagerImpl::InvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_manager_->SetAccessToken(std::string());
}

void SyncManagerImpl::AddObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SyncManagerImpl::RemoveObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SyncManagerImpl::ShutdownOnSyncThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Prevent any in-flight method calls from running.
  weak_ptr_factory_.InvalidateWeakPtrs();

  scheduler_.reset();
  cycle_context_.reset();

  data_type_registry_.reset();

  if (sync_encryption_handler_) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
    sync_encryption_handler_->RemoveObserver(encryption_observer_proxy_.get());
  }

  RemoveObserver(&debug_info_event_listener_);

  // |connection_manager_| may end up being null here in tests (in synchronous
  // initialization mode).
  //
  // TODO(akalin): Fix this behavior.
  if (connection_manager_) {
    connection_manager_->RemoveListener(this);
  }
  connection_manager_.reset();

  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  observing_network_connectivity_changes_ = false;

  initialized_ = false;
}

void SyncManagerImpl::OnConnectionChanged(network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observing_network_connectivity_changes_) {
    DVLOG(1) << "Network change dropped.";
    return;
  }
  DVLOG(1) << "Network change detected.";
  scheduler_->OnConnectionStatusChange(type);
}

void SyncManagerImpl::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event.connection_code == HttpResponse::SERVER_CONNECTION_OK) {
    for (SyncManager::Observer& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_OK);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_AUTH_ERROR) {
    observing_network_connectivity_changes_ = false;
    for (SyncManager::Observer& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_AUTH_ERROR);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_SERVER_ERROR) {
    for (SyncManager::Observer& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_SERVER_ERROR);
    }
  }
}

void SyncManagerImpl::NudgeForInitialDownload(DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->ScheduleInitialSyncNudge(type);
}

void SyncManagerImpl::NudgeForCommit(DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debug_info_event_listener_.OnNudgeFromDatatype(type);
  scheduler_->ScheduleLocalNudge(type);
}

void SyncManagerImpl::SetHasPendingInvalidations(
    DataType type,
    bool has_pending_invalidations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->SetHasPendingInvalidations(type, has_pending_invalidations);
  sync_status_tracker_->SetHasPendingInvalidations(type,
                                                   has_pending_invalidations);
}

void SyncManagerImpl::NotifySyncStatusChanged(const SyncStatus& status) {
  for (SyncManager::Observer& observer : observers_) {
    observer.OnSyncStatusChanged(status);
  }
}

void SyncManagerImpl::OnSyncCycleEvent(const SyncCycleEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up to date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncCycleEvent::SYNC_CYCLE_ENDED) {
    if (!initialized_) {
      DVLOG(1) << "OnSyncCycleCompleted not sent because sync api is not "
               << "initialized";
      return;
    }

    DVLOG(1) << "Sending OnSyncCycleCompleted";
    for (SyncManager::Observer& observer : observers_) {
      observer.OnSyncCycleCompleted(event.snapshot);
    }
  }
}

void SyncManagerImpl::OnActionableProtocolError(
    const SyncProtocolError& error) {
  for (SyncManager::Observer& observer : observers_) {
    observer.OnActionableProtocolError(error);
  }
}

void SyncManagerImpl::OnRetryTimeChanged(base::Time) {}

void SyncManagerImpl::OnThrottledTypesChanged(DataTypeSet) {}

void SyncManagerImpl::OnBackedOffTypesChanged(DataTypeSet) {}

void SyncManagerImpl::OnMigrationRequested(DataTypeSet types) {
  for (SyncManager::Observer& observer : observers_) {
    observer.OnMigrationRequested(types);
  }
}

void SyncManagerImpl::OnProtocolEvent(const ProtocolEvent& event) {
  protocol_event_buffer_.RecordProtocolEvent(event);
  for (SyncManager::Observer& observer : observers_) {
    observer.OnProtocolEvent(event);
  }
}

void SyncManagerImpl::SetInvalidatorEnabled(bool invalidator_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Invalidator enabled state is now: " << invalidator_enabled;
  sync_status_tracker_->SetNotificationsEnabled(invalidator_enabled);
  scheduler_->SetNotificationsEnabled(invalidator_enabled);
}

void SyncManagerImpl::OnIncomingInvalidation(
    DataType type,
    std::unique_ptr<SyncInvalidation> invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateHandler* handler = data_type_registry_->GetMutableUpdateHandler(type);
  if (handler) {
    handler->RecordRemoteInvalidation(std::move(invalidation));
  } else {
    DataTypeWorker::LogPendingInvalidationStatus(
        PendingInvalidationStatus::kDataTypeNotConnected);
  }
  sync_status_tracker_->IncrementNotificationsReceived();
  scheduler_->ScheduleInvalidationNudge(type);
}

void SyncManagerImpl::RefreshTypes(DataTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const DataTypeSet types_to_refresh =
      Intersection(types, data_type_registry_->GetConnectedTypes());

  if (!types_to_refresh.empty()) {
    scheduler_->ScheduleLocalRefreshRequest(types_to_refresh);
  }
}

DataTypeConnector* SyncManagerImpl::GetDataTypeConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_type_registry_.get();
}

std::unique_ptr<DataTypeConnector>
SyncManagerImpl::GetDataTypeConnectorProxy() {
  DCHECK(initialized_);
  return std::make_unique<DataTypeConnectorProxy>(
      base::SequencedTaskRunner::GetCurrentDefault(),
      data_type_registry_->AsWeakPtr());
}

std::string SyncManagerImpl::cache_guid() {
  DCHECK(initialized_);
  return cycle_context_->cache_guid();
}

std::string SyncManagerImpl::birthday() {
  DCHECK(initialized_);
  DCHECK(cycle_context_);
  return cycle_context_->birthday();
}

std::string SyncManagerImpl::bag_of_chips() {
  DCHECK(initialized_);
  DCHECK(cycle_context_);
  return cycle_context_->bag_of_chips();
}

bool SyncManagerImpl::HasUnsyncedItemsForTest() {
  return data_type_registry_->HasUnsyncedItems();
}

SyncEncryptionHandler* SyncManagerImpl::GetEncryptionHandler() {
  DCHECK(sync_encryption_handler_);
  return sync_encryption_handler_;
}

std::vector<std::unique_ptr<ProtocolEvent>>
SyncManagerImpl::GetBufferedProtocolEvents() {
  return protocol_event_buffer_.GetBufferedProtocolEvents();
}

void SyncManagerImpl::OnCookieJarChanged(bool account_mismatch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cycle_context_->set_cookie_jar_mismatch(account_mismatch);
}

void SyncManagerImpl::UpdateActiveDevicesInvalidationInfo(
    ActiveDevicesInvalidationInfo active_devices_invalidation_info) {
  cycle_context_->set_active_devices_invalidation_info(
      std::move(active_devices_invalidation_info));
}

}  // namespace syncer
