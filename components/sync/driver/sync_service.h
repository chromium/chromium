// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/driver/sync_service_observer.h"

struct CoreAccountInfo;
class GoogleServiceAuthError;
class GURL;

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace syncer {

class JsController;
class ProtocolEventObserver;
class SyncCycleSnapshot;
struct SyncTokenStatus;
class SyncUserSettings;
class TypeDebugInfoObserver;
struct SyncStatus;
struct UserShare;

// UIs that need to prevent Sync startup should hold an instance of this class
// until the user has finished modifying sync settings. This is not an inner
// class of SyncService to enable forward declarations.
class SyncSetupInProgressHandle {
 public:
  // UIs should not construct this directly, but instead call
  // SyncService::GetSetupInProgress().
  explicit SyncSetupInProgressHandle(base::Closure on_destroy);

  ~SyncSetupInProgressHandle();

 private:
  base::Closure on_destroy_;
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
//      This state is controlled by SyncUserSettings::SetChosenDataTypes. They
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
//    * SetChosenDataTypes(): Set the data types the user wants to sync.
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
//    * SyncUserSettings::SetFirstSetupComplete()
//    * GetSetupInProgressHandle()
//
//   SetFirstSetupComplete() should be called once the user has finished setting
//   up sync at least once on their account. GetSetupInProgressHandle() should
//   be called while the user is actively configuring their account. The handle
//   should be deleted once configuration is complete.
//
//   Once first setup has completed and there are no outstanding
//   setup-in-progress handles, datatype configuration will begin.
class SyncService : public KeyedService {
 public:
  // The set of reasons due to which Sync-the-feature can be disabled. Note that
  // Sync-the-transport might still start up even in the presence of (some)
  // disable reasons. Meant to be used as a bitmask.
  enum DisableReason {
    DISABLE_REASON_NONE = 0,
    // Sync is disabled via platform-level override (e.g. Android's "MasterSync"
    // toggle).
    DISABLE_REASON_PLATFORM_OVERRIDE = 1 << 0,
    // Sync is disabled by enterprise policy, either browser policy (through
    // prefs) or account policy received from the Sync server.
    DISABLE_REASON_ENTERPRISE_POLICY = 1 << 1,
    // Sync can't start because there is no authenticated user.
    DISABLE_REASON_NOT_SIGNED_IN = 1 << 2,
    // Sync is suppressed by user choice, either via the feature toggle in
    // Chrome settings (which exists on Android and iOS), a platform-level
    // toggle (e.g. Android's "ChromeSync" toggle), or a “Reset Sync” operation
    // from the dashboard. This is also set if there's simply no signed-in user
    // (in addition to DISABLE_REASON_NOT_SIGNED_IN).
    DISABLE_REASON_USER_CHOICE = 1 << 3,
    // Sync has encountered an unrecoverable error. It won't attempt to start
    // again until either the browser is restarted, or the user fully signs out
    // and back in again.
    DISABLE_REASON_UNRECOVERABLE_ERROR = 1 << 4,
    // Sync is paused because the user signed out on the web. This is different
    // from NOT_SIGNED_IN: In this case, there *is* still a primary account, but
    // it doesn't have valid credentials.
    DISABLE_REASON_PAUSED = 1 << 5,
  };

  // The overall state of Sync-the-transport, in ascending order of
  // "activeness". Note that this refers to the transport layer, which may be
  // active even if Sync-the-feature is turned off.
  enum class TransportState {
    // Sync is inactive, e.g. due to enterprise policy, or simply because there
    // is no authenticated user.
    DISABLED,
    // Sync's startup was deferred, so that it doesn't slow down browser
    // startup. Once the deferral time (usually 10s) expires, or something
    // requests immediate startup, Sync will actually start.
    START_DEFERRED,
    // The Sync engine is in the process of initializing.
    INITIALIZING,
    // The Sync engine is initialized, but the process of configuring the data
    // types hasn't been started yet. This usually occurs if the user hasn't
    // completed the initial Sync setup yet (i.e. IsFirstSetupComplete() is
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

  ~SyncService() override {}

  //////////////////////////////////////////////////////////////////////////////
  // USER SETTINGS
  //////////////////////////////////////////////////////////////////////////////

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
  virtual int GetDisableReasons() const = 0;
  // Helper that returns whether GetDisableReasons() contains the given |reason|
  // (possibly among others).
  bool HasDisableReason(DisableReason reason) const {
    return GetDisableReasons() & reason;
  }

  // Returns the overall state of the SyncService transport layer. See the enum
  // definition for what the individual states mean.
  // Note: This refers to Sync-the-transport, which may be active even if
  // Sync-the-feature is disabled by the user, by enterprise policy, etc.
  // Note: If your question is "Are we actually sending this data to Google?" or
  // "Am I allowed to send this type of data to Google?", you probably want
  // syncer::GetUploadToGoogleState instead of this.
  virtual TransportState GetTransportState() const = 0;

  // Returns true if the local sync backend server has been enabled through a
  // command line flag or policy. In this case sync is considered active but any
  // implied consent for further related services e.g. Suggestions, Web History
  // etc. is considered not granted.
  virtual bool IsLocalSyncEnabled() const = 0;

  // Information about the currently signed in user.
  virtual CoreAccountInfo GetAuthenticatedAccountInfo() const = 0;
  // Whether the currently signed in user is the "primary" browser account (see
  // IdentityManager). If this is false, then IsSyncFeatureEnabled will also be
  // false, but Sync-the-transport might still run.
  virtual bool IsAuthenticatedAccountPrimary() const = 0;

  // Returns whether the SyncService has completed at least one Sync cycle since
  // starting up (i.e. since browser startup or signin). This can be useful
  // in combination with GetAuthError(), if you need to know if the user's
  // refresh token is really valid: Before a Sync cycle has been completed,
  // Sync hasn't tried using the refresh token, so doesn't know if it's valid.
  // TODO(crbug.com/831579): If Chrome would persist auth errors, this would not
  // be necessary.
  bool HasCompletedSyncCycle() const;

  // The last authentication error that was encountered by the SyncService. This
  // error can be either from Chrome's identity system (e.g. while trying to get
  // an access token), or from the Sync server. It gets cleared when the error
  // is resolved.
  virtual GoogleServiceAuthError GetAuthError() const = 0;
  virtual base::Time GetAuthErrorTime() const = 0;

  // Returns true if the Chrome client is too old and needs to be updated for
  // Sync to work.
  virtual bool RequiresClientUpgrade() const = 0;

  // Returns a high-entropy elliptic curve (EC) private key that is unique to a
  // user and sync-ed across devices via Nigori. Populated when the transport
  // state becomes CONFIGURING. Returns nullptr if not available. Consumers of
  // this key should observe for changes via
  // SyncServiceObserver::OnSyncCycleCompleted().
  // TODO(crbug.com/1012226): Remove when VAPID migration is over.
  virtual std::unique_ptr<crypto::ECPrivateKey>
  GetExperimentalAuthenticationKey() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DERIVED STATE ACCESS
  //////////////////////////////////////////////////////////////////////////////

  // Returns whether all conditions are satisfied for Sync-the-feature to start.
  // This means that there is a primary account, no disable reasons, and
  // first-time Sync setup has been completed by the user.
  // Note: This does not imply that Sync is actually running. Check
  // IsSyncFeatureActive or GetTransportState to get the current state.
  bool IsSyncFeatureEnabled() const;

  // Equivalent to "HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)".
  bool HasUnrecoverableError() const;

  // Equivalent to GetTransportState() returning one of
  // PENDING_DESIRED_CONFIGURATION, CONFIGURING, or ACTIVE.
  // Note: This refers to Sync-the-transport, which may be active even if
  // Sync-the-feature is disabled by the user, by enterprise policy, etc.
  bool IsEngineInitialized() const;

  // Returns whether Sync-the-feature can (attempt to) start. This means that
  // there is a primary account and no disable reasons. It does *not* require
  // first-time Sync setup to be complete, because that can only happen after
  // the engine has started.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be running
  // even if this is false.
  bool CanSyncFeatureStart() const;

  // Returns whether Sync-the-feature is active, which means
  // GetTransportState() is either CONFIGURING or ACTIVE and
  // IsSyncFeatureEnabled() is true.
  // To see which datatypes are actually syncing, see GetActiveDataTypes().
  // Note: This refers to Sync-the-feature. Sync-the-transport may be active
  // even if this is false.
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

  // Returns the set of data types that are supported in principle. These will
  // typically only change via a command-line option.
  virtual syncer::ModelTypeSet GetRegisteredDataTypes() const = 0;

  // Returns the set of types which are preferred for enabling. This is a
  // superset of the active types (see GetActiveDataTypes()). This also includes
  // any forced types.
  virtual ModelTypeSet GetPreferredDataTypes() const = 0;

  // Returns the set of currently active data types (those chosen or configured
  // by the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this will
  // be the empty set. Once the configuration completes the set will be updated.
  virtual ModelTypeSet GetActiveDataTypes() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // ACTIONS / STATE CHANGE REQUESTS
  //////////////////////////////////////////////////////////////////////////////

  // Stops sync and clears all local data. This usually gets called when the
  // user fully signs out (i.e. removes the primary account).
  // Note: This refers to Sync-the-feature. Sync-the-transport may remain active
  // after calling this.
  virtual void StopAndClear() = 0;

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
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
  // USER DEMOGRAPHICS
  //////////////////////////////////////////////////////////////////////////////

  // Gets the synced user’s noised birth year and gender, see doc of
  // metrics::DemographicMetricsProvider in
  // components/metrics/demographic_metrics_provider.h for more details. Returns
  // an error status with an empty value when the user's birth year or gender
  // cannot be provided. You need to provide an accurate |now| time that
  // represents the current time.
  virtual UserDemographicsResult GetUserNoisedBirthYearAndGender(
      base::Time now) = 0;

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
  // ACCESS TO INNER OBJECTS
  //////////////////////////////////////////////////////////////////////////////

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  virtual UserShare* GetUserShare() const = 0;

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

  // Returns a ListValue indicating the status of all registered types.
  //
  // The format is:
  // [ {"name": <name>, "value": <value>, "status": <status> }, ... ]
  // where <name> is a type's name, <value> is a string providing details for
  // the type's status, and <status> is one of "error", "warning" or "ok"
  // depending on the type's current status.
  //
  // This function is used by about_sync_util.cc to help populate the about:sync
  // page.  It returns a ListValue rather than a DictionaryValue in part to make
  // it easier to iterate over its elements when constructing that page.
  virtual std::unique_ptr<base::Value> GetTypeStatusMapForDebugging() = 0;

  virtual const GURL& GetSyncServiceUrlForDebugging() const = 0;

  virtual std::string GetUnrecoverableErrorMessageForDebugging() const = 0;
  virtual base::Location GetUnrecoverableErrorLocationForDebugging() const = 0;

  virtual void AddProtocolEventObserver(ProtocolEventObserver* observer) = 0;
  virtual void RemoveProtocolEventObserver(ProtocolEventObserver* observer) = 0;

  virtual void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;
  virtual void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;

  virtual base::WeakPtr<JsController> GetJsController() = 0;

  // Asynchronously fetches base::Value representations of all sync nodes and
  // returns them to the specified callback on this thread.
  //
  // These requests can live a long time and return when you least expect it.
  // For safety, the callback should be bound to some sort of WeakPtr<> or
  // scoped_refptr<>.
  virtual void GetAllNodesForDebugging(
      const base::Callback<void(std::unique_ptr<base::ListValue>)>&
          callback) = 0;

 protected:
  SyncService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
