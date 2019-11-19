// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/driver/sync_stopped_reporter.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/network_time_update_callback.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/js/sync_js_controller.h"
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
class TypeDebugInfoObserver;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;
struct UserShare;

// Look at the SyncService interface for information on how to use this class.
// You should not need to know about ProfileSyncService directly.
class ProfileSyncService : public SyncService,
                           public SyncEngineHost,
                           public SyncPrefObserver,
                           public DataTypeManagerObserver,
                           public UnrecoverableErrorHandler,
                           public signin::IdentityManager::Observer {
 public:
  // If AUTO_START, sync will set IsFirstSetupComplete() automatically and sync
  // will begin syncing without the user needing to confirm sync settings.
  enum StartBehavior {
    AUTO_START,
    MANUAL_START,
  };

  // Bundles the arguments for ProfileSyncService construction. This is a
  // movable struct. Because of the non-POD data members, it needs out-of-line
  // constructors, so in particular the move constructor needs to be
  // explicitly defined.
  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);
    ~InitParams();

    std::unique_ptr<SyncClient> sync_client;
    signin::IdentityManager* identity_manager = nullptr;
    std::vector<invalidation::IdentityProvider*>
        invalidations_identity_providers;
    StartBehavior start_behavior = MANUAL_START;
    NetworkTimeUpdateCallback network_time_update_callback;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    network::NetworkConnectionTracker* network_connection_tracker = nullptr;
    version_info::Channel channel = version_info::Channel::UNKNOWN;
    std::string debug_identifier;
    bool autofill_enable_account_wallet_storage = false;
    bool enable_passwords_account_storage = false;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  explicit ProfileSyncService(InitParams init_params);

  ~ProfileSyncService() override;

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  // TODO(mastiz): Rename this to Start().
  void Initialize();

  // SyncService implementation
  SyncUserSettings* GetUserSettings() override;
  const SyncUserSettings* GetUserSettings() const override;
  int GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;
  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  std::unique_ptr<crypto::ECPrivateKey> GetExperimentalAuthenticationKey()
      const override;
  bool IsSetupInProgress() const override;
  ModelTypeSet GetRegisteredDataTypes() const override;
  ModelTypeSet GetPreferredDataTypes() const override;
  ModelTypeSet GetActiveDataTypes() const override;
  void StopAndClear() override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void DataTypePreconditionChanged(ModelType type) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  UserDemographicsResult GetUserNoisedBirthYearAndGender(
      base::Time now) override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  UserShare* GetUserShare() const override;
  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  std::unique_ptr<base::Value> GetTypeStatusMapForDebugging() override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  base::WeakPtr<JsController> GetJsController() override;
  void GetAllNodesForDebugging(
      const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback)
      override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const std::string& cache_guid,
      const std::string& birthday,
      const std::string& bag_of_chips,
      const std::string& last_keystore_key,
      bool success) override;
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot,
                            const std::string& last_keystore_key) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnDirectoryTypeCommitCounterUpdated(
      ModelType type,
      const CommitCounters& counters) override;
  void OnDirectoryTypeUpdateCounterUpdated(
      ModelType type,
      const UpdateCounters& counters) override;
  void OnDatatypeStatusCounterUpdated(ModelType type,
                                      const StatusCounters& counters) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnMigrationNeededForTypes(ModelTypeSet types) override;
  void OnActionableError(const SyncProtocolError& error) override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Similar to above but with a callback that will be invoked on completion.
  void OnAccountsInCookieUpdatedWithCallback(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const base::Closure& callback);

  // Returns true if currently signed in account is not present in the list of
  // accounts from cookie jar.
  bool HasCookieJarMismatch(
      const std::vector<gaia::ListedAccount>& cookie_jar_accounts);

  // UnrecoverableErrorHandler implementation.
  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override;

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;
  void OnFirstSetupCompletePrefChange(bool is_first_setup_complete) override;
  void OnSyncRequestedPrefChange(bool is_sync_requested) override;
  void OnPreferredDataTypesPrefChange() override;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

  // This triggers a Directory::SaveChanges() call on the sync thread.
  // It should be used to persist data to disk when the process might be
  // killed in the near future.
  void FlushDirectory() const;

  bool IsPassphrasePrompted() const;
  void SetPassphrasePrompted(bool prompted);

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

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  bool IsEncryptionPendingForTest() const;

  // Overrides the callback used to create network connections.
  // TODO(crbug.com/949504): Inject this in the ctor instead. As it is, it's
  // possible that the real callback was already used before the test had a
  // chance to call this.
  void OverrideNetworkForTest(const CreateHttpPostProviderFactory&
                                  create_http_post_provider_factory_cb);

  bool IsDataTypeControllerRunningForTest(ModelType type) const;

  // Sometimes we need to wait for tasks on the |backend_task_runner_| in tests.
  void FlushBackendTaskRunnerForTest() const;

  // Some tests rely on injecting calls to the encryption observer.
  SyncEncryptionHandler::Observer* GetEncryptionObserverForTest();

  SyncClient* GetSyncClientForTest();

  // Combines GAIA ID, sync birthday and keystore key with '|' sepearator to
  // generate a secret. Returns empty string if keystore key is not available.
  std::string GetExperimentalAuthenticationSecretForTest() const;

 private:
  // Passed as an argument to StopImpl to control whether or not the sync
  // engine should clear its data directory when it shuts down. See StopImpl
  // for more information.
  enum SyncStopDataFate {
    KEEP_DATA,
    CLEAR_DATA,
  };

  enum UnrecoverableErrorReason {
    ERROR_REASON_UNSET,
    ERROR_REASON_SYNCER,
    ERROR_REASON_ENGINE_INIT_FAILURE,
    ERROR_REASON_CONFIGURATION_RETRY,
    ERROR_REASON_CONFIGURATION_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
    ERROR_REASON_LIMIT
  };

  // Virtual for testing.
  virtual WeakHandle<JsEventHandler> GetJsEventHandler();

  WeakHandle<UnrecoverableErrorHandler> GetUnrecoverableErrorHandler();

  // Callbacks for SyncAuthManager.
  void AccountStateChanged();
  void CredentialsChanged();

  // Callbacks for SyncUserSettingsImpl.
  void SyncAllowedByPlatformChanged(bool allowed);

  bool IsEngineAllowedToStart() const;

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the engine if it is not already initialized.
  // If a Sync setup is currently in progress (i.e. a settings UI is open), then
  // the reconfiguration will only happen if |bypass_setup_in_progress_check| is
  // set to true.
  void ReconfigureDatatypeManager(bool bypass_setup_in_progress_check);

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager(ConfigureReason reason);

  // Shuts down the engine sync components.
  // |reason| dictates if syncing is being disabled or not.
  void ShutdownImpl(ShutdownReason reason);

  // Helper for OnUnrecoverableError.
  void OnUnrecoverableErrorImpl(const base::Location& from_here,
                                const std::string& message,
                                UnrecoverableErrorReason reason);

  // Stops the sync engine. Does NOT set IsSyncRequested to false. Use
  // RequestStop for that. |data_fate| controls whether the local sync data is
  // deleted or kept when the engine shuts down.
  void StopImpl(SyncStopDataFate data_fate);

  // Puts the engine's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Notify all observers that a change has occurred.
  void NotifyObservers();

  void NotifySyncCycleCompleted();
  void NotifyShutdown();

  void ClearUnrecoverableError();

  // Initializes |backend_task_runner_| which is backed by |sync_thread_| or the
  // ThreadPool depending on the ProfileSyncServiceUsesThreadPool experiment.
  void InitializeBackendTaskRunnerIfNeeded();

  // Kicks off asynchronous initialization of the SyncEngine.
  void StartUpSlowEngineComponents();

  // Update UMA for syncing engine.
  void UpdateEngineInitUMA(bool success) const;

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Estimates and records memory usage histograms per type.
  void RecordMemoryUsageHistograms();

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes(bool bypass_setup_in_progress_check) const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  void OnSetupInProgressHandleDestroyed();

  // Called by SyncServiceCrypto when a passphrase is required or accepted.
  void ReconfigureDueToPassphrase(ConfigureReason reason);

  std::string GetExperimentalAuthenticationSecret() const;

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  const std::unique_ptr<SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync preferences.
  SyncPrefs sync_prefs_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address and sign-out upon error.
  signin::IdentityManager* const identity_manager_;

  // The user-configurable knobs. Non-null between Initialize() and Shutdown().
  std::unique_ptr<SyncUserSettingsImpl> user_settings_;

  // Handles tracking of the authenticated account and acquiring access tokens.
  // Only null after Shutdown().
  std::unique_ptr<SyncAuthManager> auth_manager_;

  const version_info::Channel channel_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  const bool autofill_enable_account_wallet_storage_;
  const bool enable_passwords_account_storage_;

  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // A utility object containing logic and state relating to encryption.
  SyncServiceCrypto crypto_;

  // Owns the sync thread and takes care of its destruction.
  // TODO(https://crbug.com/1014464): Remove once we have switched to
  // Threadpool.
  base::OnceClosure sync_thread_stopper_;

  // TODO(crbug.com/923287): Move out of this class. Possibly to SyncEngineImpl.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<SyncEngine> engine_;

  // Used to ensure that certain operations are performed on the sequence that
  // this object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Cache of the last SyncCycleSnapshot received from the sync engine.
  SyncCycleSnapshot last_snapshot_;

  // Callback to update the network time; used for initializing the engine.
  NetworkTimeUpdateCallback network_time_update_callback_;

  // The URL loader factory for the sync.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The global NetworkConnectionTracker instance.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Indicates if this is the first time sync is being configured.
  // This is set to true if last synced time is not set at the time of
  // OnEngineInitialized().
  bool is_first_time_sync_configure_;

  // Last known keystore key, populated after engine initialization.
  // TODO(crbug.com/1012226): Remove |last_keystore_key_| when VAPID migration
  // is over.
  std::string last_keystore_key_;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // Set when sync receives STOP_SYNC_FOR_DISABLED_ACCOUNT error from server.
  // Prevents ProfileSyncService from starting engine till browser restarted
  // or user signed out.
  bool sync_disabled_by_admin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  base::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<DataTypeManager> data_type_manager_;

  base::ObserverList<SyncServiceObserver>::Unchecked observers_;
  base::ObserverList<ProtocolEventObserver>::Unchecked
      protocol_event_observers_;
  base::ObserverList<TypeDebugInfoObserver>::Unchecked
      type_debug_info_observers_;

  SyncJsController sync_js_controller_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  std::unique_ptr<BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  SyncProtocolError last_actionable_error_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  DataTypeStatusTable::TypeErrorMap data_type_error_map_;

  // This providers tells the invalidations code which identity to register for.
  // The account that it registers for should be the same as the currently
  // syncing account, so we'll need to update this whenever the account changes.
  std::vector<invalidation::IdentityProvider*> const
      invalidations_identity_providers_;

  // List of available data type controllers.
  DataTypeController::TypeMap data_type_controllers_;

  CreateHttpPostProviderFactory create_http_post_provider_factory_cb_;

  const StartBehavior start_behavior_;
  std::unique_ptr<StartupController> startup_controller_;

  std::unique_ptr<SyncStoppedReporter> sync_stopped_reporter_;

  // Whether the major version has changed since the last time Chrome ran,
  // and therefore a passphrase required state should result in prompting
  // the user. This logic is only enabled on platforms that consume the
  // IsPassphrasePrompted sync preference.
  bool passphrase_prompt_triggered_by_version_;

  // Used by StopAndClear() to remember that clearing of data is needed (as
  // sync is stopped after a callback from |user_settings_|).
  bool is_stopping_and_clearing_;

  // This weak factory invalidates its issued pointers when Sync is disabled.
  base::WeakPtrFactory<ProfileSyncService> sync_enabled_weak_factory_{this};

  base::WeakPtrFactory<ProfileSyncService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_H_
