// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

struct CoreAccountInfo;
class GoogleServiceAuthError;
class GURL;

namespace syncer {

struct LocalDataDescription;
class ProtocolEventObserver;
class SyncCycleSnapshot;
struct TypeEntitiesCount;
struct SyncTokenStatus;
class SyncUserSettings;
struct SyncStatus;

// UIs that need to prevent Sync startup should hold an instance of this class
// until the user has finished modifying sync settings. This is not an inner
// class of SyncService to enable forward declarations.
class SyncSetupInProgressHandle {
 public:
  // UIs should not construct this directly, but instead call
  // SyncService::GetSetupInProgress().
  explicit SyncSetupInProgressHandle(base::OnceClosure on_destroy);

  ~SyncSetupInProgressHandle();

  SyncSetupInProgressHandle(const SyncSetupInProgressHandle&) = delete;
  SyncSetupInProgressHandle& operator=(const SyncSetupInProgressHandle&) =
      delete;

 private:
  base::OnceClosure on_destroy_;
};

// SyncService is the layer between browser subsystems like bookmarks and the
// sync engine. Each subsystem is logically thought of as being a sync datatype.
// Individual datatypes can, at any point, be in a variety of stages of being
// "enabled". Here are some specific terms for concepts used in this class:
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
//      basis. The sync service will try to make active exactly these types.
//      If a user has opted out of syncing a particular datatype, it will
//      be registered, but not preferred. Also note that not all datatypes can
//      be directly chosen by the user: e.g. AUTOFILL_PROFILE is implied by
//      AUTOFILL but can't be selected separately. If AUTOFILL is chosen by the
//      user, then AUTOFILL_PROFILE will also be considered preferred. See
//      SyncPrefs::ResolvePrefGroups.
//
//      This state is controlled by SyncUserSettings::SetSelectedTypes. They
//      are stored in the preferences system and persist; though if a datatype
//      is not registered, it cannot be a preferred datatype.
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
//      in some intermediate state. Those finer-grained intermediate states
//      are differentiated by the DataTypeController state, but not exposed.
//
// Sync Configuration:
//
//   Sync configuration is accomplished via SyncUserSettings, in particular:
//    * SetSelectedTypes(): Set the data types the user wants to sync.
//    * SetDecryptionPassphrase(): Attempt to decrypt the user's encrypted data
//        using the passed passphrase.
//    * SetEncryptionPassphrase(): Re-encrypt the user's data using the passed
//        passphrase.
//
// Initial sync setup:
//
//   For privacy reasons, it is usually desirable to avoid syncing any data
//   types until the user has finished setting up sync. There are two APIs
//   that control the initial sync download:
//
//    * SyncUserSettings::SetInitialSyncFeatureSetupComplete()
//    * GetSetupInProgressHandle()
//
//   SetInitialSyncFeatureSetupComplete() should be called once the user has
//   finished setting up sync at least once on their account.
//   GetSetupInProgressHandle() should be called while the user is actively
//   configuring their account. The handle should be deleted once configuration
//   is complete.
//
//   Once first setup has completed and there are no outstanding
//   setup-in-progress handles, datatype configuration will begin.
class SyncService : public KeyedService {
 public:
  // The set of reasons due to which Sync can be disabled. These apply to both
  // sync-the-transport and sync-the-feature. Meant to be used as a enum set.
  enum DisableReason {
    // Sync is disabled by enterprise policy, either browser policy (through
    // prefs) or account policy received from the Sync server.
    DISABLE_REASON_ENTERPRISE_POLICY,
    DISABLE_REASON_FIRST = DISABLE_REASON_ENTERPRISE_POLICY,
    // Sync can't start because there is no authenticated user.
    DISABLE_REASON_NOT_SIGNED_IN,
    // Sync has encountered an unrecoverable error. It won't attempt to start
    // again until either the browser is restarted, or the user fully signs out
    // and back in again.
    DISABLE_REASON_UNRECOVERABLE_ERROR,
    DISABLE_REASON_LAST = DISABLE_REASON_UNRECOVERABLE_ERROR,
  };

  using DisableReasonSet =
      base::EnumSet<DisableReason, DISABLE_REASON_FIRST, DISABLE_REASON_LAST>;

  // The overall state of Sync-the-transport, in ascending order of
  // "activeness". Note that this refers to the transport layer, which may be
  // active even if Sync-the-feature is turned off.
  enum class TransportState {
    // Sync is inactive, e.g. due to enterprise policy, or simply because there
    // is no authenticated user.
    DISABLED,
    // Sync is paused because there is a persistent auth error (e.g. user signed
    // out on the web on desktop), and the engine is inactive.
    PAUSED,
    // Sync's startup was deferred, so that it doesn't slow down browser
    // startup. Once the deferral time (usually 10s) expires, or something
    // requests immediate startup, Sync will actually start.
    START_DEFERRED,
    // The Sync engine is in the process of initializing.
    INITIALIZING,
    // The Sync engine is initialized, but the process of configuring the data
    // types hasn't been started yet. This usually occurs if the user hasn't
    // completed the initial Sync setup yet (i.e.
    // IsInitialSyncFeatureSetupComplete() is
    // false), but it can also occur if a (non-initial) Sync setup happens to be
    // ongoing while the Sync service is starting up.
    PENDING_DESIRED_CONFIGURATION,
    // The Sync engine itself is up and running, but the individual data types
    // are being (re)configured. GetActiveDataTypes() will still be empty.
    CONFIGURING,
    // The Sync service is up and running. Note that this *still* doesn't
    // necessarily mean any particular data is being uploaded, e.g. individual
    // data types might be disabled or might have failed to start (check
    // GetActiveDataTypes()).
    ACTIVE
  };

  // Error states that prevent Sync from working well or working at all, usually
  // displayed to the user.
  // TODO(crbug.com/1412320): Add new cases that are missing, ideally unify with
  // other enums like AvatarSyncErrorType.
  enum class UserActionableError {
    // No errors.
    kNone,
    // There is a persistent auth error and the user needs to sign in for sync
    // to resume (affects all datatypes).
    kSignInNeedsUpdate,
    // The user needs to enter a passphrase in order to decrypt the data. This
    // can only happen to custom passphrase users and users in analogous legacy
    // encryption states. It affects most datatypes (all datatypes except the
    // ones that are never encrypted).
    kNeedsPassphrase,
    // The user needs to take action, usually go through a reauth challenge, in
    // order to get access to encryption keys. It affects datatypes that can be
    // branded to the user as 'passwords'.
    kNeedsTrustedVaultKeyForPasswords,
    // Same as above, but for the case where the encryption key is required to
    // sync all encryptable datatypes.
    kNeedsTrustedVaultKeyForEverything,
    // Recoverability degraded means sync actually works normally, but there is
    // a risk that the user may end up locked out and effectively lose access to
    // passwords stored in the Sync server.
    kTrustedVaultRecoverabilityDegradedForPasswords,
    // Same as above, but for the case where data loss may affect all
    // encryptable datatypes.
    kTrustedVaultRecoverabilityDegradedForEverything,
    // Same as DISABLE_REASON_UNRECOVERABLE_ERROR.
    // TODO(crbug.com/1412320): Consider removing this value and use disable
    // reasons instead.
    kGenericUnrecoverableError,
  };

  enum class ModelTypeDownloadStatus {
    // State is unknown or there are updates to download from the server. Data
    // types will be in this state until sync engine is initialized (or there is
    // a reason to disable sync). Note that sync initialization may be deferred,
    // the callers may use StartSyncFlare to start syncing ASAP.
    kWaitingForUpdates = 0,

    // There are no known server-side changes to download (local data is
    // up-to-date). Note that there is no guarantee that there are no new
    // updates (e.g. due to no internet connection, sync errors, etc).
    kUpToDate = 1,

    // This status includes any issues with the data type, e.g. if there are
    // bridge errors or it's throttled. In general, it means that the data
    // type is *not* necessarily up-to-date and the caller should not expect
    // this will be changed soon.
    kError = 2,
  };

  SyncService() = default;
  ~SyncService() override = default;

  SyncService(const SyncService&) = delete;
  SyncService& operator=(const SyncService&) = delete;

#if BUILDFLAG(IS_ANDROID)
  // Return the java object that allows access to the SyncService.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  //////////////////////////////////////////////////////////////////////////////
  // USER SETTINGS
  //////////////////////////////////////////////////////////////////////////////

  // Indicates the the user wants Sync-the-Feature to run. It should get invoked
  // early in the Sync setup flow, after the user has pressed "turn on Sync" but
  // before they have actually confirmed the settings.
  // TODO(crbug.com/1219990): Remove this API once the internal sync-requested
  // bit is fully removed and rollback/killswitch safe. Note that it also
  // requires finding an alternative solution to resolving
  // IsSyncFeatureDisabledViaDashboard(), tracked in crbug.com/1443446.
  virtual void SetSyncFeatureRequested() = 0;

  // Returns the SyncUserSettings, which encapsulate all the user-configurable
  // bits for Sync.
  virtual SyncUserSettings* GetUserSettings() = 0;
  virtual const SyncUserSettings* GetUserSettings() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // BASIC STATE ACCESS
  //////////////////////////////////////////////////////////////////////////////

  // Returns the set of reasons that are keeping Sync disabled, as a bitmask of
  // DisableReason enum entries.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be running
  // even in the presence of disable reasons.
  virtual DisableReasonSet GetDisableReasons() const = 0;
  // Helper that returns whether GetDisableReasons() contains the given |reason|
  // (possibly among others).
  bool HasDisableReason(DisableReason reason) const {
    return GetDisableReasons().Has(reason);
  }

  // Returns the overall state of the SyncService transport layer. See the enum
  // definition for what the individual states mean.
  // Note: This refers to Sync-the-transport, which may be active even if
  // Sync-the-feature is disabled by the user, by enterprise policy, etc.
  // Note: If your question is "Are we actually sending this data to Google?" or
  // "Am I allowed to send this type of data to Google?", you probably want
  // syncer::GetUploadToGoogleState instead of this.
  virtual TransportState GetTransportState() const = 0;

  // Returns errors that prevent SyncService from working at all or partially.
  // Usually these errors are displayed to the user in the UI.
  virtual UserActionableError GetUserActionableError() const = 0;

  // Returns true if the local sync backend server has been enabled through a
  // command line flag or policy. In this case sync is considered active but any
  // implied consent for further related services e.g. Suggestions, Web History
  // etc. is considered not granted.
  virtual bool IsLocalSyncEnabled() const = 0;

  // Information about the primary account. Note that this account doesn't
  // necessarily have Sync consent (in that case, only Sync-the-transport may be
  // running).
  virtual CoreAccountInfo GetAccountInfo() const = 0;
  // Whether the primary account has consented to Sync (see IdentityManager). If
  // this is false, then IsSyncFeatureEnabled will also be false, but
  // Sync-the-transport might still run.
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  virtual bool HasSyncConsent() const = 0;

  // Returns whether the SyncService has completed at least one Sync cycle since
  // starting up (i.e. since browser startup or signin). This can be useful
  // in combination with GetAuthError(), if you need to know if the user's
  // refresh token is really valid: Before a Sync cycle has been completed,
  // Sync hasn't tried using the refresh token, so doesn't know if it's valid.
  // TODO(crbug.com/831579): If Chrome would persist auth errors, this would not
  // be necessary.
  bool HasCompletedSyncCycle() const;

  // The last persistent authentication error that was encountered by the
  // SyncService. It gets cleared when the error is resolved.
  virtual GoogleServiceAuthError GetAuthError() const = 0;
  virtual base::Time GetAuthErrorTime() const = 0;

  // Returns true if the Chrome client is too old and needs to be updated for
  // Sync to work.
  // TODO(crbug.com/1412320): Remove this API and use GetUserActionableError()
  // instead.
  virtual bool RequiresClientUpgrade() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DERIVED STATE ACCESS
  //////////////////////////////////////////////////////////////////////////////

  // Returns whether all conditions are satisfied for Sync-the-feature to start.
  // This means that there is a Sync-consented account, no disable reasons, and
  // first-time Sync setup has been completed by the user.
  // Note: This does not imply that Sync is actually running. Check
  // IsSyncFeatureActive or GetTransportState to get the current state.
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  bool IsSyncFeatureEnabled() const;

  // Equivalent to "HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)".
  bool HasUnrecoverableError() const;

  // Equivalent to GetTransportState() returning one of
  // PENDING_DESIRED_CONFIGURATION, CONFIGURING, or ACTIVE.
  // Note: This refers to Sync-the-transport, which may be active even if
  // Sync-the-feature is disabled by the user, by enterprise policy, etc.
  bool IsEngineInitialized() const;

  // Returns whether Sync-the-feature can (attempt to) start. This means that
  // there is a Sync-consented account and no disable reasons. It does *not*
  // require first-time Sync setup to be complete, because that can only happen
  // after the engine has started.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be running
  // even if this is false.
  // TODO(crbug.com/1444344): Remove this API, in favor of
  // IsSyncFeatureEnabled().
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  bool CanSyncFeatureStart() const;

  // Returns whether Sync-the-feature is active, which means
  // GetTransportState() is either CONFIGURING or ACTIVE and
  // IsSyncFeatureEnabled() is true.
  // To see which datatypes are actually syncing, see GetActiveDataTypes().
  // Note: This refers to Sync-the-feature. Sync-the-transport may be active
  // even if this is false.
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  bool IsSyncFeatureActive() const;

  //////////////////////////////////////////////////////////////////////////////
  // SETUP-IN-PROGRESS HANDLING
  //////////////////////////////////////////////////////////////////////////////

  // Called by the UI to notify the SyncService that UI is visible, so any
  // changes to Sync settings should *not* take effect immediately (e.g. if the
  // user accidentally enabled a data type, we should give them a chance to undo
  // the change before local and remote data are irrevocably merged).
  // The UI calls this and holds onto the instance for as long as any part of
  // the Sync setup/configuration UI is visible.
  virtual std::unique_ptr<SyncSetupInProgressHandle>
  GetSetupInProgressHandle() = 0;

  // Whether a Sync setup is currently in progress, i.e. a setup UI is being
  // shown.
  virtual bool IsSetupInProgress() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DATA TYPE STATE
  //////////////////////////////////////////////////////////////////////////////

  // Returns the set of types which are preferred for enabling. This is a
  // superset of the active types (see GetActiveDataTypes()).
  // TODO(crbug.com/1429249): Deprecated, DO NOT USE! You probably want
  // `GetUserSettings()->GetSelectedTypes()` instead.
  virtual ModelTypeSet GetPreferredDataTypes() const = 0;

  // Returns the set of currently active data types (those chosen or configured
  // by the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this will
  // be the empty set. Once the configuration completes the set will be updated.
  virtual ModelTypeSet GetActiveDataTypes() const = 0;

  // Returns the datatypes that are about to become active, but are currently
  // in the process of downloading the initial data from the server (either
  // actively ongoing or queued). Note that it is not always feasible to
  // determine this reliably (e.g. during initialization) and hence the
  // implementation may return a sensible likely value.
  virtual ModelTypeSet GetTypesWithPendingDownloadForInitialSync() const = 0;

  // Returns the datatypes which have local changes that have not yet been
  // synced with the server.
  // Note: This only queries the datatypes in `requested_types`.
  // Note: This includes deletions as well.
  virtual void GetTypesWithUnsyncedData(
      ModelTypeSet requested_types,
      base::OnceCallback<void(ModelTypeSet)> callback) const = 0;

  // Queries the count and description/preview of existing local data for
  // `types` data types. This is an asynchronous method which returns the result
  // via the callback `callback` once the information for all the data types in
  // `types` is available.
  // Note: Only data types that are enabled and support this functionality are
  // part of the response.
  virtual void GetLocalDataDescriptions(
      ModelTypeSet types,
      base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
          callback) = 0;

  // Requests sync service to move all local data to account for `types` data
  // types. This is an asynchronous method which moves the local data for all
  // `types` to the account store locally. Upload to the server will happen as
  // part of the regular commit process, and is NOT part of this method.
  // Note: Only data types that are enabled and support this functionality are
  // triggered for upload.
  virtual void TriggerLocalDataMigration(ModelTypeSet types) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // ACTIONS / STATE CHANGE REQUESTS
  //////////////////////////////////////////////////////////////////////////////

  // Stops and disables Sync-the-feature and clears all local data.
  // Sync-the-transport may remain active after calling this.
  virtual void StopAndClear() = 0;

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  // TODO(crbug.com/1429293): Remove this API.
  virtual void OnDataTypeRequestsSyncStartup(ModelType type) = 0;

  // Triggers a GetUpdates call for the specified |types|, pulling any new data
  // from the sync server. Used by tests and debug UI (sync-internals).
  virtual void TriggerRefresh(const ModelTypeSet& types) = 0;

  // Informs the data type manager that the preconditions for a controller have
  // changed. If preconditions are NOT met, the datatype will be stopped
  // according to the metadata clearing policy returned by the controller's
  // GetPreconditionState(). Otherwise, if preconditions are newly met,
  // reconfiguration will be triggered so that |type| gets started again. No-op
  // if the type's state didn't actually change.
  virtual void DataTypePreconditionChanged(ModelType type) = 0;

  // Enables/disables invalidations for session sync related datatypes.
  // The session sync generates a lot of changes, which results in many
  // invalidations. This can negatively affect the battery life on Android. For
  // that reason, on Android, the invalidations for sessions should be received
  // only when user is interested in session sync data, e.g. the history sync
  // page is opened.
  virtual void SetInvalidationsForSessionsEnabled(bool enabled) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // OBSERVERS
  //////////////////////////////////////////////////////////////////////////////

  // Adds/removes an observer. SyncService does not take ownership of the
  // observer.
  virtual void AddObserver(SyncServiceObserver* observer) = 0;
  virtual void RemoveObserver(SyncServiceObserver* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(const SyncServiceObserver* observer) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DETAILED STATE FOR DEBUG UI
  //////////////////////////////////////////////////////////////////////////////

  // Returns the state of the access token and token request, for display in
  // internals UI.
  virtual SyncTokenStatus GetSyncTokenStatusForDebugging() const = 0;

  // Initializes a struct of status indicators with data from the engine.
  // Returns false if the engine was not available for querying; in that case
  // the struct will be filled with default data.
  virtual bool QueryDetailedSyncStatusForDebugging(
      SyncStatus* result) const = 0;

  virtual base::Time GetLastSyncedTimeForDebugging() const = 0;

  // Returns some statistics on the most-recently completed sync cycle.
  virtual SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const = 0;

  // Returns a Value indicating the status of all registered types.
  //
  // The format is:
  // [ {"name": <name>, "value": <value>, "status": <status> }, ... ]
  // where <name> is a type's name, <value> is a string providing details for
  // the type's status, and <status> is one of "error", "warning" or "ok"
  // depending on the type's current status.
  //
  // This function is used by sync_internals_util.cc to help populate the
  // chrome://sync-internals page.  It returns a Value::List rather than a
  // Value::Dict in part to make it easier to iterate over its elements when
  // constructing that page.
  virtual base::Value::List GetTypeStatusMapForDebugging() const = 0;

  // Retrieves the TypeEntitiesCount for all registered data types.
  virtual void GetEntityCountsForDebugging(
      base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
      const = 0;

  virtual const GURL& GetSyncServiceUrlForDebugging() const = 0;

  virtual std::string GetUnrecoverableErrorMessageForDebugging() const = 0;
  virtual base::Location GetUnrecoverableErrorLocationForDebugging() const = 0;

  virtual void AddProtocolEventObserver(ProtocolEventObserver* observer) = 0;
  virtual void RemoveProtocolEventObserver(ProtocolEventObserver* observer) = 0;

  // Asynchronously fetches base::Value representations of all sync nodes and
  // returns them to the specified callback on this thread.
  //
  // These requests can live a long time and return when you least expect it.
  // For safety, the callback should be bound to some sort of WeakPtr<> or
  // scoped_refptr<>.
  virtual void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) = 0;

  // Returns current download status for the given |type|. The caller can use
  // SyncServiceObserver::OnStateChanged() to track status changes. Must be
  // called for real data types only.
  virtual ModelTypeDownloadStatus GetDownloadStatusFor(
      ModelType type) const = 0;

  // TODO(crbug.com/1425071): remove once investigation of timeouts complete.
  // Records the reason if the `type` is waiting for updates to be downloaded.
  virtual void RecordReasonIfWaitingForUpdates(
      ModelType type,
      const std::string& histogram_name) const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_H_
