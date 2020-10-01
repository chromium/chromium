// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/profile_sync_service.h"

#include <cstddef>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/legacy_directory_deletion.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/driver/backend_migrator.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_auth_manager.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/sync_error.h"
#include "components/version_info/version_info_values.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace syncer {

namespace {

// The initial state of sync, for the Sync.InitialState histogram. Even if
// this value is CAN_START, sync startup might fail for reasons that we may
// want to consider logging in the future, such as a passphrase needed for
// decryption, or the version of Chrome being too old. This enum is used to
// back a UMA histogram, and should therefore be treated as append-only.
enum SyncInitialState {
  CAN_START = 0,                // Sync can attempt to start up.
  NOT_SIGNED_IN = 1,            // There is no signed in user.
  NOT_REQUESTED = 2,            // The user turned off sync.
  NOT_REQUESTED_NOT_SETUP = 3,  // The user turned off sync and setup completed
                                // is false. Might indicate a stop-and-clear.
  NEEDS_CONFIRMATION = 4,       // The user must confirm sync settings.
  NOT_ALLOWED_BY_POLICY = 5,    // Sync is disallowed by enterprise policy.
  OBSOLETE_NOT_ALLOWED_BY_PLATFORM = 6,
  kMaxValue = OBSOLETE_NOT_ALLOWED_BY_PLATFORM
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding values, be certain to also
// update the corresponding definition in enums.xml.
enum class LocalSyncTransportDataStartupState {
  kValidData = 0,
  kEmptyCacheGuid = 1,
  kEmptyBirthday = 2,
  kGaiaIdMismatch = 3,
  kMaxValue = kGaiaIdMismatch
};

void LogSyncTransportDataState(LocalSyncTransportDataStartupState status) {
  UMA_HISTOGRAM_ENUMERATION("Sync.LocalSyncTransportDataStartupState", status);
}

void RecordSyncInitialState(SyncService::DisableReasonSet disable_reasons,
                            bool first_setup_complete) {
  SyncInitialState sync_state = CAN_START;
  if (disable_reasons.Has(ProfileSyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
    sync_state = NOT_SIGNED_IN;
  } else if (disable_reasons.Has(
                 ProfileSyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    sync_state = NOT_ALLOWED_BY_POLICY;
  } else if (disable_reasons.Has(
                 ProfileSyncService::DISABLE_REASON_USER_CHOICE)) {
    if (first_setup_complete) {
      sync_state = NOT_REQUESTED;
    } else {
      sync_state = NOT_REQUESTED_NOT_SETUP;
    }
  } else if (!first_setup_complete) {
    sync_state = NEEDS_CONFIRMATION;
  }
  base::UmaHistogramEnumeration("Sync.InitialState", sync_state);
}

EngineComponentsFactory::Switches EngineSwitchesFromCommandLine() {
  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncShortNudgeDelayForTest)) {
    factory_switches.force_short_nudge_delay_for_test = true;
  }
  return factory_switches;
}

DataTypeController::TypeMap BuildDataTypeControllerMap(
    DataTypeController::TypeVector controllers) {
  DataTypeController::TypeMap type_map;
  for (std::unique_ptr<DataTypeController>& controller : controllers) {
    DCHECK(controller);
    ModelType type = controller->type();
    DCHECK_EQ(0U, type_map.count(type));
    type_map[type] = std::move(controller);
  }
  return type_map;
}

std::unique_ptr<HttpPostProviderFactory> CreateHttpBridgeFactory(
    const std::string& user_agent,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    const NetworkTimeUpdateCallback& network_time_update_callback) {
  return std::make_unique<HttpBridgeFactory>(
      user_agent, std::move(pending_url_loader_factory),
      network_time_update_callback);
}

void EmitUmaMetricWithEmitTimeMinutes(const std::string& histogram_name) {
  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  base::UmaHistogramExactLinear(histogram_name, now_exploded.minute, 60);
}

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  const int kGuidBytes = 128 / 8;
  std::string guid;
  base::Base64Encode(base::RandBytesAsString(kGuidBytes), &guid);
  return guid;
}

bool IsLocalSyncTransportDataValid(const SyncPrefs& sync_prefs,
                                   const CoreAccountInfo& core_account_info) {
  // If the cache GUID is empty, it most probably is because local sync data
  // has been fully cleared via ClearLocalSyncTransportData() due to
  // ShutdownReason::DISABLE_SYNC. Let's return false here anyway to make sure
  // all prefs are cleared and a new random cache GUID generated.
  if (sync_prefs.GetCacheGuid().empty()) {
    LogSyncTransportDataState(
        LocalSyncTransportDataStartupState::kEmptyCacheGuid);
    return false;
  }

  // If cache GUID is initialized but the birthday isn't, it means the first
  // sync cycle never completed (OnEngineInitialized()). This should be a rare
  // case and theoretically harmless to resume, but as safety precaution, its
  // simpler to regenerate the cache GUID and start from scratch, to avoid
  // protocol violations (fetching updates requires that the request either has
  // a birthday, or there should be no progress marker).
  if (sync_prefs.GetBirthday().empty()) {
    LogSyncTransportDataState(
        LocalSyncTransportDataStartupState::kEmptyBirthday);
    return false;
  }

  // Make sure the cached account information (gaia ID) is equal to the current
  // one (otherwise the data may be corrupt). Note that, for local sync
  // (IsLocalSyncEnabled()), the authenticated account is always empty.
  if (sync_prefs.GetGaiaId() != core_account_info.gaia) {
    DLOG(WARNING) << "Found mismatching gaia ID in sync preferences";
    LogSyncTransportDataState(
        LocalSyncTransportDataStartupState::kGaiaIdMismatch);
    return false;
  }

  // All good: local sync data looks initialized and valid.
  LogSyncTransportDataState(LocalSyncTransportDataStartupState::kValidData);
  return true;
}

}  // namespace

ProfileSyncService::InitParams::InitParams() = default;
ProfileSyncService::InitParams::InitParams(InitParams&& other) = default;
ProfileSyncService::InitParams::~InitParams() = default;

ProfileSyncService::ProfileSyncService(InitParams init_params)
    : sync_client_(std::move(init_params.sync_client)),
      sync_prefs_(sync_client_->GetPrefService()),
      identity_manager_(init_params.identity_manager),
      auth_manager_(std::make_unique<SyncAuthManager>(
          identity_manager_,
          base::BindRepeating(&ProfileSyncService::AccountStateChanged,
                              base::Unretained(this)),
          base::BindRepeating(&ProfileSyncService::CredentialsChanged,
                              base::Unretained(this)))),
      channel_(init_params.channel),
      debug_identifier_(init_params.debug_identifier),
      autofill_enable_account_wallet_storage_(
          init_params.autofill_enable_account_wallet_storage),
      enable_passwords_account_storage_(
          init_params.enable_passwords_account_storage),
      sync_service_url_(
          GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(), channel_)),
      crypto_(
          base::BindRepeating(&ProfileSyncService::NotifyObservers,
                              base::Unretained(this)),
          base::BindRepeating(&ProfileSyncService::OnRequiredUserActionChanged,
                              base::Unretained(this)),
          base::BindRepeating(&ProfileSyncService::ReconfigureDueToPassphrase,
                              base::Unretained(this)),
          &sync_prefs_,
          sync_client_->GetTrustedVaultClient()),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      network_time_update_callback_(
          std::move(init_params.network_time_update_callback)),
      url_loader_factory_(std::move(init_params.url_loader_factory)),
      network_connection_tracker_(init_params.network_connection_tracker),
      is_first_time_sync_configure_(false),
      sync_disabled_by_admin_(false),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      expect_sync_configuration_aborted_(false),
      invalidations_identity_provider_(
          init_params.invalidations_identity_provider),
      create_http_post_provider_factory_cb_(
          base::BindRepeating(&CreateHttpBridgeFactory)),
      start_behavior_(init_params.start_behavior),
      passphrase_prompt_triggered_by_version_(false),
      is_stopping_and_clearing_(false),
      should_record_trusted_vault_error_shown_on_startup_(true),
#if defined(OS_ANDROID)
      sessions_invalidations_enabled_(false) {
#else
      sessions_invalidations_enabled_(true) {
#endif
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_client_);
  DCHECK(IsLocalSyncEnabled() || identity_manager_ != nullptr);

  // If Sync is disabled via command line flag, then ProfileSyncService
  // shouldn't be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());

  std::string last_version = sync_prefs_.GetLastRunVersion();
  std::string current_version = PRODUCT_VERSION;
  sync_prefs_.SetLastRunVersion(current_version);

  // Check for a major version change. Note that the versions have format
  // MAJOR.MINOR.BUILD.PATCH.
  if (last_version.substr(0, last_version.find('.')) !=
      current_version.substr(0, current_version.find('.'))) {
    passphrase_prompt_triggered_by_version_ = true;
  }

  startup_controller_ = std::make_unique<StartupController>(
      base::BindRepeating(&ProfileSyncService::GetPreferredDataTypes,
                          base::Unretained(this)),
      base::BindRepeating(&ProfileSyncService::IsEngineAllowedToRun,
                          base::Unretained(this)),
      base::BindRepeating(&ProfileSyncService::StartUpSlowEngineComponents,
                          base::Unretained(this)));

  sync_stopped_reporter_ = std::make_unique<SyncStoppedReporter>(
      sync_service_url_, MakeUserAgentForSync(channel_), url_loader_factory_,
      SyncStoppedReporter::ResultCallback());

  if (identity_manager_)
    identity_manager_->AddObserver(this);
}

ProfileSyncService::~ProfileSyncService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  DCHECK(!engine_);
}

void ProfileSyncService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(mastiz): The controllers map should be provided as argument.
  data_type_controllers_ =
      BuildDataTypeControllerMap(sync_client_->CreateDataTypeControllers(this));

  user_settings_ = std::make_unique<SyncUserSettingsImpl>(
      &crypto_, &sync_prefs_, sync_client_->GetPreferenceProvider(),
      GetRegisteredDataTypes(),
      base::BindRepeating(&ProfileSyncService::SyncAllowedByPlatformChanged,
                          base::Unretained(this)));

  sync_prefs_.AddSyncPrefObserver(this);

  if (!IsLocalSyncEnabled()) {
    auth_manager_->RegisterForAuthNotifications();
    if (invalidations_identity_provider_) {
      invalidations_identity_provider_->SetActiveAccountId(
          GetAuthenticatedAccountInfo().account_id);
    }

    SyncInvalidationsService* sync_invalidations_service =
        sync_client_->GetSyncInvalidationsService();
    if (sync_invalidations_service) {
      sync_invalidations_service->SetActive(IsSignedIn());
    }
  }

  // If sync is disabled permanently, clean up old data that may be around (e.g.
  // crash during signout).
  if (HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY) ||
      (HasDisableReason(DISABLE_REASON_NOT_SIGNED_IN) &&
       auth_manager_->IsActiveAccountInfoFullyLoaded())) {
    // TODO(crbug/1031162): Remove once traffic investigation is closed.
    EmitUmaMetricWithEmitTimeMinutes(
        "Sync.PeakAnalysis.StopOnSyncPermanentlyDisabled");
    StopImpl(CLEAR_DATA);
  }

  // Note: We need to record the initial state *after* calling
  // RegisterForAuthNotifications(), because before that the authenticated
  // account isn't initialized.
  RecordSyncInitialState(GetDisableReasons(),
                         user_settings_->IsFirstSetupComplete());

  // Auto-start means the first time the profile starts up, sync should start up
  // immediately. Since IsSyncRequested() is false by default and nobody else
  // will set it, we need to set it here.
  // Local Sync bypasses the IsSyncRequested() check, so no need to set it in
  // that case.
  // TODO(crbug.com/920158): Get rid of AUTO_START and remove this workaround.
  if (start_behavior_ == AUTO_START && !IsLocalSyncEnabled()) {
    user_settings_->SetSyncRequestedIfNotSetExplicitly();
  }
  bool force_immediate = (start_behavior_ == AUTO_START &&
                          !HasDisableReason(DISABLE_REASON_USER_CHOICE) &&
                          !user_settings_->IsFirstSetupComplete());
  startup_controller_->TryStart(force_immediate);
}

void ProfileSyncService::StartSyncingWithServer() {
  if (engine_)
    engine_->StartSyncingWithServer();

  if (IsLocalSyncEnabled()) {
    TriggerRefresh(Intersection(GetActiveDataTypes(), ProtocolTypes()));
  }
}

bool ProfileSyncService::IsDataTypeControllerRunningForTest(
    ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = data_type_controllers_.find(type);
  if (iter == data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

WeakHandle<JsEventHandler> ProfileSyncService::GetJsEventHandler() {
  return MakeWeakHandle(sync_js_controller_.AsWeakPtr());
}

void ProfileSyncService::AccountStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsSignedIn()) {
    // The account was signed out, so shut down.
    sync_disabled_by_admin_ = false;
    // TODO(crbug/1031162): Remove once traffic investigation is closed.
    EmitUmaMetricWithEmitTimeMinutes(
        "Sync.PeakAnalysis.StopAfterAccountStateChanged");
    StopImpl(CLEAR_DATA);
    DCHECK(!engine_);
  } else {
    // Either a new account was signed in, or the existing account's
    // |is_primary| bit was changed. Start up or reconfigure.
    if (!engine_) {
      // Note: We only get here after an actual sign-in (not during browser
      // startup with an existing signed-in account), so no need for deferred
      // startup.
      startup_controller_->TryStart(/*force_immediate=*/true);
    } else {
      ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
    }
  }

  // Propagate the (potentially) changed account ID to the invalidations system.
  if (invalidations_identity_provider_) {
    invalidations_identity_provider_->SetActiveAccountId(
        GetAuthenticatedAccountInfo().account_id);
  }

  SyncInvalidationsService* sync_invalidations_service =
      sync_client_->GetSyncInvalidationsService();
  if (sync_invalidations_service) {
    sync_invalidations_service->SetActive(IsSignedIn());
  }
}

void ProfileSyncService::CredentialsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't allowed to start anymore due to the credentials change,
  // then shut down. This happens when the user signs out on the web, i.e. we're
  // in the "Sync paused" state.
  if (!IsEngineAllowedToRun()) {
    // TODO(crbug/1031162): Remove once traffic investigation is closed.
    EmitUmaMetricWithEmitTimeMinutes(
        "Sync.PeakAnalysis.StopAfterCredentialsChanged");
    // This will notify observers if appropriate.
    StopImpl(KEEP_DATA);
    return;
  }

  if (!engine_) {
    startup_controller_->TryStart(/*force_immediate=*/true);
  } else {
    // If the engine already exists, just propagate the new credentials.
    SyncCredentials credentials = auth_manager_->GetCredentials();
    if (credentials.access_token.empty()) {
      engine_->InvalidateCredentials();
    } else {
      engine_->UpdateCredentials(credentials);
    }
  }

  NotifyObservers();
}

bool ProfileSyncService::IsEngineAllowedToRun() const {
  // USER_CHOICE (i.e. the Sync feature toggle) and PLATFORM_OVERRIDE (i.e.
  // Android's "MasterSync" toggle) do not prevent starting up the Sync
  // transport.
  auto disable_reasons = GetDisableReasons();
  disable_reasons.RemoveAll(SyncService::DisableReasonSet(
      DISABLE_REASON_USER_CHOICE, DISABLE_REASON_PLATFORM_OVERRIDE));
  return disable_reasons.Empty() && !IsInPausedState();
}

bool ProfileSyncService::IsInPausedState() const {
  return auth_manager_->IsSyncPaused() &&
         base::FeatureList::IsEnabled(switches::kStopSyncInPausedState);
}

void ProfileSyncService::OnProtocolEvent(const ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : protocol_event_observers_)
    observer.OnProtocolEvent(event);
}

void ProfileSyncService::OnDirectoryTypeCommitCounterUpdated(
    ModelType type,
    const CommitCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnCommitCountersUpdated(type, counters);
}

void ProfileSyncService::OnDirectoryTypeUpdateCounterUpdated(
    ModelType type,
    const UpdateCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnUpdateCountersUpdated(type, counters);
}

void ProfileSyncService::OnDatatypeStatusCounterUpdated(
    ModelType type,
    const StatusCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnStatusCountersUpdated(type, counters);
}

void ProfileSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(UserTypes().Has(type));

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << ModelTypeToString(type) << "not enabled.";
    return;
  }

  // If this is a data type change after a major version update, reset the
  // passphrase prompted state and notify observers.
  if (user_settings_->IsPassphraseRequired() &&
      passphrase_prompt_triggered_by_version_) {
    // The major version has changed and a local syncable change was made.
    // Reset the passphrase prompt state.
    passphrase_prompt_triggered_by_version_ = false;
    SetPassphrasePrompted(false);
    NotifyObservers();
  }

  if (engine_) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void ProfileSyncService::StartUpSlowEngineComponents() {
  DCHECK(IsEngineAllowedToRun());

  const CoreAccountInfo authenticated_account_info =
      GetAuthenticatedAccountInfo();

  if (IsLocalSyncEnabled()) {
    // With local sync (roaming profiles) there is no identity manager and hence
    // |authenticated_account_info| is empty. This is required for
    // IsLocalSyncTransportDataValid() to work properly.
    DCHECK(authenticated_account_info.gaia.empty());
    DCHECK(authenticated_account_info.account_id.empty());
  } else {
    // Except for local sync (roaming profiles), the user must be signed in for
    // sync to start.
    DCHECK(!authenticated_account_info.gaia.empty());
    DCHECK(!authenticated_account_info.account_id.empty());
  }

  engine_ = sync_client_->GetSyncApiComponentFactory()->CreateSyncEngine(
      debug_identifier_, sync_client_->GetInvalidationService(),
      sync_client_->GetSyncInvalidationsService(), sync_prefs_.AsWeakPtr());

  // Clear any old errors the first time sync starts.
  if (!user_settings_->IsFirstSetupComplete()) {
    last_actionable_error_ = SyncProtocolError();
  }

  // The gaia ID in SyncPrefs was introduced with M81, so having an empty value
  // is legitimate and should be populated as a one-off migration.
  // TODO(mastiz): Clean up this migration code after a grace period (e.g. 1
  // year).
  if (sync_prefs_.GetGaiaId().empty()) {
    sync_prefs_.SetGaiaId(authenticated_account_info.gaia);
  }

  if (!IsLocalSyncTransportDataValid(sync_prefs_, authenticated_account_info)) {
    // Either the local data is uninitialized or corrupt, so let's throw
    // everything away and start from scratch with a new cache GUID, which also
    // cascades into datatypes throwing away their dangling sync metadata due to
    // cache GUID mismatches.
    sync_prefs_.ClearLocalSyncTransportData();
    sync_prefs_.SetCacheGuid(GenerateCacheGUID());
    sync_prefs_.SetGaiaId(authenticated_account_info.gaia);
  }

  SyncEngine::InitParams params;
  params.sync_task_runner = backend_task_runner_;
  params.host = this;
  params.registrar = std::make_unique<SyncBackendRegistrar>(
      debug_identifier_,
      base::BindRepeating(&SyncClient::CreateModelWorkerForGroup,
                          base::Unretained(sync_client_.get())));
  params.encryption_observer_proxy = crypto_.GetEncryptionObserverProxy();

  params.extensions_activity = sync_client_->GetExtensionsActivity();
  params.event_handler = GetJsEventHandler();
  params.service_url = sync_service_url_;
  params.http_factory_getter = base::BindOnce(
      create_http_post_provider_factory_cb_, MakeUserAgentForSync(channel_),
      url_loader_factory_->Clone(), network_time_update_callback_);
  params.authenticated_account_id = authenticated_account_info.account_id;
  if (!base::FeatureList::IsEnabled(switches::kSyncE2ELatencyMeasurement)) {
    invalidation::InvalidationService* invalidator =
        sync_client_->GetInvalidationService();
    params.invalidator_client_id =
        invalidator ? invalidator->GetInvalidatorClientId() : std::string();
  }
  params.sync_manager_factory =
      std::make_unique<SyncManagerFactory>(network_connection_tracker_);
  if (sync_prefs_.IsLocalSyncEnabled()) {
    params.enable_local_sync_backend = true;
    params.local_sync_backend_folder =
        sync_client_->GetLocalSyncBackendFolder();
  }
  params.restored_key_for_bootstrapping =
      sync_prefs_.GetEncryptionBootstrapToken();
  params.restored_keystore_key_for_bootstrapping =
      sync_prefs_.GetKeystoreEncryptionBootstrapToken();
  params.cache_guid = sync_prefs_.GetCacheGuid();
  params.birthday = sync_prefs_.GetBirthday();
  params.bag_of_chips = sync_prefs_.GetBagOfChips();
  params.engine_components_factory =
      std::make_unique<EngineComponentsFactoryImpl>(
          EngineSwitchesFromCommandLine());
  params.invalidation_versions = sync_prefs_.GetInvalidationVersions();
  params.poll_interval = sync_prefs_.GetPollInterval();
  if (params.poll_interval.is_zero()) {
    params.poll_interval =
        base::TimeDelta::FromSeconds(kDefaultPollIntervalSeconds);
  }

  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionOpened();
  }

  engine_->Initialize(std::move(params));
}

void ProfileSyncService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyShutdown();
  ShutdownImpl(BROWSER_SHUTDOWN);

  DCHECK(!data_type_manager_);
  data_type_controllers_.clear();

  // All observers must be gone now: All KeyedServices should have unregistered
  // their observers already before, in their own Shutdown(), and all others
  // should have done it now when they got the shutdown notification.
  // Note: "might_have_observers" sounds like it might be inaccurate, but it can
  // only return false positives while an iteration over the ObserverList is
  // ongoing.
  DCHECK(!observers_.might_have_observers());

  auth_manager_.reset();
}

void ProfileSyncService::ShutdownImpl(ShutdownReason reason) {
  if (!engine_) {
    // If the engine hasn't started or is already shut down when a DISABLE_SYNC
    // happens, the Directory needs to be cleaned up here.
    if (reason == ShutdownReason::DISABLE_SYNC) {
      // Clearing the Directory via DeleteLegacyDirectoryFilesAndNigoriStorage()
      // means there's IO involved which may we considerable overhead if
      // triggered consistently upon browser startup (which is the case for
      // certain codepaths such as the user being signed out). To avoid that,
      // SyncPrefs is used to determine whether it's worth it.
      if (!sync_prefs_.GetCacheGuid().empty()) {
        backend_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&DeleteLegacyDirectoryFilesAndNigoriStorage,
                           sync_client_->GetSyncDataPath()));
      }
      sync_prefs_.ClearLocalSyncTransportData();
    }
    return;
  }

  if (reason == ShutdownReason::STOP_SYNC ||
      reason == ShutdownReason::DISABLE_SYNC) {
    RemoveClientFromServer();
  }

  // First, we spin down the engine to stop change processing as soon as
  // possible.
  engine_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed. Note that until Stop completes,
  // it is possible in theory to have a ChangeProcessor apply a change from a
  // native model. In that case, it will get applied to the sync database (which
  // doesn't get destroyed until we destroy the engine below) as an unsynced
  // change. That will be persisted, and committed on restart.
  if (data_type_manager_) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop(reason);
    }
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the engine to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<JsBackend>());

  engine_->Shutdown(reason);
  engine_.reset();

  sync_enabled_weak_factory_.InvalidateWeakPtrs();

  startup_controller_->Reset();

  // Clear various state.
  crypto_.Reset();
  expect_sync_configuration_aborted_ = false;
  last_snapshot_ = SyncCycleSnapshot();

  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionClosed();
  }

  if (reason == ShutdownReason::DISABLE_SYNC) {
    sync_prefs_.ClearLocalSyncTransportData();
  }

  NotifyObservers();
}

void ProfileSyncService::StopImpl(SyncStopDataFate data_fate) {
  switch (data_fate) {
    case KEEP_DATA:
      ShutdownImpl(STOP_SYNC);
      break;
    case CLEAR_DATA:
      ClearUnrecoverableError();
      ShutdownImpl(DISABLE_SYNC);
      // Note: ShutdownImpl(DISABLE_SYNC) does *not* clear prefs which are
      // directly user-controlled such as the set of selected types here, so
      // that if the user ever chooses to enable Sync again, they start off
      // with their previous settings by default. We do however require going
      // through first-time setup again.
      sync_prefs_.ClearFirstSetupComplete();
      // For explicit passphrase users, clear the encryption key, such that they
      // will need to reenter it if sync gets re-enabled.
      sync_prefs_.ClearEncryptionBootstrapToken();
      // Also let observers know that Sync-the-feature is now fully disabled
      // (before it possibly starts up again in transport-only mode).
      NotifyObservers();
      break;
  }
}

SyncUserSettings* ProfileSyncService::GetUserSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_.get();
}

const SyncUserSettings* ProfileSyncService::GetUserSettings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_.get();
}

SyncService::DisableReasonSet ProfileSyncService::GetDisableReasons() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If Sync is disabled via command line flag, then ProfileSyncService
  // shouldn't even be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());
  DisableReasonSet result;
  if (!user_settings_->IsSyncAllowedByPlatform()) {
    result.Put(DISABLE_REASON_PLATFORM_OVERRIDE);
  }
  if (sync_prefs_.IsManaged() || sync_disabled_by_admin_) {
    result.Put(DISABLE_REASON_ENTERPRISE_POLICY);
  }
  // Local sync doesn't require sign-in.
  if (!IsSignedIn() && !IsLocalSyncEnabled()) {
    result.Put(DISABLE_REASON_NOT_SIGNED_IN);
  }
  // When local sync is on sync should be considered requsted or otherwise it
  // will not resume after the policy or the flag has been removed.
  if (!user_settings_->IsSyncRequested() && !IsLocalSyncEnabled()) {
    result.Put(DISABLE_REASON_USER_CHOICE);
  }
  if (unrecoverable_error_reason_ != ERROR_REASON_UNSET) {
    result.Put(DISABLE_REASON_UNRECOVERABLE_ERROR);
  }
  return result;
}

SyncService::TransportState ProfileSyncService::GetTransportState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEngineAllowedToRun()) {
    // We generally shouldn't have an engine while in a disabled state, but it
    // can happen if this method gets called during ShutdownImpl().
    return IsInPausedState() ? TransportState::PAUSED
                             : TransportState::DISABLED;
  }

  if (!engine_ || !engine_->IsInitialized()) {
    switch (startup_controller_->GetState()) {
        // TODO(crbug.com/935523): If the engine is allowed to start, then we
        // should generally have kicked off the startup process already, so
        // NOT_STARTED should be impossible. But we can temporarily be in this
        // state between shutting down and starting up again (e.g. during the
        // NotifyObservers() call in ShutdownImpl()).
      case StartupController::State::NOT_STARTED:
      case StartupController::State::STARTING_DEFERRED:
        DCHECK(!engine_);
        return TransportState::START_DEFERRED;
      case StartupController::State::STARTED:
        DCHECK(engine_);
        return TransportState::INITIALIZING;
    }
    NOTREACHED();
  }
  DCHECK(engine_);
  // The DataTypeManager gets created once the engine is initialized.
  DCHECK(data_type_manager_);

  // At this point we should usually be able to configure our data types (and
  // once the data types can be configured, they must actually get configured).
  // However, if the initial setup hasn't been completed, then we can't
  // configure the data types. Also if a later (non-initial) setup happens to be
  // in progress, we won't configure them right now.
  if (data_type_manager_->state() == DataTypeManager::STOPPED) {
    DCHECK(!CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false));
    return TransportState::PENDING_DESIRED_CONFIGURATION;
  }

  // Note that if a setup is started after the data types have been configured,
  // then they'll stay configured even though CanConfigureDataTypes will be
  // false.
  DCHECK(CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false) ||
         IsSetupInProgress());

  if (data_type_manager_->state() != DataTypeManager::CONFIGURED) {
    return TransportState::CONFIGURING;
  }

  return TransportState::ACTIVE;
}

void ProfileSyncService::UpdateLastSyncedTime() {
  sync_prefs_.SetLastSyncedTime(base::Time::Now());
}

void ProfileSyncService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnStateChanged(this);
  }
}

void ProfileSyncService::NotifySyncCycleCompleted() {
  for (auto& observer : observers_)
    observer.OnSyncCycleCompleted(this);
}

void ProfileSyncService::NotifyShutdown() {
  for (auto& observer : observers_)
    observer.OnSyncShutdown(this);
}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = base::Location();
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const base::Location& from_here,
    const std::string& message,
    UnrecoverableErrorReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(reason, ERROR_REASON_UNSET);
  unrecoverable_error_reason_ = reason;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  LOG(ERROR) << "Unrecoverable error detected at " << from_here.ToString()
             << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  ShutdownImpl(DISABLE_SYNC);
}

void ProfileSyncService::DataTypePreconditionChanged(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !engine_->IsInitialized() || !data_type_manager_)
    return;
  data_type_manager_->DataTypePreconditionChanged(type);
}

void ProfileSyncService::UpdateEngineInitUMA(bool success) const {
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  base::Time on_engine_initialized_time = base::Time::Now();
  base::TimeDelta delta =
      on_engine_initialized_time - startup_controller_->start_engine_time();
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
  }
}

void ProfileSyncService::OnEngineInitialized(
    ModelTypeSet initial_types,
    const WeakHandle<JsBackend>& js_backend,
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    const std::string& birthday,
    const std::string& bag_of_chips,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(treib): Based on some crash reports, it seems like the user could have
  // signed out already at this point, so many of the steps below, including
  // datatype reconfiguration, should not be triggered.
  DCHECK(IsEngineAllowedToRun());

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  LastSyncedTime will only be null in
  // this case, because the pref wasn't restored on StartUp.
  is_first_time_sync_configure_ = sync_prefs_.GetLastSyncedTime().is_null();

  UpdateEngineInitUMA(success);

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    OnUnrecoverableErrorImpl(FROM_HERE, "BackendInitialize failure",
                             ERROR_REASON_ENGINE_INIT_FAILURE);
    return;
  }

  sync_js_controller_.AttachJsBackend(js_backend);

  // Save initialization data to preferences.
  sync_prefs_.SetBirthday(birthday);
  sync_prefs_.SetBagOfChips(bag_of_chips);

  if (protocol_event_observers_.might_have_observers()) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  if (type_debug_info_observers_.might_have_observers()) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }

  if (is_first_time_sync_configure_) {
    UpdateLastSyncedTime();
  }

  data_type_manager_ =
      sync_client_->GetSyncApiComponentFactory()->CreateDataTypeManager(
          initial_types, debug_info_listener, &data_type_controllers_, &crypto_,
          engine_.get(), this);

  crypto_.SetSyncEngine(GetAuthenticatedAccountInfo(), engine_.get());

  // Auto-start means IsFirstSetupComplete gets set automatically.
  if (start_behavior_ == AUTO_START &&
      !user_settings_->IsFirstSetupComplete()) {
    // This will trigger a configure if it completes setup.
    user_settings_->SetFirstSetupComplete(
        SyncFirstSetupCompleteSource::ENGINE_INITIALIZED_WITH_AUTO_START);
  } else if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    // Datatype downloads on restart are generally due to newly supported
    // datatypes (although it's also possible we're picking up where a failed
    // previous configuration left off).
    // TODO(sync): consider detecting configuration recovery and setting
    // the reason here appropriately.
    ConfigureDataTypeManager(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE);
  }

  // Check for a cookie jar mismatch.
  if (identity_manager_) {
    signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
        identity_manager_->GetAccountsInCookieJar();
    if (accounts_in_cookie_jar_info.accounts_are_fresh) {
      OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                GoogleServiceAuthError::AuthErrorNone());
    }
  }

  NotifyObservers();
}

void ProfileSyncService::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_snapshot_ = snapshot;

  UpdateLastSyncedTime();
  if (!snapshot.poll_finish_time().is_null())
    sync_prefs_.SetLastPollTime(snapshot.poll_finish_time());
  DCHECK(!snapshot.poll_interval().is_zero());
  sync_prefs_.SetPollInterval(snapshot.poll_interval());

  sync_prefs_.SetBagOfChips(snapshot.bag_of_chips());

  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void ProfileSyncService::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionStatusChanged(status);
  }
  NotifyObservers();
}

void ProfileSyncService::OnMigrationNeededForTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_);
  DCHECK(engine_->IsInitialized());
  DCHECK(data_type_manager_);

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action, UNKNOWN_ACTION);
  switch (error.action) {
    case UPGRADE_CLIENT:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (IsSetupInProgress()) {
        StopImpl(CLEAR_DATA);
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnUnrecoverableErrorImpl(FROM_HERE,
                               last_actionable_error_.error_description,
                               ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case DISABLE_SYNC_ON_CLIENT:
      if (error.error_type == NOT_MY_BIRTHDAY) {
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", BIRTHDAY_ERROR,
                                  STOP_SOURCE_LIMIT);
      }
      // Note: Here we explicitly want StopAndClear (rather than StopImpl), so
      // that IsSyncRequested gets set to false, and Sync won't start again on
      // the next browser startup.
      StopAndClear();
#if !defined(OS_CHROMEOS)
      // On every platform except ChromeOS, sign out the user after a dashboard
      // clear.
      if (!IsLocalSyncEnabled()) {
        auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

        // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
        DCHECK(account_mutator);
        account_mutator->ClearPrimaryAccount(
            signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
            signin_metrics::SERVER_FORCED_DISABLE,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
      }
#endif
      break;
    case STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. we should stop syncing until next
      // restart.
      sync_disabled_by_admin_ = true;
      ShutdownImpl(DISABLE_SYNC);
      break;
    case RESET_LOCAL_SYNC_DATA:
      ShutdownImpl(DISABLE_SYNC);
      startup_controller_->TryStart(/*force_immediate=*/true);
      break;
    case UNKNOWN_ACTION:
      NOTREACHED();
  }
  NotifyObservers();
}

void ProfileSyncService::OnBackedOffTypesChanged() {
  NotifyObservers();
}

void ProfileSyncService::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_type_error_map_ = result.data_type_status_table.GetAllErrors();

  DVLOG(1) << "PSS OnConfigureDone called with status: " << result.status;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.
  //    Everything else is an UnrecoverableError. So treat it as such.

  // First handle the abort case.
  if (result.status == DataTypeManager::ABORTED &&
      expect_sync_configuration_aborted_) {
    DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  // Handle unrecoverable error.
  if (result.status != DataTypeManager::OK) {
    // Something catastrophic had happened. We should only have one
    // error representing it.
    SyncError error = result.data_type_status_table.GetUnrecoverableError();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(result.status) +
        " caused by " +
        ModelTypeSetToString(
            result.data_type_status_table.GetUnrecoverableErrorTypes()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: " << message;
    OnUnrecoverableErrorImpl(error.location(), message,
                             ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  DCHECK_EQ(DataTypeManager::OK, result.status);

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(!user_settings_->IsPassphraseRequiredForPreferredDataTypes() ||
         user_settings_->IsEncryptedDatatypeEnabled());

  // Notify listeners that configuration is done.
  for (auto& observer : observers_)
    observer.OnSyncConfigurationCompleted(this);

  // This must be done before we start syncing with the server to avoid
  // sending unencrypted data up on a first time sync.
  if (user_settings_->IsEncryptionPending())
    engine_->EnableEncryptEverything();
  NotifyObservers();

  if (migrator_.get() && migrator_->state() != BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
    return;
  }

  RecordMemoryUsageAndCountsHistograms();

  StartSyncingWithServer();
}

void ProfileSyncService::OnConfigureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  engine_->StartConfiguration();
  NotifyObservers();
}

bool ProfileSyncService::IsSetupInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return outstanding_setup_in_progress_handles_ > 0;
}

bool ProfileSyncService::QueryDetailedSyncStatusForDebugging(
    SyncStatus* result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    *result = engine_->GetDetailedStatus();
    return true;
  }
  SyncStatus status;
  status.sync_protocol_error = last_actionable_error_;
  *result = status;
  return false;
}

GoogleServiceAuthError ProfileSyncService::GetAuthError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetLastAuthError();
}

base::Time ProfileSyncService::GetAuthErrorTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetLastAuthErrorTime();
}

bool ProfileSyncService::RequiresClientUpgrade() const {
  return last_actionable_error_.action == UPGRADE_CLIENT;
}

bool ProfileSyncService::CanConfigureDataTypes(
    bool bypass_setup_in_progress_check) const {
  // TODO(crbug.com/856179): Arguably, IsSetupInProgress() shouldn't prevent
  // configuring data types in transport mode, but at least for now, it's
  // easier to keep it like this. Changing this will likely require changes to
  // the setup UI flow.
  return data_type_manager_ &&
         (bypass_setup_in_progress_check || !IsSetupInProgress());
}

std::unique_ptr<SyncSetupInProgressHandle>
ProfileSyncService::GetSetupInProgressHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (++outstanding_setup_in_progress_handles_ == 1) {
    startup_controller_->TryStart(/*force_immediate=*/true);

    NotifyObservers();
  }

  return std::make_unique<SyncSetupInProgressHandle>(
      base::BindRepeating(&ProfileSyncService::OnSetupInProgressHandleDestroyed,
                          weak_factory_.GetWeakPtr()));
}

bool ProfileSyncService::IsLocalSyncEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_prefs_.IsLocalSyncEnabled();
}

void ProfileSyncService::TriggerRefresh(const ModelTypeSet& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    engine_->TriggerRefresh(types);
  }
}

bool ProfileSyncService::IsSignedIn() const {
  // Sync is logged in if there is a non-empty account id.
  return !GetAuthenticatedAccountInfo().account_id.empty();
}

base::Time ProfileSyncService::GetLastSyncedTimeForDebugging() const {
  return sync_prefs_.GetLastSyncedTime();
}

void ProfileSyncService::OnPreferredDataTypesPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!engine_ && !HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    return;
  }

  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();

  ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
}

SyncClient* ProfileSyncService::GetSyncClientForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_client_.get();
}

// static
std::string ProfileSyncService::GenerateCacheGUIDForTest() {
  return GenerateCacheGUID();
}

void ProfileSyncService::AddObserver(SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool ProfileSyncService::HasObserver(
    const SyncServiceObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ModelTypeSet registered_types;
  // The |data_type_controllers_| are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (const std::pair<const ModelType, std::unique_ptr<DataTypeController>>&
           type_and_controller : data_type_controllers_) {
    registered_types.Put(type_and_controller.first);
  }
  return registered_types;
}

ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_->GetPreferredDataTypes();
}

ModelTypeSet ProfileSyncService::GetActiveDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_type_manager_ || GetAuthError().IsPersistentError())
    return ModelTypeSet();
  return data_type_manager_->GetActiveDataTypes();
}

ModelTypeSet ProfileSyncService::GetBackedOffDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    return engine_->GetDetailedStatus().backed_off_types;
  }
  return ModelTypeSet();
}

void ProfileSyncService::SyncAllowedByPlatformChanged(bool allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!allowed) {
    StopImpl(KEEP_DATA);
    // Try to start up again (in transport-only mode).
    // TODO(crbug.com/856179): Evaluate whether we can get away without a full
    // restart (i.e. just reconfigure). See also similar comment in
    // OnSyncRequestedPrefChange().
    startup_controller_->TryStart(/*force_immediate=*/true);
  }
}

void ProfileSyncService::ConfigureDataTypeManager(ConfigureReason reason) {
  ConfigureContext configure_context;
  configure_context.authenticated_account_id =
      GetAuthenticatedAccountInfo().account_id;
  configure_context.cache_guid = sync_prefs_.GetCacheGuid();
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.reason = reason;
  configure_context.configuration_start_time = base::Time::Now();

  DCHECK(!configure_context.cache_guid.empty());

  if (!migrator_) {
    // We create the migrator at the same time.
    migrator_ = std::make_unique<BackendMigrator>(
        debug_identifier_, data_type_manager_.get(),
        base::BindRepeating(&ProfileSyncService::ConfigureDataTypeManager,
                            base::Unretained(this), CONFIGURE_REASON_MIGRATION),
        base::BindRepeating(&ProfileSyncService::StartSyncingWithServer,
                            base::Unretained(this)));

    // Override reason if no configuration has completed ever.
    if (is_first_time_sync_configure_) {
      configure_context.reason = CONFIGURE_REASON_NEW_CLIENT;
    }
  }

  DCHECK(!configure_context.authenticated_account_id.empty() ||
         IsLocalSyncEnabled());
  DCHECK(!configure_context.cache_guid.empty());
  DCHECK_NE(configure_context.reason, CONFIGURE_REASON_UNKNOWN);

  const bool use_transport_only_mode = UseTransportOnlyMode();

  if (use_transport_only_mode) {
    configure_context.sync_mode = SyncMode::kTransportOnly;
  }
  data_type_manager_->Configure(GetDataTypesToConfigure(), configure_context);

  UpdateDataTypesForInvalidations();

  // Record in UMA whether we're configuring the full Sync feature or only the
  // transport.
  enum class ConfigureDataTypeManagerOption {
    kFeature = 0,
    kTransport = 1,
    kMaxValue = kTransport
  };
  UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureDataTypeManagerOption",
                            use_transport_only_mode
                                ? ConfigureDataTypeManagerOption::kTransport
                                : ConfigureDataTypeManagerOption::kFeature);

  // Only if it's the full Sync feature, also record the user's choice of data
  // types.
  if (!use_transport_only_mode) {
    bool sync_everything = sync_prefs_.HasKeepEverythingSynced();
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything2", sync_everything);

    if (!sync_everything) {
      for (UserSelectableType type : user_settings_->GetSelectedTypes()) {
        UMA_HISTOGRAM_ENUMERATION("Sync.CustomSync2",
                                  UserSelectableTypeToHistogramInt(type),
                                  UserSelectableTypeHistogramNumEntries());
      }
    }
  }
}

bool ProfileSyncService::UseTransportOnlyMode() const {
  // Note: When local Sync is enabled, then we want full-sync mode (not just
  // transport), even though Sync-the-feature is not considered enabled.
  return !IsSyncFeatureEnabled() && !IsLocalSyncEnabled();
}

ModelTypeSet ProfileSyncService::GetModelTypesForTransportOnlyMode() const {
  ModelTypeSet allowed_types = {
      DEVICE_INFO,
      SECURITY_EVENTS,
      SHARING_MESSAGE,
      SUPERVISED_USER_SETTINGS,
      SUPERVISED_USER_ALLOWLISTS,
      USER_CONSENTS,
  };

  if (autofill_enable_account_wallet_storage_) {
    if (!GetUserSettings()->IsUsingSecondaryPassphrase() ||
        base::FeatureList::IsEnabled(
            switches::
                kSyncAllowWalletDataInTransportModeWithCustomPassphrase)) {
      allowed_types.Put(AUTOFILL_WALLET_DATA);
    }
  }

  if (enable_passwords_account_storage_ &&
      !GetUserSettings()->IsUsingSecondaryPassphrase()) {
    allowed_types.Put(PASSWORDS);
  }

  // Outside the #if so non-Chrome OS developers will hit it before uploading.
  static_assert(41 == ModelType::NUM_ENTRIES,
                "If a new ModelType is Chrome OS-only and uses OS sync "
                "consent, add it below.");
#if defined(OS_CHROMEOS)
  // Chrome OS system types are not tied to browser sync-the-feature.
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    allowed_types.PutAll({APP_LIST, APP_SETTINGS, APPS, ARC_PACKAGE,
                          OS_PREFERENCES, OS_PRIORITY_PREFERENCES, PRINTERS,
                          WEB_APPS, WIFI_CONFIGURATIONS});
  }
#endif  // defined(OS_CHROMEOS)

  return allowed_types;
}

ModelTypeSet ProfileSyncService::GetDataTypesToConfigure() const {
  ModelTypeSet types = GetPreferredDataTypes();
  // In transport-only mode, only a subset of data types is supported.
  if (UseTransportOnlyMode()) {
    types = Intersection(types, GetModelTypesForTransportOnlyMode());
  }
  return types;
}

void ProfileSyncService::UpdateDataTypesForInvalidations() {
  SyncInvalidationsService* invalidations_service =
      sync_client_->GetSyncInvalidationsService();
  if (!invalidations_service) {
    return;
  }

  // No need to register invalidations for non-protocol or commit-only types.
  ModelTypeSet types = Intersection(GetDataTypesToConfigure(), ProtocolTypes());
  types.RemoveAll(CommitOnlyTypes());
  if (!sessions_invalidations_enabled_) {
    types.Remove(SESSIONS);
  }
  if (!(base::FeatureList::IsEnabled(switches::kUseSyncInvalidations) &&
        base::FeatureList::IsEnabled(
            switches::kUseSyncInvalidationsForWalletAndOffer))) {
    types.RemoveAll({AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});
  }
  invalidations_service->SetInterestedDataTypes(
      types, base::BindRepeating(&ProfileSyncService::TriggerRefresh,
                                 sync_enabled_weak_factory_.GetWeakPtr()));
}

SyncCycleSnapshot ProfileSyncService::GetLastCycleSnapshotForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_snapshot_;
}

void ProfileSyncService::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_);
  DCHECK(engine_->IsInitialized());
  engine_->HasUnsyncedItemsForTest(std::move(cb));
}

BackendMigrator* ProfileSyncService::GetBackendMigratorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return migrator_.get();
}

std::unique_ptr<base::Value>
ProfileSyncService::GetTypeStatusMapForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = std::make_unique<base::ListValue>();

  if (!engine_ || !engine_->IsInitialized()) {
    return std::move(result);
  }

  const SyncStatus& detailed_status = engine_->GetDetailedStatus();
  const ModelTypeSet& throttled_types(detailed_status.throttled_types);
  const ModelTypeSet& backed_off_types(detailed_status.backed_off_types);

  std::unique_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());
  type_status_header->SetString("status", "header");
  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  type_status_header->SetString("message", "Message");
  type_status_header->SetString("state", "State");
  type_status_header->SetString("group_type", "Group Type");
  result->Append(std::move(type_status_header));

  ModelSafeRoutingInfo routing_info;
  engine_->GetModelSafeRoutingInfo(&routing_info);
  const ModelTypeSet registered = GetRegisteredDataTypes();
  for (ModelType type : registered) {
    auto type_status = std::make_unique<base::DictionaryValue>();
    type_status->SetString("name", ModelTypeToString(type));
    type_status->SetString("group_type",
                           ModelSafeGroupToString(routing_info[type]));

    if (data_type_error_map_.find(type) != data_type_error_map_.end()) {
      const SyncError& error = data_type_error_map_.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case SyncError::SYNC_ERROR_SEVERITY_ERROR:
          type_status->SetString("status", "error");
          type_status->SetString(
              "message", "Error: " + error.location().ToString() + ", " +
                             error.GetMessagePrefix() + error.message());
          break;
        case SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "disabled");
          type_status->SetString("message", error.message());
          break;
      }
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", " Throttled");
    } else if (backed_off_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Backed off");
    } else if (routing_info.find(type) != routing_info.end()) {
      type_status->SetString("status", "ok");
      type_status->SetString("message", "");
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Disabled by User");
    }

    const auto& dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      type_status->SetString("state", DataTypeController::StateToString(
                                          dtc_iter->second->state()));
      if (dtc_iter->second->state() != DataTypeController::NOT_RUNNING) {
        // We use BindToCurrentSequence() to make sure observers (i.e.
        // |type_debug_info_observers_|) are not notified synchronously, which
        // the UI code (chrome://sync-internals) doesn't handle well.
        dtc_iter->second->GetStatusCounters(
            BindToCurrentSequence(base::BindRepeating(
                &ProfileSyncService::OnDatatypeStatusCounterUpdated,
                base::Unretained(this))));
      }
    }

    result->Append(std::move(type_status));
  }
  return std::move(result);
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_sync_managed) {
    // TODO(crbug/1031162): Remove once traffic investigation is closed.
    EmitUmaMetricWithEmitTimeMinutes(
        "Sync.PeakAnalysis.StopOnSyncManagedPrefChange");
    StopImpl(CLEAR_DATA);
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    DCHECK(!engine_);
    startup_controller_->TryStart(/*force_immediate=*/true);
  }
}

void ProfileSyncService::OnFirstSetupCompletePrefChange(
    bool is_first_setup_complete) {
  if (engine_ && engine_->IsInitialized()) {
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
  }
}

void ProfileSyncService::OnSyncRequestedPrefChange(bool is_sync_requested) {
  if (is_sync_requested) {
    // If the Sync engine was already initialized (probably running in transport
    // mode), just reconfigure.
    if (engine_ && engine_->IsInitialized()) {
      ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
    } else {
      // Otherwise try to start up. Note that there might still be other disable
      // reasons remaining, in which case this will effectively do nothing.
      startup_controller_->TryStart(/*force_immediate=*/true);
    }

    NotifyObservers();
  } else {
    // This will notify the observers.
    if (is_stopping_and_clearing_) {
      is_stopping_and_clearing_ = false;
      StopImpl(CLEAR_DATA);
    } else {
      // TODO(crbug.com/856179): Evaluate whether we can get away without a
      // full restart in this case (i.e. just reconfigure).
      StopImpl(KEEP_DATA);
    }

    // Try to start up again (in transport-only mode).
    // TODO(crbug.com/1035874): There's no real need to delay the startup here,
    // i.e. it should be fine to set force_immediate to true. However currently
    // some tests depend on the startup *not* happening immediately (because
    // they want to check that Sync (the feature) got disabled, which is hard to
    // do if the engine starts up again immediately).
    startup_controller_->TryStart(/*force_immediate=*/false);
  }
}

void ProfileSyncService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  OnAccountsInCookieUpdatedWithCallback(
      accounts_in_cookie_jar_info.signed_in_accounts, base::NullCallback());
}

void ProfileSyncService::OnAccountsCookieDeletedByUserAction() {
  sync_client_->GetTrustedVaultClient()->RemoveAllStoredKeys();
}

void ProfileSyncService::OnAccountsInCookieUpdatedWithCallback(
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !engine_->IsInitialized())
    return;

  bool cookie_jar_mismatch = HasCookieJarMismatch(signed_in_accounts);
  bool cookie_jar_empty = signed_in_accounts.empty();

  DVLOG(1) << "Cookie jar mismatch: " << cookie_jar_mismatch;
  DVLOG(1) << "Cookie jar empty: " << cookie_jar_empty;
  engine_->OnCookieJarChanged(cookie_jar_mismatch, cookie_jar_empty,
                              std::move(callback));
}

bool ProfileSyncService::HasCookieJarMismatch(
    const std::vector<gaia::ListedAccount>& cookie_jar_accounts) {
  CoreAccountId account_id = GetAuthenticatedAccountInfo().account_id;
  // Iterate through list of accounts, looking for current sync account.
  for (const auto& account : cookie_jar_accounts) {
    if (account.id == account_id)
      return false;
  }
  return true;
}

void ProfileSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.AddObserver(observer);
  if (engine_) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void ProfileSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.RemoveObserver(observer);
  if (engine_ && !protocol_event_observers_.might_have_observers()) {
    engine_->DisableProtocolEventForwarding();
  }
}

void ProfileSyncService::AddTypeDebugInfoObserver(
    TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_debug_info_observers_.AddObserver(type_debug_info_observer);
  if (type_debug_info_observers_.might_have_observers() && engine_ &&
      engine_->IsInitialized()) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::RemoveTypeDebugInfoObserver(
    TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_debug_info_observers_.RemoveObserver(type_debug_info_observer);
  if (!type_debug_info_observers_.might_have_observers() && engine_ &&
      engine_->IsInitialized()) {
    engine_->DisableDirectoryTypeDebugInfoForwarding();
  }
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      ModelTypeSet requested_types,
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback);

  void OnReceivedNodesForType(const ModelType type,
                              std::unique_ptr<base::ListValue> node_list);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  std::unique_ptr<base::ListValue> result_accumulator_;
  ModelTypeSet awaiting_types_;
  base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(GetAllNodesRequestHelper);
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    ModelTypeSet requested_types,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback)
    : result_accumulator_(std::make_unique<base::ListValue>()),
      awaiting_types_(requested_types),
      callback_(std::move(callback)) {}

GetAllNodesRequestHelper::~GetAllNodesRequestHelper() {
  if (!awaiting_types_.Empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << ModelTypeSetToString(awaiting_types_);
  }
}

// Called when the set of nodes for a type has been returned.
// Only return one type of nodes each time.
void GetAllNodesRequestHelper::OnReceivedNodesForType(
    const ModelType type,
    std::unique_ptr<base::ListValue> node_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Add these results to our list.
  base::DictionaryValue type_dict;
  type_dict.SetKey("type", base::Value(ModelTypeToString(type)));
  type_dict.SetKey("nodes",
                   base::Value::FromUniquePtrValue(std::move(node_list)));
  result_accumulator_->Append(std::move(type_dict));

  // Remember that this part of the request is satisfied.
  awaiting_types_.Remove(type);

  if (awaiting_types_.Empty()) {
    std::move(callback_).Run(std::move(result_accumulator_));
  }
}

}  // namespace

void ProfileSyncService::GetAllNodesForDebugging(
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't initialized yet, then there are no nodes to return.
  if (!engine_ || !engine_->IsInitialized()) {
    std::move(callback).Run(std::make_unique<base::ListValue>());
    return;
  }

  ModelTypeSet all_types = GetActiveDataTypes();
  all_types.PutAll(ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(all_types, std::move(callback));

  for (ModelType type : all_types) {
    const auto dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      if (dtc_iter->second->state() == DataTypeController::NOT_RUNNING) {
        // In the NOT_RUNNING state it's not allowed to call GetAllNodes on the
        // DataTypeController, so just return an empty result.
        // This can happen e.g. if we're waiting for a custom passphrase to be
        // entered - the data types are already considered active in this case,
        // but their DataTypeControllers are still NOT_RUNNING.
        helper->OnReceivedNodesForType(type,
                                       std::make_unique<base::ListValue>());
      } else {
        dtc_iter->second->GetAllNodes(base::BindRepeating(
            &GetAllNodesRequestHelper::OnReceivedNodesForType, helper));
      }
    } else {
      // We should have no data type controller only for Nigori.
      DCHECK_EQ(type, NIGORI);
      engine_->GetNigoriNodeForDebugging(base::BindOnce(
          &GetAllNodesRequestHelper::OnReceivedNodesForType, helper));
    }
  }
}

CoreAccountInfo ProfileSyncService::GetAuthenticatedAccountInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!auth_manager_) {
    // Some crashes on iOS (crbug.com/962384) suggest that ProfileSyncService
    // gets called after it has been already shutdown. It's not clear why this
    // actually happens. We add this null check here to protect against such
    // crashes.
    return CoreAccountInfo();
  }
  return auth_manager_->GetActiveAccountInfo().account_info;
}

bool ProfileSyncService::IsAuthenticatedAccountPrimary() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!auth_manager_) {
    // This is a precautionary check to be consistent with the check in
    // GetAuthenticatedAccountInfo().
    return false;
  }
  return auth_manager_->GetActiveAccountInfo().is_primary;
}

void ProfileSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    engine_->SetInvalidationsForSessionsEnabled(enabled);
  }

  sessions_invalidations_enabled_ = enabled;
  UpdateDataTypesForInvalidations();
}

void ProfileSyncService::AddTrustedVaultDecryptionKeysFromWeb(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  sync_client_->GetTrustedVaultClient()->StoreKeys(gaia_id, keys,
                                                   last_key_version);
}

UserDemographicsResult ProfileSyncService::GetUserNoisedBirthYearAndGender(
    base::Time now) {
  // Do not provide the synced user’s birth year and gender when sync is
  // disabled or paused because the user’s birth year and gender should only be
  // provided when the sync prefs are synced with the sync server.
  if (!IsSyncFeatureEnabled() || auth_manager_->IsSyncPaused()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kSyncNotEnabled);
  }

  return sync_prefs_.GetUserNoisedBirthYearAndGender(now);
}

base::WeakPtr<JsController> ProfileSyncService::GetJsController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_js_controller_.AsWeakPtr();
}

void ProfileSyncService::StopAndClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This can happen if the user had disabled sync before and is now setting up
  // sync again but hits the "Cancel" button on the confirmation dialog.
  // TODO(crbug.com/906034): Maybe we can streamline the defaults and the
  // behavior on setting up sync so that either this whole early return goes
  // away or it treats all "Cancel the confirmation" cases?
  if (!user_settings_->IsSyncRequested()) {
    StopImpl(CLEAR_DATA);
    // Try to start up again (in transport-only mode).
    startup_controller_->TryStart(/*force_immediate=*/true);
    return;
  }

  // We need to remember that clearing of data is needed when sync will be
  // stopped. This flag is cleared in OnSyncRequestedPrefChange() where sync
  // gets stopped. This happens synchronously when |user_settings_| get changed
  // below.
  DCHECK(!is_stopping_and_clearing_);
  is_stopping_and_clearing_ = true;
  user_settings_->SetSyncRequested(false);
  DCHECK(!is_stopping_and_clearing_);
}

void ProfileSyncService::ReconfigureDatatypeManager(
    bool bypass_setup_in_progress_check) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    DCHECK(engine_);
    // Don't configure datatypes if the setup UI is still on the screen - this
    // is to help multi-screen setting UIs (like iOS) where they don't want to
    // start syncing data until the user is done configuring encryption options,
    // etc. ReconfigureDatatypeManager() will get called again once the last
    // SyncSetupInProgressHandle is released.
    if (CanConfigureDataTypes(bypass_setup_in_progress_check)) {
      ConfigureDataTypeManager(CONFIGURE_REASON_RECONFIGURATION);
    } else {
      DVLOG(0) << "ConfigureDataTypeManager not invoked because datatypes "
               << "cannot be configured now";
      // If we can't configure the data type manager yet, we should still notify
      // observers. This is to support multiple setup UIs being open at once.
      NotifyObservers();
    }
  } else if (HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because engine is not "
             << "initialized";
  }
}

bool ProfileSyncService::IsRetryingAccessTokenFetchForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->IsRetryingAccessTokenFetchForTest();
}

std::string ProfileSyncService::GetAccessTokenForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->access_token();
}

SyncTokenStatus ProfileSyncService::GetSyncTokenStatusForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetSyncTokenStatus();
}

void ProfileSyncService::OverrideNetworkForTest(
    const CreateHttpPostProviderFactory& create_http_post_provider_factory_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the engine has already been created, then it has a copy of the previous
  // HttpPostProviderFactory creation callback. In that case, shut down and
  // recreate the engine, so that it uses the correct (overridden) callback.
  // This is a horrible hack; the proper fix would be to inject the
  // callback in the ctor instead of adding it retroactively.
  bool restart = false;
  if (engine_) {
    StopImpl(KEEP_DATA);
    restart = true;
  }
  DCHECK(!engine_);

  // If a previous request (with the wrong callback) already failed, the next
  // one would be backed off, which breaks tests. So reset the backoff.
  auth_manager_->ResetRequestAccessTokenBackoffForTest();

  create_http_post_provider_factory_cb_ = create_http_post_provider_factory_cb;

  // For allowing tests to easily reset to the default (real) callback.
  if (!create_http_post_provider_factory_cb_) {
    create_http_post_provider_factory_cb_ =
        base::BindRepeating(&CreateHttpBridgeFactory);
  }

  if (restart) {
    startup_controller_->TryStart(/*force_immediate=*/true);
    DCHECK(engine_);
  }
}

bool ProfileSyncService::IsPassphrasePrompted() const {
  return sync_prefs_.IsPassphrasePrompted();
}

void ProfileSyncService::SetPassphrasePrompted(bool prompted) {
  sync_prefs_.SetPassphrasePrompted(prompted);
}

#if defined(OS_ANDROID)
void ProfileSyncService::SetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_prefs_.SetDecoupledFromAndroidMasterSync();
}

bool ProfileSyncService::GetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_prefs_.GetDecoupledFromAndroidMasterSync();
}
#endif  // defined(OS_ANDROID)

SyncEncryptionHandler::Observer*
ProfileSyncService::GetEncryptionObserverForTest() {
  return &crypto_;
}

void ProfileSyncService::RemoveClientFromServer() const {
  if (!engine_ || !engine_->IsInitialized()) {
    return;
  }
  const std::string cache_guid = sync_prefs_.GetCacheGuid();
  const std::string birthday = sync_prefs_.GetBirthday();
  DCHECK(!cache_guid.empty());
  const std::string& access_token = auth_manager_->access_token();
  if (!access_token.empty() && !birthday.empty()) {
    sync_stopped_reporter_->ReportSyncStopped(access_token, cache_guid,
                                              birthday);
  }
}

void ProfileSyncService::RecordMemoryUsageAndCountsHistograms() {
  ModelTypeSet active_types = GetActiveDataTypes();
  for (ModelType type : active_types) {
    auto dtc_it = data_type_controllers_.find(type);
    if (dtc_it != data_type_controllers_.end() &&
        dtc_it->second->state() != DataTypeController::NOT_RUNNING) {
      // It's possible that a data type is considered active, but its
      // DataTypeController is still NOT_RUNNING, in the case where we're
      // waiting for a custom passphrase.
      dtc_it->second->RecordMemoryUsageAndCountsHistograms();
    }
  }
}

const GURL& ProfileSyncService::GetSyncServiceUrlForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_service_url_;
}

std::string ProfileSyncService::GetUnrecoverableErrorMessageForDebugging()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_message_;
}

base::Location ProfileSyncService::GetUnrecoverableErrorLocationForDebugging()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_location_;
}

void ProfileSyncService::OnSetupInProgressHandleDestroyed() {
  DCHECK_GT(outstanding_setup_in_progress_handles_, 0);

  --outstanding_setup_in_progress_handles_;

  if (engine_ && engine_->IsInitialized()) {
    // The user closed a setup UI, and will expect their changes to actually
    // take effect now. So we reconfigure here even if another setup UI happens
    // to be open right now.
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/true);
  }

  NotifyObservers();
}

void ProfileSyncService::ReconfigureDueToPassphrase(ConfigureReason reason) {
  if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    DCHECK(data_type_manager_->IsNigoriEnabled());
    ConfigureDataTypeManager(reason);
  }
  // Notify observers that the passphrase status may have changed, regardless of
  // whether we triggered configuration or not. This is needed for the
  // IsSetupInProgress() case where the UI needs to be updated to reflect that
  // the passphrase was accepted (https://crbug.com/870256).
  NotifyObservers();
}

void ProfileSyncService::OnRequiredUserActionChanged() {
  if (should_record_trusted_vault_error_shown_on_startup_ &&
      crypto_.IsTrustedVaultKeyRequiredStateKnown() && IsSyncFeatureEnabled()) {
    should_record_trusted_vault_error_shown_on_startup_ = false;
    if (crypto_.GetPassphraseType() ==
        PassphraseType::kTrustedVaultPassphrase) {
      base::UmaHistogramBoolean(
          "Sync.TrustedVaultErrorShownOnStartup",
          user_settings_->IsTrustedVaultKeyRequiredForPreferredDataTypes());
    }
  }
}

}  // namespace syncer
