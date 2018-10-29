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
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/sync_service_observer.h"

struct AccountInfo;
class GoogleServiceAuthError;
class GURL;

namespace sync_sessions {
class OpenTabsUIDelegate;
}  // namespace sync_sessions

namespace syncer {

class BaseTransaction;
class JsController;
class ProtocolEventObserver;
class SyncCycleSnapshot;
struct SyncTokenStatus;
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

class SyncService : public DataTypeEncryptionHandler, public KeyedService {
 public:
  // The set of reasons due to which Sync can be disabled. Meant to be used as a
  // bitmask.
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
    // Sync is suppressed by user choice, either via platform-level toggle (e.g.
    // Android's "ChromeSync" toggle), a “Reset Sync” operation from the
    // dashboard on desktop/ChromeOS.
    // NOTE: Other code paths that go through RequestStop also set this reason
    // (e.g. disabling due to sign-out or policy), so it's only really
    // meaningful when it's the *only* disable reason.
    // TODO(crbug.com/839834): Only set this reason when it's meaningful.
    DISABLE_REASON_USER_CHOICE = 1 << 3,
    // Sync has encountered an unrecoverable error. It won't attempt to start
    // again until either the browser is restarted, or the user fully signs out
    // and back in again.
    DISABLE_REASON_UNRECOVERABLE_ERROR = 1 << 4
  };

  // The overall state of the SyncService, in ascending order of "activeness".
  enum class TransportState {
    // Sync is inactive, e.g. due to enterprise policy, or simply because there
    // is no authenticated user.
    DISABLED,
    // Sync can start in principle, but nothing has prodded it to actually do it
    // yet. Note that during subsequent browser startups, Sync starts
    // automatically, i.e. no prod is necessary, but during the first start Sync
    // does need a kick. This usually happens via starting (not finishing!) the
    // initial setup, or via an explicit call to RequestStart.
    // TODO(crbug.com/839834): Check whether this state is necessary, or if Sync
    // can just always start up if all conditions are fulfilled (that's what
    // happens in practice anyway).
    WAITING_FOR_START_REQUEST,
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

  // Passed as an argument to RequestStop to control whether or not the sync
  // engine should clear its data directory when it shuts down. See
  // RequestStop for more information.
  enum SyncStopDataFate {
    KEEP_DATA,
    CLEAR_DATA,
  };

  ~SyncService() override {}

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
  virtual AccountInfo GetAuthenticatedAccountInfo() const = 0;
  // Whether the currently signed in user is the "primary" browser account (see
  // IdentityManager). If this is false, then IsSyncFeatureEnabled will also be
  // false, but Sync-the-transport might still run.
  virtual bool IsAuthenticatedAccountPrimary() const = 0;

  // The last authentication error that was encountered by the SyncService. This
  // error can be either from Chrome's identity system (e.g. while trying to get
  // an access token), or from the Sync server. It gets cleared when the error
  // is resolved.
  virtual const GoogleServiceAuthError& GetAuthError() const = 0;

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

  // Equivalent to having no disable reasons, i.e.
  // "GetDisableReasons() == DISABLE_REASON_NONE".
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
  // INITIAL SETUP / CONSENT
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if initial sync setup is in progress (does not return true
  // if the user is customizing sync after already completing setup once).
  // SyncService uses this to determine if it's OK to start syncing, or if the
  // user is still setting up the initial sync configuration.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be active
  // independent of first-setup state.
  bool IsFirstSetupInProgress() const;

  // Whether the user has completed the initial Sync setup. This does not mean
  // that sync is currently running (due to delayed startup, unrecoverable
  // errors, or shutdown). If you want to know whether Sync is actually running,
  // use GetTransportState or IsSyncFeatureActive instead.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be active
  // independent of first-setup state.
  virtual bool IsFirstSetupComplete() const = 0;

  // Called when Sync has been setup by the user and can be started.
  // Note: This refers to Sync-the-feature. Sync-the-transport may be active
  // independent of first-setup state.
  virtual void SetFirstSetupComplete() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // SETUP-IN-PROGRESS HANDLING
  //////////////////////////////////////////////////////////////////////////////

  // Called by the UI to notify the SyncService that UI is visible so it will
  // not start syncing. This tells sync whether it's safe to start downloading
  // data types yet (we don't start syncing until after sync setup is complete).
  // The UI calls this and holds onto the instance for as long as any part of
  // the signin wizard is displayed (even just the login UI).
  // When the last outstanding handle is deleted, this kicks off the sync engine
  // to ensure that data download starts.
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
  virtual ModelTypeSet GetPreferredDataTypes() const = 0;

  // Get the set of current active data types (those chosen or configured by
  // the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this
  // will the the empty set. Once the configuration completes the set will
  // be updated.
  virtual ModelTypeSet GetActiveDataTypes() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // ACTIONS / STATE CHANGE REQUESTS
  //////////////////////////////////////////////////////////////////////////////

  // The user requests that sync start. This only actually starts sync if
  // IsSyncAllowed is true and the user is signed in. Once sync starts,
  // other things such as IsFirstSetupComplete being false can still prevent
  // it from moving into the "active" state.
  virtual void RequestStart() = 0;

  // Stops sync at the user's request. |data_fate| controls whether the sync
  // engine should clear its data directory when it shuts down. Generally
  // KEEP_DATA is used when the user just stops sync, and CLEAR_DATA is used
  // when they sign out of the profile entirely.
  // Note: This refers to Sync-the-feature. Sync-the-transport may remain active
  // after calling this.
  virtual void RequestStop(SyncStopDataFate data_fate) = 0;

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  virtual void OnDataTypeRequestsSyncStartup(ModelType type) = 0;

  // Called when a user chooses which data types to sync. |sync_everything|
  // represents whether they chose the "keep everything synced" option; if
  // true, |chosen_types| will be ignored and all data types will be synced.
  // |sync_everything| means "sync all current and future data types."
  // |chosen_types| must be a subset of UserSelectableTypes().
  virtual void OnUserChoseDatatypes(bool sync_everything,
                                    ModelTypeSet chosen_types) = 0;

  // Triggers a GetUpdates call for the specified |types|, pulling any new data
  // from the sync server. Used by tests and debug UI (sync-internals).
  virtual void TriggerRefresh(const ModelTypeSet& types) = 0;

  // Attempts to re-enable a data type that is currently disabled due to a
  // data type error or an unready error. Note, this does not change the
  // preferred state of a datatype, and is not persisted across restarts.
  virtual void ReenableDatatype(ModelType type) = 0;

  // Informs the data type manager that the ready-for-start status of a
  // controller has changed. If the controller is not ready any more, it will
  // stop |type|. Otherwise, it will trigger reconfiguration so that |type| gets
  // started again.
  virtual void ReadyForStartChanged(ModelType type) = 0;

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
  // ENCRYPTION
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const = 0;

  // Returns the time the current explicit passphrase (if any), was set.
  // If no secondary passphrase is in use, or no time is available, returns an
  // unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const = 0;

  // Returns true if a secondary (explicit) passphrase is being used. It is not
  // legal to call this method before the engine is initialized.
  virtual bool IsUsingSecondaryPassphrase() const = 0;

  // Turns on encryption for all data. Callers must call OnUserChoseDatatypes()
  // after calling this to force the encryption to occur.
  virtual void EnableEncryptEverything() = 0;

  // Returns true if we are currently set to encrypt all the sync data.
  virtual bool IsEncryptEverythingEnabled() const = 0;

  // Asynchronously sets the passphrase to |passphrase| for encryption. |type|
  // specifies whether the passphrase is a custom passphrase or the GAIA
  // password being reused as a passphrase.
  virtual void SetEncryptionPassphrase(const std::string& passphrase) = 0;

  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT = 0;

  // Checks whether the Cryptographer is ready to encrypt and decrypt updates
  // for sensitive data types. Caller must be holding a syncer::BaseTransaction
  // to ensure thread safety.
  virtual bool IsCryptographerReady(const BaseTransaction* trans) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // ACCESS TO INNER OBJECTS
  //////////////////////////////////////////////////////////////////////////////

  // Return the active OpenTabsUIDelegate. If open/proxy tabs is not enabled or
  // not currently syncing, returns nullptr.
  virtual sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() = 0;

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  virtual UserShare* GetUserShare() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DETAILED STATE FOR DEBUG UI
  //////////////////////////////////////////////////////////////////////////////

  virtual SyncTokenStatus GetSyncTokenStatus() const = 0;

  // Initializes a struct of status indicators with data from the engine.
  // Returns false if the engine was not available for querying; in that case
  // the struct will be filled with default data.
  virtual bool QueryDetailedSyncStatus(SyncStatus* result) const = 0;

  virtual base::Time GetLastSyncedTime() const = 0;

  virtual SyncCycleSnapshot GetLastCycleSnapshot() const = 0;

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
  virtual std::unique_ptr<base::Value> GetTypeStatusMap() = 0;

  virtual const GURL& sync_service_url() const = 0;

  virtual std::string unrecoverable_error_message() const = 0;
  virtual base::Location unrecoverable_error_location() const = 0;

  virtual void AddProtocolEventObserver(ProtocolEventObserver* observer) = 0;
  virtual void RemoveProtocolEventObserver(ProtocolEventObserver* observer) = 0;

  virtual void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;
  virtual void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;

  // Returns a weak pointer to the service's JsController.
  virtual base::WeakPtr<JsController> GetJsController() = 0;

  // Asynchronously fetches base::Value representations of all sync nodes and
  // returns them to the specified callback on this thread.
  //
  // These requests can live a long time and return when you least expect it.
  // For safety, the callback should be bound to some sort of WeakPtr<> or
  // scoped_refptr<>.
  virtual void GetAllNodes(
      const base::Callback<void(std::unique_ptr<base::ListValue>)>&
          callback) = 0;

 protected:
  SyncService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
