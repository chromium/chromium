// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/sync/base/experiments.h"
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
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/net/network_time_update_callback.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/js/sync_js_controller.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace base {
class MessageLoop;
}

namespace identity {
class IdentityManager;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {
class BackendMigrator;
class BaseTransaction;
class DeviceInfoSyncBridge;
class DeviceInfoTracker;
class ModelTypeControllerDelegate;
class NetworkResources;
class SyncTypePreferenceProvider;
class TypeDebugInfoObserver;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;
struct UserShare;
}  // namespace syncer

namespace browser_sync {

class SyncAuthManager;

// ProfileSyncService is the layer between browser subsystems like bookmarks,
// and the sync engine. Each subsystem is logically thought of as being a sync
// datatype. Individual datatypes can, at any point, be in a variety of stages
// of being "enabled". Here are some specific terms for concepts used in this
// class:
//
//   'Registered' (feature suppression for a datatype)
//
//      When a datatype is registered, the user has the option of syncing it.
//      The sync opt-in UI will show only registered types; a checkbox should
//      never be shown for an unregistered type, nor can it ever be synced.
//
//   'Preferred' (user preferences and opt-out for a datatype)
//
//      This means the user's opt-in or opt-out preference on a per-datatype
//      basis.  The sync service will try to make active exactly these types.
//      If a user has opted out of syncing a particular datatype, it will
//      be registered, but not preferred.
//
//      This state is controlled by OnUserChoseDatatypes and
//      GetPreferredDataTypes.  They are stored in the preferences system,
//      and persist; though if a datatype is not registered, it cannot
//      be a preferred datatype.
//
//   'Active' (run-time initialization of sync system for a datatype)
//
//      An active datatype is a preferred datatype that is actively being
//      synchronized: the syncer has been instructed to querying the server
//      for this datatype, first-time merges have finished, and there is an
//      actively installed ChangeProcessor that listens for changes to this
//      datatype, propagating such changes into and out of the sync engine
//      as necessary.
//
//      When a datatype is in the process of becoming active, it may be
//      in some intermediate state.  Those finer-grained intermediate states
//      are differentiated by the DataTypeController state.
//
// Sync Configuration:
//
//   Sync configuration is accomplished via the following APIs:
//    * OnUserChoseDatatypes(): Set the data types the user wants to sync.
//    * SetDecryptionPassphrase(): Attempt to decrypt the user's encrypted data
//        using the passed passphrase.
//    * SetEncryptionPassphrase(): Re-encrypt the user's data using the passed
//        passphrase.
//
//   Additionally, the current sync configuration can be fetched by calling
//    * GetRegisteredDataTypes()
//    * GetPreferredDataTypes()
//    * GetActiveDataTypes()
//    * IsUsingSecondaryPassphrase()
//    * IsEncryptEverythingEnabled()
//    * IsPassphraseRequired()/IsPassphraseRequiredForDecryption()
//
//   The "sync everything" state cannot be read from ProfileSyncService, but
//   is instead pulled from SyncPrefs.HasKeepEverythingSynced().
//
// Initial sync setup:
//
//   For privacy reasons, it is usually desirable to avoid syncing any data
//   types until the user has finished setting up sync. There are two APIs
//   that control the initial sync download:
//
//    * SetFirstSetupComplete()
//    * GetSetupInProgressHandle()
//
//   SetFirstSetupComplete() should be called once the user has finished setting
//   up sync at least once on their account. GetSetupInProgressHandle() should
//   be called while the user is actively configuring their account. The handle
//   should be deleted once configuration is complete.
//
//   Once first setup has completed and there are no outstanding
//   setup-in-progress handles, CanConfigureDataTypes() will return true and
//   datatype configuration can begin.
class ProfileSyncService : public syncer::SyncService,
                           public syncer::SyncEngineHost,
                           public syncer::SyncPrefObserver,
                           public syncer::DataTypeManagerObserver,
                           public syncer::UnrecoverableErrorHandler,
                           public GaiaCookieManagerService::Observer {
 public:
  using SigninScopedDeviceIdCallback = base::RepeatingCallback<std::string()>;

  // NOTE: Used in a UMA histogram, do not reorder etc.
  enum SyncEventCodes {
    // Events starting the sync service.
    // START_FROM_NTP = 1,
    // START_FROM_WRENCH = 2,
    // START_FROM_OPTIONS = 3,
    // START_FROM_BOOKMARK_MANAGER = 4,
    // START_FROM_PROFILE_MENU = 5,
    // START_FROM_URL = 6,

    // Events regarding cancellation of the signon process of sync.
    // CANCEL_FROM_SIGNON_WITHOUT_AUTH = 10,
    // CANCEL_DURING_SIGNON = 11,
    CANCEL_DURING_CONFIGURE = 12,  // Cancelled before choosing data types and
                                   // clicking OK.

    // Events resulting in the stoppage of sync service.
    STOP_FROM_OPTIONS = 20,  // Sync was stopped from Wrench->Options.
    // STOP_FROM_ADVANCED_DIALOG = 21,

    MAX_SYNC_EVENT_CODE = 22
  };

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

    std::unique_ptr<syncer::SyncClient> sync_client;
    identity::IdentityManager* identity_manager;
    SigninScopedDeviceIdCallback signin_scoped_device_id_callback;
    GaiaCookieManagerService* gaia_cookie_manager_service = nullptr;
    std::vector<invalidation::IdentityProvider*>
        invalidations_identity_providers;
    StartBehavior start_behavior = MANUAL_START;
    syncer::NetworkTimeUpdateCallback network_time_update_callback;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    network::NetworkConnectionTracker* network_connection_tracker;
    std::string debug_identifier;
    version_info::Channel channel = version_info::Channel::UNKNOWN;
    bool user_events_separate_pref_group = false;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  explicit ProfileSyncService(InitParams init_params);

  ~ProfileSyncService() override;

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  void Initialize();

  // syncer::SyncService implementation
  int GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsFirstSetupComplete() const override;
  bool IsLocalSyncEnabled() const override;
  void TriggerRefresh(const syncer::ModelTypeSet& types) override;
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type) override;
  void RequestStop(SyncStopDataFate data_fate) override;
  void RequestStart() override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;
  void AddObserver(syncer::SyncServiceObserver* observer) override;
  void RemoveObserver(syncer::SyncServiceObserver* observer) override;
  bool HasObserver(const syncer::SyncServiceObserver* observer) const override;
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  void OnUserChoseDatatypes(bool sync_everything,
                            syncer::ModelTypeSet chosen_types) override;
  void SetFirstSetupComplete() override;
  std::unique_ptr<syncer::SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  const GoogleServiceAuthError& GetAuthError() const override;
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override
      WARN_UNUSED_RESULT;
  bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;
  syncer::UserShare* GetUserShare() const override;
  void ReenableDatatype(syncer::ModelType type) override;
  void ReadyForStartChanged(syncer::ModelType type) override;
  syncer::SyncTokenStatus GetSyncTokenStatus() const override;
  bool QueryDetailedSyncStatus(syncer::SyncStatus* result) const override;
  base::Time GetLastSyncedTime() const override;
  syncer::SyncCycleSnapshot GetLastCycleSnapshot() const override;
  std::unique_ptr<base::Value> GetTypeStatusMap() override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  base::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(
      syncer::ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(
      syncer::ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  base::WeakPtr<syncer::JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;
  AccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;

  // Add a sync type preference provider. Each provider may only be added once.
  void AddPreferenceProvider(syncer::SyncTypePreferenceProvider* provider);
  // Remove a sync type preference provider. May only be called for providers
  // that have been added. Providers must not remove themselves while being
  // called back.
  void RemovePreferenceProvider(syncer::SyncTypePreferenceProvider* provider);
  // Check whether a given sync type preference provider has been added.
  bool HasPreferenceProvider(
      syncer::SyncTypePreferenceProvider* provider) const;

  const syncer::LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() const;
  void SetLocalDeviceInfoProviderForTest(
      std::unique_ptr<syncer::LocalDeviceInfoProvider> provider);

  // Returns the ModelTypeControllerDelegate for syncer::DEVICE_INFO.
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetDeviceInfoSyncControllerDelegate();

  // Returns synced devices tracker.
  // Virtual for testing.
  virtual syncer::DeviceInfoTracker* GetDeviceInfoTracker() const;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      syncer::ModelTypeSet initial_types,
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      const std::string& session_name,
      bool success) override;
  void OnSyncCycleCompleted(const syncer::SyncCycleSnapshot& snapshot) override;
  void OnProtocolEvent(const syncer::ProtocolEvent& event) override;
  void OnDirectoryTypeCommitCounterUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) override;
  void OnDirectoryTypeUpdateCounterUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) override;
  void OnDatatypeStatusCounterUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) override;
  void OnConnectionStatusChange(syncer::ConnectionStatus status) override;
  void OnMigrationNeededForTypes(syncer::ModelTypeSet types) override;
  void OnExperimentsChanged(const syncer::Experiments& experiments) override;
  void OnActionableError(const syncer::SyncProtocolError& error) override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(
      const syncer::DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // DataTypeEncryptionHandler implementation.
  bool IsPassphraseRequired() const override;
  syncer::ModelTypeSet GetEncryptedDataTypes() const override;

  // GaiaCookieManagerService::Observer implementation.
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;

  // Similar to above but with a callback that will be invoked on completion.
  void OnGaiaAccountsInCookieUpdatedWithCallback(
      const std::vector<gaia::ListedAccount>& accounts,
      const base::Closure& callback);

  // Returns true if currently signed in account is not present in the list of
  // accounts from cookie jar.
  bool HasCookieJarMismatch(
      const std::vector<gaia::ListedAccount>& cookie_jar_accounts);

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the engine if it is not already initialized.
  // If a Sync setup is currently in progress (i.e. a settings UI is open), then
  // the reconfiguration will only happen if |bypass_setup_in_progress_check| is
  // set to true.
  void ReconfigureDatatypeManager(bool bypass_setup_in_progress_check);

  syncer::PassphraseRequiredReason passphrase_required_reason_for_test() const {
    return crypto_.passphrase_required_reason();
  }

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is allowed to run based on command-line switches.
  // Profile::IsSyncAllowed() is probably a better signal than this function.
  // This function can be called from any thread, and the implementation doesn't
  // assume it's running on the UI thread.
  static bool IsSyncAllowedByFlag();

  // Whether sync is currently blocked from starting because the sync
  // confirmation dialog hasn't been shown. Note that once the dialog is
  // showing (i.e. IsFirstSetupInProgress() is true), this will return false.
  // TODO(crbug.com/839834): This method is somewhat misnamed.
  // Virtual for testing.
  virtual bool IsSyncConfirmationNeeded() const;

  // syncer::UnrecoverableErrorHandler implementation.
  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  void HasUnsyncedItemsForTest(base::OnceCallback<void(bool)> cb) const;

  // Used by MigrationWatcher.  May return null.
  syncer::BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with OAuth2TokenService.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;

  // Returns the set of types which are enforced programmatically and can not
  // be disabled by the user.
  syncer::ModelTypeSet GetForcedDataTypes() const;

  // Gets the set of all data types that could be allowed (the set that
  // should be advertised to the user).  These will typically only change
  // via a command-line option.  See class comment for more on what it means
  // for a datatype to be Registered.
  // Virtual for testing.
  virtual syncer::ModelTypeSet GetRegisteredDataTypes() const;

  // See the SyncServiceCrypto header.
  // Virtual for testing.
  virtual syncer::PassphraseType GetPassphraseType() const;
  virtual bool IsEncryptEverythingAllowed() const;
  virtual void SetEncryptEverythingAllowed(bool allowed);

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  bool encryption_pending() const;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

  // Overrides the NetworkResources used for Sync connections.
  // TODO(treib): Inject this in the ctor instead. As it is, it's possible that
  // the real NetworkResources were already used before the test had a chance
  // to call this.
  void OverrideNetworkResourcesForTest(
      std::unique_ptr<syncer::NetworkResources> network_resources);

  // Virtual for testing.
  virtual bool IsDataTypeControllerRunning(syncer::ModelType type) const;

  // This triggers a Directory::SaveChanges() call on the sync thread.
  // It should be used to persist data to disk when the process might be
  // killed in the near future.
  void FlushDirectory() const;

  // Notifies observers that foreign sessions have been updated.
  // TODO(crbug.com/883199): This doesn't belong here, just like
  // OnForeignSessionUpdated() doesn't belong in the observer. Let's move them
  // to OpenTabsUIDelegate by introducing a similar observer mechanism.
  virtual void NotifyForeignSessionUpdated();

  // Returns a serialized NigoriKey proto generated from the bootstrap token in
  // SyncPrefs. Will return the empty string if no bootstrap token exists.
  std::string GetCustomPassphraseKey() const;

  // Set whether sync is currently allowed by the platform.
  void SetSyncAllowedByPlatform(bool allowed);

  // Sometimes we need to wait for tasks on the sync thread in tests.
  base::MessageLoop* GetSyncLoopForTest() const;

  // Some tests rely on injecting calls to the encryption observer.
  syncer::SyncEncryptionHandler::Observer* GetEncryptionObserverForTest();

  // Calls sync engine to send ClearServerDataMessage to server. This is used
  // to start accounts with a clean slate when performing end to end testing.
  void ClearServerDataForTest(const base::Closure& callback);

  syncer::SyncClient* GetSyncClientForTest();

 private:
  // Virtual for testing.
  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler();

  syncer::SyncEngine::HttpPostProviderFactoryGetter
  MakeHttpPostProviderFactoryGetter();

  syncer::WeakHandle<syncer::UnrecoverableErrorHandler>
  GetUnrecoverableErrorHandler();

  // Callbacks for SyncAuthManager.
  void AccountStateChanged();
  void CredentialsChanged();

  bool IsEngineAllowedToStart() const;

  // Callback for StartupController.
  bool ShouldStartEngine(bool bypass_first_setup_check) const;

  enum UnrecoverableErrorReason {
    ERROR_REASON_UNSET,
    ERROR_REASON_SYNCER,
    ERROR_REASON_ENGINE_INIT_FAILURE,
    ERROR_REASON_CONFIGURATION_RETRY,
    ERROR_REASON_CONFIGURATION_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
    ERROR_REASON_LIMIT
  };

  friend class TestProfileSyncService;

  // Returns whether sync is currently allowed on this platform.
  bool IsSyncAllowedByPlatform() const;

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager(syncer::ConfigureReason reason);

  // Shuts down the engine sync components.
  // |reason| dictates if syncing is being disabled or not, and whether
  // to claim ownership of sync thread from engine.
  void ShutdownImpl(syncer::ShutdownReason reason);

  // Helper method for managing encryption UI.
  bool IsEncryptedDatatypeEnabled() const;

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

  void ClearStaleErrors();

  void ClearUnrecoverableError();

  // Kicks off asynchronous initialization of the SyncEngine.
  // Virtual for testing.
  virtual void StartUpSlowEngineComponents();

  // Collects preferred sync data types from |preference_providers_|.
  syncer::ModelTypeSet GetDataTypesFromPreferenceProviders() const;

  // Update UMA for syncing engine.
  void UpdateEngineInitUMA(bool success) const;

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // Update first sync time stored in preferences
  void UpdateFirstSyncTimePref();

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Check if previous shutdown is shutdown cleanly.
  void ReportPreviousSessionMemoryWarningCount();

  // Estimates and records memory usage histograms per type.
  void RecordMemoryUsageHistograms();

  // After user switches to custom passphrase encryption a set of steps needs to
  // be performed:
  //
  // - Download all latest updates from server (catch up configure).
  // - Clear user data on server.
  // - Clear directory so that data is merged from model types and encrypted.
  //
  // SyncServiceCrypto::BeginConfigureCatchUpBeforeClear() and the following two
  // functions perform these steps.

  // Calls sync engine to send ClearServerDataMessage to server.
  void ClearAndRestartSyncForPassphraseEncryption();

  // Restarts sync clearing directory in the process.
  void OnClearServerDataDone();

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes(bool bypass_setup_in_progress_check) const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  virtual void OnSetupInProgressHandleDestroyed();

  // Called by SyncServiceCrypto when a passphrase is required or accepted.
  void ReconfigureDueToPassphrase(syncer::ConfigureReason reason);

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  const std::unique_ptr<syncer::SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync preferences.
  syncer::SyncPrefs sync_prefs_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address and sign-out upon error.
  identity::IdentityManager* const identity_manager_;

  // Handles tracking of the authenticated account and acquiring access tokens.
  // Only null after Shutdown().
  std::unique_ptr<SyncAuthManager> auth_manager_;

  // The product channel of the embedder.
  const version_info::Channel channel_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // Whether USER_EVENTS model type has a separate pref group instead of
  // being bundled with the TYPED_URLS model type.
  const bool user_events_separate_pref_group_;

  // A utility object containing logic and state relating to encryption.
  syncer::SyncServiceCrypto crypto_;

  // The thread where all the sync operations happen. This thread is kept alive
  // until browser shutdown and reused if sync is turned off and on again. It is
  // joined during the shutdown process, but there is an abort mechanism in
  // place to prevent slow HTTP requests from blocking browser shutdown.
  std::unique_ptr<base::Thread> sync_thread_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<syncer::SyncEngine> engine_;

  // Used to ensure that certain operations are performed on the sequence that
  // this object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  SigninScopedDeviceIdCallback signin_scoped_device_id_callback_;
  // Cache of the last SyncCycleSnapshot received from the sync engine.
  syncer::SyncCycleSnapshot last_snapshot_;

  // The time that OnConfigureStart is called. This member is zero if
  // OnConfigureStart has not yet been called, and is reset to zero once
  // OnConfigureDone is called.
  base::Time sync_configure_start_time_;

  // Callback to update the network time; used for initializing the engine.
  syncer::NetworkTimeUpdateCallback network_time_update_callback_;

  // The URL loader factory for the sync.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The global NetworkConnectionTracker instance.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Indicates if this is the first time sync is being configured.
  // This is set to true if last synced time is not set at the time of
  // OnEngineInitialized().
  bool is_first_time_sync_configure_;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // Whether the SyncEngine has been initialized.
  bool engine_initialized_;

  // Set when sync receives STOP_SYNC_FOR_DISABLED_ACCOUNT error from server.
  // Prevents ProfileSyncService from starting engine till browser restarted
  // or user signed out.
  bool sync_disabled_by_admin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  base::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<syncer::DataTypeManager> data_type_manager_;

  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;
  base::ObserverList<syncer::ProtocolEventObserver>::Unchecked
      protocol_event_observers_;
  base::ObserverList<syncer::TypeDebugInfoObserver>::Unchecked
      type_debug_info_observers_;

  std::set<syncer::SyncTypePreferenceProvider*> preference_providers_;

  syncer::SyncJsController sync_js_controller_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  std::unique_ptr<syncer::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  syncer::SyncProtocolError last_actionable_error_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  syncer::DataTypeStatusTable::TypeErrorMap data_type_error_map_;

  // The set of currently enabled sync experiments.
  syncer::Experiments current_experiments_;

  // The gaia cookie manager. Used for monitoring cookie jar changes to detect
  // when the user signs out of the content area.
  GaiaCookieManagerService* const gaia_cookie_manager_service_;

  // This providers tells the invalidations code which identity to register for.
  // The account that it registers for should be the same as the currently
  // syncing account, so we'll need to update this whenever the account changes.
  std::vector<invalidation::IdentityProvider*> const
      invalidations_identity_providers_;

  std::unique_ptr<syncer::LocalDeviceInfoProvider> local_device_;

  std::unique_ptr<syncer::DeviceInfoSyncBridge> device_info_sync_bridge_;

  // List of available data type controllers.
  syncer::DataTypeController::TypeMap data_type_controllers_;

  std::unique_ptr<syncer::NetworkResources> network_resources_;

  const StartBehavior start_behavior_;
  std::unique_ptr<syncer::StartupController> startup_controller_;

  std::unique_ptr<syncer::SyncStoppedReporter> sync_stopped_reporter_;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Whether the major version has changed since the last time Chrome ran,
  // and therefore a passphrase required state should result in prompting
  // the user. This logic is only enabled on platforms that consume the
  // IsPassphrasePrompted sync preference.
  bool passphrase_prompt_triggered_by_version_;

  // Whether sync is currently allowed on this platform.
  bool sync_allowed_by_platform_;

  // This weak factory invalidates its issued pointers when Sync is disabled.
  base::WeakPtrFactory<ProfileSyncService> sync_enabled_weak_factory_;

  base::WeakPtrFactory<ProfileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
