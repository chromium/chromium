// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/glue/sync_engine_impl.h"

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
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/service/active_devices_provider.h"
#include "components/sync/service/glue/sync_engine_backend.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"

namespace syncer {

namespace {

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

enum class SyncTransportDataStartupState {
  kValidData,
  kEmptyCacheGuid,
  kEmptyBirthday,
  kGaiaIdMismatch,
};

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  const int kGuidBytes = 128 / 8;
  return base::Base64Encode(base::RandBytesAsVector(kGuidBytes));
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

  // Make sure the previously-syncing account (gaia ID) is equal to the current
  // one (otherwise the data may be corrupt). Note that, for local sync, the
  // authenticated account is always empty.
  if (prefs.GetCurrentSyncingGaiaId() != core_account_info.gaia) {
    // Note that an empty last-syncing-GaiaID is fine and expected if the user
    // signed out and back in again.
    if (!prefs.GetCurrentSyncingGaiaId().empty()) {
      DLOG(WARNING) << "Found mismatching gaia ID in sync preferences";
      return SyncTransportDataStartupState::kGaiaIdMismatch;
    }
  }

  // All good: local sync data looks initialized and valid.
  return SyncTransportDataStartupState::kValidData;
}

}  // namespace

SyncEngineImpl::SyncEngineImpl(
    const std::string& name,
    SyncInvalidationsService* sync_invalidations_service,
    std::unique_ptr<ActiveDevicesProvider> active_devices_provider,
    std::unique_ptr<SyncTransportDataPrefs> prefs,
    const base::FilePath& sync_data_folder,
    scoped_refptr<base::SequencedTaskRunner> sync_task_runner)
    : sync_task_runner_(std::move(sync_task_runner)),
      name_(name),
      prefs_(std::move(prefs)),
      sync_invalidations_service_(sync_invalidations_service),
      active_devices_provider_(std::move(active_devices_provider)),
      engine_created_time_for_metrics_(base::TimeTicks::Now()) {
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

  const SyncTransportDataStartupState state =
      ValidateSyncTransportData(*prefs_, params.authenticated_account_info);

  if (state != SyncTransportDataStartupState::kValidData) {
    // The local data is either uninitialized or corrupt, so let's throw
    // everything away and start from scratch with a new cache GUID, which also
    // cascades into datatypes throwing away their dangling sync metadata due to
    // cache GUID mismatches.
    prefs_->ClearForCurrentAccount();

    prefs_->SetCacheGuid(GenerateCacheGUID());
    prefs_->SetCurrentSyncingGaiaId(params.authenticated_account_info.gaia);
  }

  cached_cache_guid_ = prefs_->GetCacheGuid();
  cached_birthday_ = prefs_->GetBirthday();

  // Clear host here to avoid holding a dangling pointer in case the task
  // outlives the SyncEngineHost. It is safe to clear host here since
  // SyncEngineBackend doesn't actually need it.
  params.host = nullptr;
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoInitialize, backend_,
                                std::move(params),
                                RestoreLocalTransportDataFromPrefs(*prefs_)));
}

bool SyncEngineImpl::IsInitialized() const {
  return initialized_;
}

void SyncEngineImpl::TriggerRefresh(const DataTypeSet& types) {
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
  // The cached cache GUID should usually be identical to the one stored in
  // prefs, but in some cases (when an account got removed from the device) the
  // one in prefs may have been cleared.
  return cached_cache_guid_;
}

std::string SyncEngineImpl::GetBirthday() const {
  // The cached birthday should usually be identical to the one stored in
  // prefs, but in some cases (when an account got removed from the device) the
  // one in prefs may have been cleared.
  return cached_birthday_;
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
  // If there's no known last poll time, that means this is the initial Sync
  // startup. Treat it as if a poll just happened.
  if (last_poll_time.is_null()) {
    last_poll_time = base::Time::Now();
    // Note: Persisting this is important to ensure that polling correctly
    // resumes after a browser restart, even if no poll request happens during
    // this run.
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

  // Adding a listener several times is safe. Replays the last incoming messages
  // received so far.
  sync_invalidations_service_->AddListener(this);

  // UpdateStandaloneInvalidationsState() must be called after AddListener(),
  // the invalidations should not be considered as initialized until any
  // outstanding FCM messages are handled.
  // TODO(crbug.com/40260679): this logic is quite fragile and should be
  // revisited.
  UpdateStandaloneInvalidationsState();
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

  // It's safe to call RemoveListener even if AddListener wasn't called
  // before.
  DCHECK(sync_invalidations_service_);
  sync_invalidations_service_->RemoveListener(this);
  sync_invalidations_service_->RemoveTokenObserver(this);
  sync_invalidations_service_ = nullptr;

  last_enabled_types_.Clear();

  active_devices_provider_->SetActiveDevicesChangedCallback(
      base::RepeatingClosure());

  data_type_connector_.reset();

  // Shut down and destroy SyncManager.
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoShutdown, backend_, reason));

  // Ensure that |backend_| destroyed inside Sync sequence, not inside current
  // one.
  sync_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));

  if (reason == ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
    prefs_->ClearCurrentSyncingGaiaId();
  }
}

void SyncEngineImpl::ConfigureDataTypes(ConfigureParams params) {
  DCHECK(Difference(params.to_download, ProtocolTypes()).empty());

  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoConfigureSyncer, backend_,
                                std::move(params)));
}

void SyncEngineImpl::ConnectDataType(
    DataType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(ProtocolTypes().Has(type));
  data_type_connector_->ConnectDataType(type, std::move(activation_response));
}

void SyncEngineImpl::DisconnectDataType(DataType type) {
  data_type_connector_->DisconnectDataType(type);
}

const SyncStatus& SyncEngineImpl::GetDetailedStatus() const {
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
    base::OnceCallback<void(DataTypeSet)> cb) const {
  DCHECK(IsInitialized());
  // Instead of reading directly from |cached_status_.throttled_types|, issue
  // a round trip to the backend sequence, in case there is an ongoing cycle
  // that could update the throttled types.
  sync_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(
          [](base::WeakPtr<SyncEngineImpl> engine,
             base::OnceCallback<void(DataTypeSet)> cb) {
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
    const DataTypeSet enabled_types,
    base::OnceClosure ready_task) {
  last_enabled_types_ = enabled_types;

  std::move(ready_task).Run();
}

void SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop(
    std::unique_ptr<DataTypeConnector> data_type_connector,
    const std::string& birthday,
    const std::string& bag_of_chips) {
  TRACE_EVENT0("sync",
               "SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_type_connector_ = std::move(data_type_connector);

  initialized_ = true;

  active_devices_provider_->SetActiveDevicesChangedCallback(base::BindRepeating(
      &SyncEngineImpl::OnActiveDevicesChanged, weak_ptr_factory_.GetWeakPtr()));

  // Initialize active devices count.
  OnActiveDevicesChanged();

  // Save initialization data to preferences.
  prefs_->SetBirthday(birthday);
  cached_birthday_ = prefs_->GetBirthday();
  prefs_->SetBagOfChips(bag_of_chips);

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  This gets determined based on whether
  // there used to be local transport metadata or not.
  bool is_first_time_sync_configure = false;

  // NOTE: Keep this logic consistent with how
  // SyncEngineFactoryImpl::HasTransportDataIncludingFirstSync()
  // determines whether transport data exists.
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

void SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop(DataTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_->OnMigrationNeededForTypes(types);
}

void SyncEngineImpl::OnInvalidatorStateChange(bool enabled) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SyncEngineBackend::DoOnInvalidatorStateChange,
                                backend_, enabled));
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
  const bool has_new_invalidated_data_types =
      !cached_status_.invalidated_data_types.HasAll(
          status.invalidated_data_types);
  cached_status_ = status;
  if (backed_off_types_changed) {
    host_->OnBackedOffTypesChanged();
  }
  if (invalidation_status_changed) {
    if (status.notifications_enabled && !invalidations_enabled_reported_) {
      // Record the time since the engine was created until invalidations are
      // initialized.
      base::UmaHistogramMediumTimes(
          "Sync.InvalidationsInitializationTime",
          base::TimeTicks::Now() - engine_created_time_for_metrics_);
      invalidations_enabled_reported_ = true;
    }
    host_->OnInvalidationStatusChanged();
  }
  if (has_new_invalidated_data_types) {
    // Notify about any new data types having pending invalidations. When there
    // are less such data types, this basically means that sync cycle has been
    // finished, and |host_| will be notified via OnSyncCycleCompleted(), so
    // there is no point in duplicating it.
    host_->OnNewInvalidatedDataTypes();
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

bool SyncEngineImpl::IsNextPollTimeInThePast() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time last_poll_time = prefs_->GetLastPollTime();
  base::TimeDelta poll_interval = prefs_->GetPollInterval();
  if (last_poll_time.is_null() || poll_interval.is_zero()) {
    // It's likely the first startup so the very first poll interval is just
    // starting.
    return false;
  }

  base::Time now = base::Time::Now();
  return now >= last_poll_time + poll_interval;
}

void SyncEngineImpl::ClearNigoriDataForMigration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::DoClearNigoriDataForMigration,
                     backend_));
}

void SyncEngineImpl::GetNigoriNodeForDebugging(AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncEngineBackend::GetNigoriNodeForDebugging, backend_,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void SyncEngineImpl::RecordNigoriMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncEngineBackend::RecordNigoriMemoryUsageAndCountsHistograms,
          backend_));
}

void SyncEngineImpl::OnInvalidationReceived(const std::string& payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<DataTypeSet> interested_data_types =
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

void SyncEngineImpl::UpdateStandaloneInvalidationsState() {
  DCHECK(sync_invalidations_service_);

  // Wait for FCM registration token and until the engine actually starts
  // listening for invalidations (and processed the incoming messages if there
  // are any).
  if (!sync_invalidations_service_->GetFCMRegistrationToken().has_value() ||
      !sync_invalidations_service_->HasListener(this)) {
    OnInvalidatorStateChange(/*enabled=*/false);
    return;
  }

  // This code should not be called when the token is empty (which means that
  // sync standalone invalidations are disabled).
  DCHECK_NE(sync_invalidations_service_->GetFCMRegistrationToken().value(), "");

  // TODO(crbug.com/40266819): wait for FCM token to be committed before change
  // the state to enabled.
  OnInvalidatorStateChange(/*enabled=*/true);
}

}  // namespace syncer
