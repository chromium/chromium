// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/sync_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/invalidation_interface.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine_impl/loopback_server/loopback_connection_manager.h"
#include "components/sync/engine_impl/model_type_connector_proxy.h"
#include "components/sync/engine_impl/net/sync_server_connection_manager.h"
#include "components/sync/engine_impl/sync_scheduler.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/protocol/sync.pb.h"

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
      NOTREACHED();
  }
  return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
}

}  // namespace

SyncManagerImpl::SyncManagerImpl(
    const std::string& name,
    network::NetworkConnectionTracker* network_connection_tracker)
    : name_(name),
      network_connection_tracker_(network_connection_tracker),
      initialized_(false),
      observing_network_connectivity_changes_(false),
      sync_encryption_handler_(nullptr) {
  // Pre-fill |notification_info_map_|.
  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
    notification_info_map_.insert(
        std::make_pair(ModelTypeFromInt(i), NotificationInfo()));
  }
}

SyncManagerImpl::~SyncManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
}

SyncManagerImpl::NotificationInfo::NotificationInfo() : total_count(0) {}
SyncManagerImpl::NotificationInfo::~NotificationInfo() {}

base::DictionaryValue* SyncManagerImpl::NotificationInfo::ToValue() const {
  base::DictionaryValue* value = new base::DictionaryValue();
  value->SetInteger("totalCount", total_count);
  value->SetString("payload", payload);
  return value;
}

ModelTypeSet SyncManagerImpl::InitialSyncEndedTypes() {
  DCHECK(initialized_);
  return model_type_registry_->GetInitialSyncEndedTypes();
}

void SyncManagerImpl::ConfigureSyncer(ConfigureReason reason,
                                      ModelTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      base::OnceClosure ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ready_task.is_null());
  DCHECK(initialized_);

  DVLOG(1) << "Configuring -"
           << "\n\t"
           << "types to download: " << ModelTypeSetToString(to_download);
  ConfigurationParams params(GetOriginFromReason(reason), to_download,
                             std::move(ready_task));

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  scheduler_->ScheduleConfiguration(std::move(params));
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
  if (!args->enable_local_sync_backend) {
    DCHECK(!args->authenticated_account_id.empty());
  }
  DCHECK(args->cancelation_signal);
  DVLOG(1) << "SyncManager starting Init...";

  DCHECK(args->encryption_observer_proxy);
  encryption_observer_proxy_ = std::move(args->encryption_observer_proxy);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(args->event_handler);

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
  sync_encryption_handler_->AddObserver(&js_sync_encryption_handler_observer_);

  DVLOG(1) << "AccountId: " << args->authenticated_account_id;

  allstatus_.SetHasKeystoreKey(
      !sync_encryption_handler_->GetKeystoreKeysHandler()->NeedKeystoreKey());

  if (args->enable_local_sync_backend) {
    VLOG(1) << "Running against local sync backend.";
    allstatus_.SetLocalBackendFolder(
        args->local_sync_backend_folder.AsUTF8Unsafe());
    connection_manager_ = std::make_unique<LoopbackConnectionManager>(
        args->local_sync_backend_folder);
  } else {
    connection_manager_ = std::make_unique<SyncServerConnectionManager>(
        args->service_url.host() + args->service_url.path(),
        args->service_url.EffectiveIntPort(),
        args->service_url.SchemeIsCryptographic(),
        std::move(args->post_factory), args->cancelation_signal);
  }
  connection_manager_->set_client_id(args->cache_guid);
  connection_manager_->AddListener(this);

  DVLOG(1) << "Setting sync client ID: " << args->cache_guid;
  allstatus_.SetSyncId(args->cache_guid);
  DVLOG(1) << "Setting invalidator client ID: " << args->invalidator_client_id;
  allstatus_.SetInvalidatorClientId(args->invalidator_client_id);

  // Add observers after all initializations. Each observer will get initialized
  // status while being added.
  for (SyncStatusObserver* observer : args->sync_status_observers) {
    allstatus_.AddObserver(observer);
  }

  model_type_registry_ = std::make_unique<ModelTypeRegistry>(
      args->workers, this, args->cancelation_signal,
      sync_encryption_handler_->GetKeystoreKeysHandler());
  sync_encryption_handler_->AddObserver(model_type_registry_.get());

  // Build a SyncCycleContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncCycleContext.";
  std::vector<SyncEngineEventListener*> listeners;
  listeners.push_back(&allstatus_);
  listeners.push_back(this);
  cycle_context_ = args->engine_components_factory->BuildContext(
      connection_manager_.get(), args->extensions_activity, listeners,
      &debug_info_event_listener_, model_type_registry_.get(),
      args->invalidator_client_id, args->cache_guid, args->birthday,
      args->bag_of_chips, args->poll_interval);
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

  NotifyInitializationSuccess();
}

void SyncManagerImpl::NotifyInitializationSuccess() {
  for (auto& observer : observers_) {
    observer.OnInitializationComplete(
        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()), true);
  }
}

void SyncManagerImpl::NotifyInitializationFailure() {
  for (auto& observer : observers_) {
    observer.OnInitializationComplete(
        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()), false);
  }
}

void SyncManagerImpl::OnPassphraseRequired(
    PassphraseRequiredReason reason,
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

void SyncManagerImpl::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {
  if (type == KEYSTORE_BOOTSTRAP_TOKEN)
    allstatus_.SetHasKeystoreKey(true);
}

void SyncManagerImpl::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                              bool encrypt_everything) {
  allstatus_.SetEncryptedTypes(encrypted_types);
}

void SyncManagerImpl::OnEncryptionComplete() {
  // Does nothing.
}

void SyncManagerImpl::OnCryptographerStateChanged(Cryptographer* cryptographer,
                                                  bool has_pending_keys) {
  allstatus_.SetCryptographerCanEncrypt(cryptographer->CanEncrypt());
  allstatus_.SetCryptoHasPendingKeys(has_pending_keys);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
}

void SyncManagerImpl::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  allstatus_.SetPassphraseType(type);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
}

void SyncManagerImpl::StartSyncingNormally(
    base::Time last_poll_time) {
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
  DCHECK(!credentials.account_id.empty());
  cycle_context_->set_account_name(credentials.email);

  observing_network_connectivity_changes_ = true;
  if (!connection_manager_->SetAccessToken(credentials.access_token))
    return;  // Auth token is known to be invalid, so exit early.

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

  if (model_type_registry_)
    sync_encryption_handler_->RemoveObserver(model_type_registry_.get());

  model_type_registry_.reset();

  if (sync_encryption_handler_) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
  }

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  // |connection_manager_| may end up being null here in tests (in synchronous
  // initialization mode).
  //
  // TODO(akalin): Fix this behavior.
  if (connection_manager_)
    connection_manager_->RemoveListener(this);
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
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_OK);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_AUTH_ERROR) {
    observing_network_connectivity_changes_ = false;
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_AUTH_ERROR);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_SERVER_ERROR) {
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_SERVER_ERROR);
    }
  }
}

void SyncManagerImpl::RequestNudgeForDataTypes(
    const base::Location& nudge_location,
    ModelTypeSet types) {
  debug_info_event_listener_.OnNudgeFromDatatype(*(types.begin()));

  scheduler_->ScheduleLocalNudge(types, nudge_location);
}

void SyncManagerImpl::NudgeForInitialDownload(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->ScheduleInitialSyncNudge(type);
}

void SyncManagerImpl::NudgeForCommit(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestNudgeForDataTypes(FROM_HERE, ModelTypeSet(type));
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
    for (auto& observer : observers_) {
      observer.OnSyncCycleCompleted(event.snapshot);
    }
  }
}

void SyncManagerImpl::OnActionableError(const SyncProtocolError& error) {
  for (auto& observer : observers_) {
    observer.OnActionableError(error);
  }
}

void SyncManagerImpl::OnRetryTimeChanged(base::Time) {}

void SyncManagerImpl::OnThrottledTypesChanged(ModelTypeSet) {}

void SyncManagerImpl::OnBackedOffTypesChanged(ModelTypeSet) {}

void SyncManagerImpl::OnMigrationRequested(ModelTypeSet types) {
  for (auto& observer : observers_) {
    observer.OnMigrationRequested(types);
  }
}

void SyncManagerImpl::OnProtocolEvent(const ProtocolEvent& event) {
  protocol_event_buffer_.RecordProtocolEvent(event);
  for (auto& observer : observers_) {
    observer.OnProtocolEvent(event);
  }
}

void SyncManagerImpl::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_sync_manager_observer_.SetJsEventHandler(event_handler);
  js_sync_encryption_handler_observer_.SetJsEventHandler(event_handler);
}

void SyncManagerImpl::SetInvalidatorEnabled(bool invalidator_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Invalidator enabled state is now: " << invalidator_enabled;
  allstatus_.SetNotificationsEnabled(invalidator_enabled);
  scheduler_->SetNotificationsEnabled(invalidator_enabled);
}

void SyncManagerImpl::OnIncomingInvalidation(
    ModelType type,
    std::unique_ptr<InvalidationInterface> invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  allstatus_.IncrementNotificationsReceived();
  scheduler_->ScheduleInvalidationNudge(type, std::move(invalidation),
                                        FROM_HERE);
}

void SyncManagerImpl::RefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (types.Empty()) {
    LOG(WARNING) << "Sync received refresh request with no types specified.";
  } else {
    scheduler_->ScheduleLocalRefreshRequest(types, FROM_HERE);
  }
}

ModelTypeConnector* SyncManagerImpl::GetModelTypeConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_registry_.get();
}

std::unique_ptr<ModelTypeConnector>
SyncManagerImpl::GetModelTypeConnectorProxy() {
  DCHECK(initialized_);
  return std::make_unique<ModelTypeConnectorProxy>(
      base::SequencedTaskRunnerHandle::Get(),
      model_type_registry_->AsWeakPtr());
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
  return model_type_registry_->HasUnsyncedItems();
}

SyncEncryptionHandler* SyncManagerImpl::GetEncryptionHandler() {
  DCHECK(sync_encryption_handler_);
  return sync_encryption_handler_;
}

std::vector<std::unique_ptr<ProtocolEvent>>
SyncManagerImpl::GetBufferedProtocolEvents() {
  return protocol_event_buffer_.GetBufferedProtocolEvents();
}

void SyncManagerImpl::RegisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  model_type_registry_->RegisterDirectoryTypeDebugInfoObserver(observer);
}

void SyncManagerImpl::UnregisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  model_type_registry_->UnregisterDirectoryTypeDebugInfoObserver(observer);
}

bool SyncManagerImpl::HasDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  return model_type_registry_->HasDirectoryTypeDebugInfoObserver(observer);
}

void SyncManagerImpl::RequestEmitDebugInfo() {
  model_type_registry_->RequestEmitDebugInfo();
}

void SyncManagerImpl::OnCookieJarChanged(bool account_mismatch,
                                         bool empty_jar) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cycle_context_->set_cookie_jar_mismatch(account_mismatch);
  cycle_context_->set_cookie_jar_empty(empty_jar);
}

void SyncManagerImpl::UpdateInvalidationClientId(const std::string& client_id) {
  DVLOG(1) << "Setting invalidator client ID: " << client_id;
  allstatus_.SetInvalidatorClientId(client_id);
  cycle_context_->set_invalidator_client_id(client_id);
}

}  // namespace syncer
