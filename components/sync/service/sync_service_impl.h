// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/data_type_manager_observer.h"
#include "components/sync/service/sync_client.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_crypto.h"
#include "components/sync/service/sync_stopped_reporter.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {

class BackendMigrator;
class SyncAuthManager;
class SyncFeatureStatusForMigrationsRecorder;
class SyncPrefsPolicyHandler;

#if BUILDFLAG(IS_ANDROID)
class SyncServiceAndroidBridge;
#endif  // BUILDFLAG(IS_ANDROID)

// Look at the SyncService interface for information on how to use this class.
// You should not need to know about SyncServiceImpl directly.
class SyncServiceImpl : public SyncService,
                        public SyncEngineHost,
                        public SyncPrefObserver,
                        public DataTypeManagerObserver,
                        public SyncServiceCrypto::Delegate,
                        public SyncUserSettingsImpl::Delegate,
                        public signin::IdentityManager::Observer {
 public:
  // Bundles the arguments for SyncServiceImpl construction. This is a
  // movable struct. Because of the non-POD data members, it needs out-of-line
  // constructors, so in particular the move constructor needs to be
  // explicitly defined.
  struct InitParams {
    InitParams();

    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;

    InitParams(InitParams&& other);

    ~InitParams();

    std::unique_ptr<SyncClient> sync_client;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    raw_ptr<network::NetworkConnectionTracker> network_connection_tracker =
        nullptr;
    version_info::Channel channel = version_info::Channel::UNKNOWN;
    std::string debug_identifier;
  };

  explicit SyncServiceImpl(InitParams init_params);

  SyncServiceImpl(const SyncServiceImpl&) = delete;
  SyncServiceImpl& operator=(const SyncServiceImpl&) = delete;

  ~SyncServiceImpl() override;

  // Initializes the object. This must be called at most once, and immediately
  // after an object of this class is constructed. `controllers` determines all
  // supported types and their controllers.
  void Initialize(DataTypeController::TypeVector controllers);

  // SyncService implementation
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif  // BUILDFLAG(IS_ANDROID)
  void SetSyncFeatureRequested() override;
  SyncUserSettings* GetUserSettings() override;
  const SyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  UserActionableError GetUserActionableError() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAccountInfo() const override;
  bool HasSyncConsent() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;
  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  DataTypeSet GetPreferredDataTypes() const override;
  DataTypeSet GetActiveDataTypes() const override;
  DataTypeSet GetTypesWithPendingDownloadForInitialSync() const override;
  void OnDataTypeRequestsSyncStartup(DataType type) override;
  void TriggerRefresh(const DataTypeSet& types) override;
  void DataTypePreconditionChanged(DataType type) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void SendExplicitPassphraseToPlatformClient() override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  TypeStatusMapForDebugging GetTypeStatusMapForDebugging() const override;
  void GetEntityCountsForDebugging(
      base::RepeatingCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) override;
  DataTypeDownloadStatus GetDownloadStatusFor(DataType type) const override;
  void GetTypesWithUnsyncedData(
      DataTypeSet requested_types,
      base::OnceCallback<void(DataTypeSet)> callback) const override;
  void GetLocalDataDescriptions(
      DataTypeSet types,
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
          callback) override;
  void TriggerLocalDataMigration(DataTypeSet types) override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(bool success,
                           bool is_first_time_sync_configure) override;
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnMigrationNeededForTypes(DataTypeSet types) override;
  void OnActionableProtocolError(const SyncProtocolError& error) override;
  void OnBackedOffTypesChanged() override;
  void OnInvalidationStatusChanged() override;
  void OnNewInvalidatedDataTypes() override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // SyncServiceCrypto::Delegate implementation.
  void CryptoStateChanged() override;
  void CryptoRequiredUserActionChanged() override;
  void ReconfigureDataTypesDueToCrypto() override;
  void PassphraseTypeChanged(PassphraseType passphrase_type) override;
  std::optional<PassphraseType> GetPassphraseType() const override;
  void SetEncryptionBootstrapToken(const std::string& bootstrap_token) override;
  std::string GetEncryptionBootstrapToken() const override;

  // SyncUserSettingsImpl::Delegate implementation.
  bool IsCustomPassphraseAllowed() const override;
  SyncPrefs::SyncAccountState GetSyncAccountStateForPrefs() const override;
  CoreAccountInfo GetSyncAccountInfoForPrefs() const override;

  // IdentityManager::Observer implementation.
  void OnAccountsCookieDeletedByUserAction() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Similar to above but with a callback that will be invoked on completion.
  void OnAccountsInCookieUpdatedWithCallback(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      base::OnceClosure callback);

  // Returns true if currently signed in account is not present in the list of
  // accounts from cookie jar.
  bool HasCookieJarMismatch(
      const std::vector<gaia::ListedAccount>& cookie_jar_accounts);

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void OnFirstSetupCompletePrefChange(
      bool is_initial_sync_feature_setup_complete) override;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  void OnSelectedTypesPrefChange() override;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  void HasUnsyncedItemsForTest(base::OnceCallback<void(bool)> cb) const;

  // Used by MigrationWatcher.  May return null.
  BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with the access token fetcher.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // Overrides the callback used to create network connections.
  // TODO(crbug.com/41451146): Inject this in the ctor instead. As it is, it's
  // possible that the real callback was already used before the test had a
  // chance to call this.
  void OverrideNetworkForTest(const CreateHttpPostProviderFactory&
                                  create_http_post_provider_factory_cb);

  DataTypeSet GetRegisteredDataTypesForTest() const;
  bool HasAnyModelErrorForTest(DataTypeSet types) const;

  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(DataTypeSet)> cb) const;

  // Some tests rely on injecting calls to the encryption observer.
  SyncEncryptionHandler::Observer* GetEncryptionObserverForTest();

  SyncClient* GetSyncClientForTest();

  // Simulates data type error reported by the bridge.
  void ReportDataTypeErrorForTest(DataType type);

 private:
  enum UnrecoverableErrorReason {
    ERROR_REASON_ENGINE_INIT_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(SyncResetEngineReason)
  enum class ResetEngineReason {
    kShutdown = 0,
    kUnrecoverableError = 1,
    kDisabledAccount = 2,
    // kRequestedPrefChange = 3,
    kUpgradeClientError = 4,
    // kSetSyncAllowedByPlatform = 5,
    kCredentialsChanged = 6,
    kResetLocalData = 7,
    kNotSignedIn = 8,
    kEnterprisePolicy = 9,
    kDisableSyncOnClient = 10,

    kMaxValue = kDisableSyncOnClient
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncResetEngineReason)

  static ShutdownReason ShutdownReasonForResetEngineReason(
      ResetEngineReason reset_reason);

  static bool ShouldClearTransportDataForAccount(
      ResetEngineReason reset_reason);

  void StopAndClear(ResetEngineReason reset_engine_reason);

  // Callbacks for SyncAuthManager.
  void AccountStateChanged();
  void CredentialsChanged();

  bool IsEngineAllowedToRun() const;

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the engine if it is not already initialized.
  // If a Sync setup is currently in progress (i.e. a settings UI is open), then
  // the reconfiguration will only happen if |bypass_setup_in_progress_check| is
  // set to true.
  void ReconfigureDatatypeManager(bool bypass_setup_in_progress_check);

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager(ConfigureReason reason);

  bool UseTransportOnlyMode() const;

  void UpdateDataTypesForInvalidations();

  // Shuts down and destroys the engine. |reset_reason| specifies the reason for
  // the shutdown, and dictates if sync metadata should be kept or not.
  // If the engine is still allowed to run (per IsEngineAllowedToRun()), it will
  // soon start up again (possibly in transport-only mode).
  std::unique_ptr<SyncEngine> ResetEngine(ResetEngineReason reset_reason);

  // Helper for OnUnrecoverableError.
  void OnUnrecoverableErrorImpl(const base::Location& from_here,
                                const std::string& message,
                                UnrecoverableErrorReason reason);

  // Puts the engine's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Notify all observers that a change has occurred.
  void NotifyObservers();

  void NotifySyncCycleCompleted();
  void NotifyShutdown();

  void ClearUnrecoverableError();

  // Posts a task to create the sync engine, if IsEngineAllowedToRun() is true
  // and there is no engine yet (no-op otherwise). This method posts a task so
  // callers can set up other state as necessary before the engine starts.
  void TryStart();

  // The actual synchronous implementation of TryStart().
  void TryStartImpl();

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Records histograms about the history opt-in state.
  void RecordHistoryOptInStateOnSigninHistograms(
      signin_metrics::AccessPoint access_point,
      signin::ConsentLevel consent_level);

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes(bool bypass_setup_in_progress_check) const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  void OnSetupInProgressHandleDestroyed();

  // Records (or may record) histograms related to trusted vault passphrase
  // type.
  void MaybeRecordTrustedVaultHistograms();

  void OnPasswordSyncAllowedChanged();

  // Updates PrefService (SyncPrefs) to cache the last known value for trusted
  // vault AutoUpgradeDebugInfo. It also notifies SyncClient.
  void CacheTrustedVaultDebugInfoToPrefsFromEngine();

  // Exercises SyncClient to register synthetic field trials for trusted vault
  // passphrase type.
  void RegisterTrustedVaultSyntheticFieldTrialsIfNecessary();

  // The actual implementation of GetLocalDataDescriptions(), where some code
  // paths can be synchronous. GetLocalDataDescriptions() posts a task before
  // invoking this, to ensure that the public call is always async.
  void GetLocalDataDescriptionsImpl(
      DataTypeSet types,
      base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
          callback);

  // This profile's SyncClient.
  const std::unique_ptr<SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync preferences.
  SyncPrefs sync_prefs_;

  // The class that updates SyncPrefs when a policy is applied.
  std::unique_ptr<SyncPrefsPolicyHandler> sync_prefs_policy_handler_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address and sign-out upon error.
  // May be null (if local Sync is enabled).
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The user-configurable knobs. Non-null between Initialize() and Shutdown().
  std::unique_ptr<SyncUserSettingsImpl> user_settings_;

  // Handles tracking of the authenticated account and acquiring access tokens.
  // Only null after Shutdown().
  std::unique_ptr<SyncAuthManager> auth_manager_;

  const version_info::Channel channel_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // A utility object containing logic and state relating to encryption.
  SyncServiceCrypto crypto_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<SyncEngine> engine_;

  // Used to ensure that certain operations are performed on the sequence that
  // this object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Cache of the last SyncCycleSnapshot received from the sync engine.
  SyncCycleSnapshot last_snapshot_;

  // The URL loader factory for the sync.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The global NetworkConnectionTracker instance.
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  // Indicates if this is the first time sync is being configured.
  // This is set to true if last synced time is not set at the time of
  // OnEngineInitialized().
  bool is_first_time_sync_configure_ = false;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // Set when sync receives STOP_SYNC_FOR_DISABLED_ACCOUNT error from server.
  // Prevents SyncServiceImpl from starting engine till browser restarted
  // or user signed out.
  bool sync_disabled_by_admin_ = false;

  // Information describing an unrecoverable error.
  std::optional<UnrecoverableErrorReason> unrecoverable_error_reason_ =
      std::nullopt;
  std::string unrecoverable_error_message_;
  base::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<DataTypeManager> data_type_manager_;

  // Note: This is an Optional so that we can control its destruction - in
  // particular, to trigger the "check_empty" test in Shutdown().
  std::optional<base::ObserverList<SyncServiceObserver,
                                   /*check_empty=*/true>::Unchecked>
      observers_;

  base::ObserverList<ProtocolEventObserver>::Unchecked
      protocol_event_observers_;

  std::unique_ptr<BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  SyncProtocolError last_actionable_error_;

  CreateHttpPostProviderFactory create_http_post_provider_factory_cb_;

  std::unique_ptr<SyncStoppedReporter> sync_stopped_reporter_;

  // Used for UMA to determine whether TrustedVaultErrorShownOnStartup
  // histogram needs to recorded. Set to false iff histogram was already
  // recorded or trusted vault passphrase type wasn't used on startup.
  bool should_record_trusted_vault_error_shown_on_startup_ = true;

  // Whether or not SyncClient was exercised to register synthetic field trials
  // related to trusted vault passphrase, and if yes which precise group was
  // registered.
  std::optional<TrustedVaultAutoUpgradeSyntheticFieldTrialGroup>
      registered_trusted_vault_auto_upgrade_synthetic_field_trial_group_;

  // Whether we want to receive invalidations for the SESSIONS data type. This
  // is typically false on Android (to save network traffic), but true on all
  // other platforms.
  bool sessions_invalidations_enabled_ = !BUILDFLAG(IS_ANDROID);

  // Set if/when Initialize() schedules a deferred task to start the engine.
  // Cleared on the first start attempt, regardless of success and who triggered
  // that attempt (the posted task or a new TryStart()).
  base::Time deferring_first_start_since_;

  std::unique_ptr<SyncFeatureStatusForMigrationsRecorder> sync_status_recorder_;

  base::ScopedObservation<SyncPrefs, SyncPrefObserver> sync_prefs_observation_{
      this};

#if BUILDFLAG(IS_ANDROID)
  // Manage and fetch the java object that wraps this SyncService on
  // android.
  std::unique_ptr<SyncServiceAndroidBridge> sync_service_android_;
#endif  // BUILDFLAG(IS_ANDROID)

  base::WeakPtrFactory<SyncServiceImpl> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_IMPL_H_
