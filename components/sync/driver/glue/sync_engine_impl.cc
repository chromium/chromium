// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/features.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/active_devices_provider.h"
#include "components/sync/driver/glue/sync_engine_backend.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/invalidations/fcm_handler.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

namespace syncer {

namespace {

// Enables updating invalidator state for Sync standalone invalidations.
BASE_FEATURE(kSyncUpdateInvalidatorState,
             "SyncUpdateInvalidatorState",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reads from prefs into a struct, to be posted across sequences.
SyncEngineBackend::RestoredLocalTransportData
RestoreLocalTransportDataFromPrefs(const SyncTransportDataPrefs& prefs) {
  SyncEngineBackend::RestoredLocalTransportData result;
  result.cache_guid = prefs.GetCacheGuid();
  result.birthday = prefs.GetBirthday();
  result.bag_of_chips = prefs.GetBagOfChips();
  result.poll_interval = prefs.GetPollInterval();
  if (result.poll_interval.is_zero()) {
    result.poll_interval = kDefaultPollInterval;
  }
  return result;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding values, be certain to also
// update the corresponding definition in enums.xml.
enum class SyncTransportDataStartupState {
  kValidData = 0,
  kEmptyCacheGuid = 1,
  kEmptyBirthday = 2,
  kGaiaIdMismatch = 3,
  kMaxValue = kGaiaIdMismatch
};

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  const int kGuidBytes = 128 / 8;
  std::string guid;
  base::Base64Encode(base::RandBytesAsString(kGuidBytes), &guid);
  return guid;
}

SyncTransportDataStartupState ValidateSyncTransportData(
    const SyncTransportDataPrefs& prefs,
    const CoreAccountInfo& core_account_info) {
  // If the cache GUID is empty, it most probably is because local sync data
  // has been fully cleared. Let's treat this as invalid to make sure all prefs
  // are cleared and a new random cache GUID generated.
  if (prefs.GetCacheGuid().empty()) {
    return SyncTransportDataStartupState::kEmptyCacheGuid;
  }

  // If cache GUID is initialized but the birthday isn't, it means the first
  // sync cycle never completed (OnEngineInitialized()). This should be a rare
  // case and theoretically harmless to resume, but as safety precaution, its
  // simpler to regenerate the cache GUID and start from scratch, to avoid
  // protocol violations (fetching updates requires that the request either has
  // a birthday, or there should be no progress marker).
  if (prefs.GetBirthday().empty()) {
    return SyncTransportDataStartupState::kEmptyBirthday;
  }

  // Make sure the cached account information (gaia ID) is equal to the current
  // one (otherwise the data may be corrupt). Note that, for local sync, the
  // authenticated account is always empty.
  if (prefs.GetGaiaId() != core_account_info.gaia) {
    DLOG(WARNING) << "Found mismatching gaia ID in sync preferences";
    return SyncTransportDataStartupState::kGaiaIdMismatch;
  }

  // All good: local sync data looks initialized and valid.
  return SyncTransportDataStartupState::kValidData;
}

}  // namespace

SyncEngineImpl::SyncEngineImpl(
    const std::string& name,
    invalidation::InvalidationService* invalidator,
    SyncInvalidationsService* sync_invalidations_service,
    std::unique_ptr<ActiveDevicesProvider> active_devices_provider,
    std::unique_ptr<SyncTransportDataPrefs> prefs,
    const base::FilePath& sync_data_folder,
    scoped_refptr<base::SequencedTaskRunner> sync_task_runner,
    const base::RepeatingClosure& sync_transport_data_cleared_cb)
    : sync_task_runner_(std::move(sync_task_runner)),
      name_(name),
      prefs_(std::move(prefs)),
      sync_transport_data_cleared_cb_(sync_transport_data_cleared_cb),
      invalidator_(invalidator),
      sync_invalidations_service_(sync_invalidations_service),
#if BUILDFLAG(IS_ANDROID)
      sessions_invalidation_enabled_(false),
#else
      sessions_invalidation_enabled_(true),
#endif
      active_devices_provider_(std::move(active_devices_provider)) {
  DCHECK(prefs_);
  DCHECK(sync_invalidations_service_);
  backend_ = base::MakeRefCounted<SyncEngineBackend>(
      name_, sync_data_folder, weak_ptr_factory_.GetWeakPtr());
  sync_invalidations_service_->AddTokenObserver(this);
}

SyncEngineImpl::~SyncEngineImpl() {
  DCHECK(!backend_ && !host_) << "Must call Shutdown before destructor.";
}

void SyncEngineImpl::Initialize(InitParams params) {
  DCHECK(params.host);
  host_ = params.host;

  // The gaia ID in sync prefs was introduced with M81, so having an empty value
  // is legitimate and should be populated as a one-off migration.
  // TODO(mastiz): Clean up this migration code after a grace period (e.g. 1
  // year).
  if (prefs_->GetGaiaId().empty()) {
    prefs_->SetGaiaId(params.authenticated_account_info.gaia);
  }

  const SyncTransportDataStartupState state =
      ValidateSyncTransportData(*prefs_, params.authenticated_account_info);

  if (state != SyncTransportDataStartupState::kValidData) {
    // The local data is either uninitialized or corrupt, so let's throw
    // everything away and start from scratch with a new cache GUID, which also
    // cascades into datatypes throwing away their dangling sync metadata due to
    // cache GUID mismatches.
    ClearLocalTransportDataAndNotify();
    prefs_->SetCacheGuid(GenerateCacheGUID());
    prefs_->SetGaiaId(params.authenticated_account_info.gaia);
  }

  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoInitialize, backend_,
                                std::move(params),
                                RestoreLocalTransportDataFromPrefs(*prefs_)));

  // If the new invalidations system (SyncInvalidationsService) is fully
  // enabled, then the SyncService doesn't need to communicate with the old
  // InvalidationService anymore.
  if (invalidator_ && base::FeatureList::IsEnabled(kUseSyncInvalidations) &&
      base::FeatureList::IsEnabled(kUseSyncInvalidationsForWalletAndOffer)) {
    DCHECK(!invalidation_handler_registered_);
    invalidator_->RegisterInvalidationHandler(this);
    bool success = invalidator_->UpdateInterestedTopics(this, /*topics=*/{});
    DCHECK(success);
    invalidator_->UnsubscribeFromUnregisteredTopics(this);
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = nullptr;
  }
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

std::string SyncEngineImpl::GetCacheGuid() const {
  return prefs_->GetCacheGuid();
}

std::string SyncEngineImpl::GetBirthday() const {
  return prefs_->GetBirthday();
}

base::Time SyncEngineImpl::GetLastSyncedTimeForDebugging() const {
  return prefs_->GetLastSyncedTime();
}

void SyncEngineImpl::StartConfiguration() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoStartConfiguration, backend_));
}

void SyncEngineImpl::StartSyncingWithServer() {
  DVLOG(1) << name_ << ": SyncEngineImpl::StartSyncingWithServer called.";
  base::Time last_poll_time = prefs_->GetLastPollTime();
  // If there's no known last poll time (e.g. on initial start-up), we treat
  // this as if a poll just happened.
  if (last_poll_time.is_null()) {
    last_poll_time = base::Time::Now();
    prefs_->SetLastPollTime(last_poll_time);
  }
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoStartSyncing, backend_,
                                last_poll_time));
}

void SyncEngineImpl::StartHandlingInvalidations() {
  // Sync invalidation service must be subscribed to data types by this time.
  // Without that, incoming invalidations would be filtered out.
  DCHECK(sync_invalidations_service_->GetInterestedDataTypes().has_value());

  // Adding a listener several times is safe. Only first adding replays last
  // incoming messages.
  sync_invalidations_service_->AddListener(this);
}

void SyncEngineImpl::SetEncryptionPassphrase(
    const std::string& passphrase,
    const KeyDerivationParams& key_derivation_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoSetEncryptionPassphrase,
                                backend_, passphrase, key_derivation_params));
}

void SyncEngineImpl::SetExplicitPassphraseDecryptionKey(
    std::unique_ptr<Nigori> key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoSetExplicitPassphraseDecryptionKey,
                     backend_, std::move(key)));
}

void SyncEngineImpl::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& keys,
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

  backend_->ShutdownOnUIThread();
}

void SyncEngineImpl::Shutdown(ShutdownReason reason) {
  // StopSyncingForShutdown() (which nulls out |host_|) should be
  // called first.
  DCHECK(!host_);

  if (invalidation_handler_registered_) {
    if (reason != ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA) {
      bool success = invalidator_->UpdateInterestedTopics(this, /*topics=*/{});
      DCHECK(success);
    }
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = nullptr;
  }

  // It's safe to call RemoveListener even if AddListener wasn't called
  // before.
  DCHECK(sync_invalidations_service_);
  sync_invalidations_service_->RemoveListener(this);
  sync_invalidations_service_->RemoveTokenObserver(this);
  sync_invalidations_service_ = nullptr;

  last_enabled_types_.Clear();
  invalidation_handler_registered_ = false;

  active_devices_provider_->SetActiveDevicesChangedCallback(
      base::RepeatingClosure());

  model_type_connector_.reset();

  // Shut down and destroy SyncManager.
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoShutdown, backend_, reason));

  // Ensure that |backend_| destroyed inside Sync sequence, not inside current
  // one.
  sync_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));

  if (reason == ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
    ClearLocalTransportDataAndNotify();
  }
}

void SyncEngineImpl::ConfigureDataTypes(ConfigureParams params) {
  DCHECK(Difference(params.to_download, ProtocolTypes()).Empty());

  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoPurgeDisabledTypes,
                                backend_, params.to_purge));
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoConfigureSyncer, backend_,
                                std::move(params)));
}

void SyncEngineImpl::ConnectDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(ProtocolTypes().Has(type));
  model_type_connector_->ConnectDataType(type, std::move(activation_response));
}

void SyncEngineImpl::DisconnectDataType(ModelType type) {
  model_type_connector_->DisconnectDataType(type);
}

void SyncEngineImpl::SetProxyTabsDatatypeEnabled(bool enabled) {
  model_type_connector_->SetProxyTabsDatatypeEnabled(enabled);
}

const SyncEngineImpl::Status& SyncEngineImpl::GetDetailedStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());
  return cached_status_;
}

void SyncEngineImpl::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {
  DCHECK(IsInitialized());
  sync_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::HasUnsyncedItemsForTest, backend_),
      std::move(cb));
}

void SyncEngineImpl::GetThrottledDataTypesForTest(
    base::OnceCallback<void(ModelTypeSet)> cb) const {
  DCHECK(IsInitialized());
  // Instead of reading directly from |cached_status_.throttled_types|, issue
  // a round trip to the backend sequence, in case there is an ongoing cycle
  // that could update the throttled types.
  sync_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(
          [](base::WeakPtr<SyncEngineImpl> engine,
             base::OnceCallback<void(ModelTypeSet)> cb) {
            std::move(cb).Run(engine->cached_status_.throttled_types);
          },
          weak_ptr_factory_.GetMutableWeakPtr(), std::move(cb)));
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

void SyncEngineImpl::FinishConfigureDataTypesOnFrontendLoop(
    const ModelTypeSet enabled_types,
    base::OnceClosure ready_task) {
  last_enabled_types_ = enabled_types;
  SendInterestedTopicsToInvalidator();

  std::move(ready_task).Run();
}

void SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop(
    std::unique_ptr<ModelTypeConnector> model_type_connector,
    const std::string& birthday,
    const std::string& bag_of_chips) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  model_type_connector_ = std::move(model_type_connector);

  initialized_ = true;

  if (invalidator_) {
    invalidator_->RegisterInvalidationHandler(this);
    invalidation_handler_registered_ = true;

    // Fake a state change to initialize the SyncManager's cached invalidator
    // state.
    OnInvalidatorStateChange(invalidator_->GetInvalidatorState());
  } else {
    DCHECK(base::FeatureList::IsEnabled(kUseSyncInvalidations));
    UpdateStandaloneInvalidationsState();
  }

  active_devices_provider_->SetActiveDevicesChangedCallback(base::BindRepeating(
      &SyncEngineImpl::OnActiveDevicesChanged, weak_ptr_factory_.GetWeakPtr()));

  // Initialize active devices count.
  OnActiveDevicesChanged();

  // Save initialization data to preferences.
  prefs_->SetBirthday(birthday);
  prefs_->SetBagOfChips(bag_of_chips);

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  This gets determined based on whether
  // there used to be local transport metadata or not.
  bool is_first_time_sync_configure = false;

  if (prefs_->GetLastSyncedTime().is_null()) {
    is_first_time_sync_configure = true;
    UpdateLastSyncedTime();
  }

  host_->OnEngineInitialized(/*success=*/true, is_first_time_sync_configure);
}

void SyncEngineImpl::HandleInitializationFailureOnFrontendLoop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnEngineInitialized(
      /*success=*/false,
      /*is_first_time_sync_configure=*/false);
}

void SyncEngineImpl::HandleSyncCycleCompletedOnFrontendLoop(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (!IsInitialized()) {
    return;
  }

  UpdateLastSyncedTime();
  if (!snapshot.poll_finish_time().is_null()) {
    prefs_->SetLastPollTime(snapshot.poll_finish_time());
  }
  DCHECK(!snapshot.poll_interval().is_zero());
  prefs_->SetPollInterval(snapshot.poll_interval());
  prefs_->SetBagOfChips(snapshot.bag_of_chips());

  host_->OnSyncCycleCompleted(snapshot);
}

void SyncEngineImpl::HandleActionableProtocolErrorEventOnFrontendLoop(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnActionableProtocolError(sync_error);
}

void SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop(
    ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnMigrationNeededForTypes(types);
}

void SyncEngineImpl::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoOnInvalidatorStateChange,
                                backend_, state));
}

void SyncEngineImpl::OnIncomingInvalidation(
    const invalidation::TopicInvalidationMap& invalidation_map) {
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

void SyncEngineImpl::HandleSyncStatusChanged(const SyncStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool backed_off_types_changed =
      (status.backed_off_types != cached_status_.backed_off_types);
  const bool invalidation_status_changed =
      (status.notifications_enabled != cached_status_.notifications_enabled);
  cached_status_ = status;
  if (backed_off_types_changed) {
    host_->OnBackedOffTypesChanged();
  }
  if (invalidation_status_changed) {
    host_->OnInvalidationStatusChanged();
  }
}

void SyncEngineImpl::OnCookieJarChanged(bool account_mismatch,
                                        base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoOnCookieJarChanged, backend_,
                     account_mismatch, std::move(callback)));
}

void SyncEngineImpl::SetInvalidationsForSessionsEnabled(bool enabled) {
  sessions_invalidation_enabled_ = enabled;
  SendInterestedTopicsToInvalidator();
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

void SyncEngineImpl::OnInvalidationReceived(const std::string& payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  absl::optional<ModelTypeSet> interested_data_types =
      sync_invalidations_service_->GetInterestedDataTypes();

  // Interested data types must be initialized before handling invalidations to
  // prevent missing incoming invalidations which were received during
  // configuration.
  DCHECK(interested_data_types.has_value());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoOnStandaloneInvalidationReceived,
                     backend_, payload, *interested_data_types));
}

void SyncEngineImpl::OnFCMRegistrationTokenChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateStandaloneInvalidationsState();
}

// static
std::string SyncEngineImpl::GenerateCacheGUIDForTest() {
  return GenerateCacheGUID();
}

void SyncEngineImpl::OnCookieJarChangedDoneOnFrontendLoop(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void SyncEngineImpl::SendInterestedTopicsToInvalidator() {
  if (!invalidator_) {
    return;
  }

  // No need to register invalidations for CommitOnlyTypes().
  ModelTypeSet invalidation_enabled_types(
      Difference(last_enabled_types_, CommitOnlyTypes()));
  if (!sessions_invalidation_enabled_) {
    invalidation_enabled_types.Remove(syncer::SESSIONS);
  }
#if BUILDFLAG(IS_ANDROID)
  // On Android, don't subscribe to HISTORY invalidations, to save network
  // traffic.
  invalidation_enabled_types.Remove(HISTORY);
#endif
  // kUseSyncInvalidations means that the new invalidations system is
  // used for all data types except Wallet and Offer, so only keep these types.
  if (base::FeatureList::IsEnabled(kUseSyncInvalidations)) {
    invalidation_enabled_types.RetainAll(
        {AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});
  }

  bool success = invalidator_->UpdateInterestedTopics(
      this, ModelTypeSetToTopicSet(invalidation_enabled_types));
  DCHECK(success);
}

void SyncEngineImpl::OnActiveDevicesChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoOnActiveDevicesChanged, backend_,
                     active_devices_provider_->CalculateInvalidationInfo(
                         cached_status_.cache_guid)));
}

void SyncEngineImpl::UpdateLastSyncedTime() {
  prefs_->SetLastSyncedTime(base::Time::Now());
}

void SyncEngineImpl::ClearLocalTransportDataAndNotify() {
  prefs_->ClearAll();
  sync_transport_data_cleared_cb_.Run();
}

void SyncEngineImpl::UpdateStandaloneInvalidationsState() {
  DCHECK(sync_invalidations_service_);
  if (!sync_invalidations_service_->GetFCMRegistrationToken().has_value() &&
      base::FeatureList::IsEnabled(kSyncUpdateInvalidatorState)) {
    OnInvalidatorStateChange(invalidation::TRANSIENT_INVALIDATION_ERROR);
    return;
  }

  // This code should not be called when the token is empty (which means that
  // sync standalone invalidations are disabled). DCHECK_NE does not support
  // comparison between an optional and a string, so use has_value() directly.
  DCHECK(!sync_invalidations_service_->GetFCMRegistrationToken().has_value() ||
         sync_invalidations_service_->GetFCMRegistrationToken().value() != "");
  OnInvalidatorStateChange(invalidation::INVALIDATIONS_ENABLED);
}

}  // namespace syncer
