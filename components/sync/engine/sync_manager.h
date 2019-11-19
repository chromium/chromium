// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/sync/base/invalidation_interface.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/model_type_connector.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/syncable/change_record.h"
#include "url/gurl.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event
}  // namespace base

namespace syncer {

class BaseTransaction;
class CancelationSignal;
class DataTypeDebugInfoListener;
class EngineComponentsFactory;
class ExtensionsActivity;
class JsBackend;
class JsEventHandler;
class ProtocolEvent;
class SyncCycleSnapshot;
class TypeDebugInfoObserver;
class UnrecoverableErrorHandler;
struct UserShare;

namespace syncable {
class NigoriHandler;
}  // namespace syncable

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SyncManager {
 public:
  // An interface the embedding application implements to be notified
  // on change events.  Note that these methods may be called on *any*
  // thread.
  class ChangeDelegate {
   public:
    // Notify the delegate that changes have been applied to the sync model.
    //
    // This will be invoked on the same thread as on which ApplyChanges was
    // called. |changes| is an array of size |change_count|, and contains the
    // ID of each individual item that was changed. |changes| exists only for
    // the duration of the call. If items of multiple data types change at
    // the same time, this method is invoked once per data type and |changes|
    // is restricted to items of the ModelType indicated by |model_type|.
    // Because the observer is passed a |trans|, the observer can assume a
    // read lock on the sync model that will be released after the function
    // returns.
    //
    // The SyncManager constructs |changes| in the following guaranteed order:
    //
    // 1. Deletions, from leaves up to parents.
    // 2. Updates to existing items with synced parents & predecessors.
    // 3. New items with synced parents & predecessors.
    // 4. Items with parents & predecessors in |changes|.
    // 5. Repeat #4 until all items are in |changes|.
    //
    // Thus, an implementation of OnChangesApplied should be able to
    // process the change records in the order without having to worry about
    // forward dependencies.  But since deletions come before reparent
    // operations, a delete may temporarily orphan a node that is
    // updated later in the list.
    virtual void OnChangesApplied(ModelType model_type,
                                  int64_t model_version,
                                  const BaseTransaction* trans,
                                  const ImmutableChangeRecordList& changes) = 0;

    // OnChangesComplete gets called when the TransactionComplete event is
    // posted (after OnChangesApplied finishes), after the transaction lock
    // and the change channel mutex are released.
    //
    // The purpose of this function is to support processors that require
    // split-transactions changes. For example, if a model processor wants to
    // perform blocking I/O due to a change, it should calculate the changes
    // while holding the transaction lock (from within OnChangesApplied), buffer
    // those changes, let the transaction fall out of scope, and then commit
    // those changes from within OnChangesComplete (postponing the blocking
    // I/O to when it no longer holds any lock).
    virtual void OnChangesComplete(ModelType model_type) = 0;

   protected:
    virtual ~ChangeDelegate();
  };

  // Like ChangeDelegate, except called only on the sync thread and
  // not while a transaction is held.  For objects that want to know
  // when changes happen, but don't need to process them.
  class ChangeObserver {
   public:
    // Ids referred to in |changes| may or may not be in the write
    // transaction specified by |write_transaction_id|.  If they're
    // not, that means that the node didn't actually change, but we
    // marked them as changed for some other reason (e.g., siblings of
    // re-ordered nodes).
    //
    // TODO(sync, long-term): Ideally, ChangeDelegate/Observer would
    // be passed a transformed version of EntryKernelMutation instead
    // of a transaction that would have to be used to look up the
    // changed nodes.  That is, ChangeDelegate::OnChangesApplied()
    // would still be called under the transaction, but all the needed
    // data will be passed down.
    //
    // Even more ideally, we would have sync semantics such that we'd
    // be able to apply changes without being under a transaction.
    // But that's a ways off...
    virtual void OnChangesApplied(ModelType model_type,
                                  int64_t write_transaction_id,
                                  const ImmutableChangeRecordList& changes) = 0;

    virtual void OnChangesComplete(ModelType model_type) = 0;

   protected:
    virtual ~ChangeObserver();
  };

  // An interface the embedding application implements to receive
  // notifications from the SyncManager.  Register an observer via
  // SyncManager::AddObserver.  All methods are called only on the
  // sync thread.
  class Observer {
   public:
    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.
    virtual void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) = 0;

    // Called when the status of the connection to the sync server has
    // changed.
    virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

    // Called when initialization is complete to the point that SyncManager can
    // process changes. This does not necessarily mean authentication succeeded
    // or that the SyncManager is online.
    // IMPORTANT: Creating any type of transaction before receiving this
    // notification is illegal!
    // WARNING: Calling methods on the SyncManager before receiving this
    // message, unless otherwise specified, produces undefined behavior.

    virtual void OnInitializationComplete(
        const WeakHandle<JsBackend>& js_backend,
        const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
        bool success) = 0;

    virtual void OnActionableError(
        const SyncProtocolError& sync_protocol_error) = 0;

    virtual void OnMigrationRequested(ModelTypeSet types) = 0;

    virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

   protected:
    virtual ~Observer();
  };

  // Arguments for initializing SyncManager.
  struct InitArgs {
    InitArgs();
    ~InitArgs();

    // Path in which to create or open sync's sqlite database (aka the
    // directory).
    base::FilePath database_location;

    // Used to propagate events to chrome://sync-internals.  Optional.
    WeakHandle<JsEventHandler> event_handler;

    // URL of the sync server.
    GURL service_url;

    // Whether the local backend provided by the LoopbackServer should be used
    // and the location of the local sync backend storage.
    bool enable_local_sync_backend;
    base::FilePath local_sync_backend_folder;

    // Used to communicate with the sync server.
    std::unique_ptr<HttpPostProviderFactory> post_factory;

    std::vector<scoped_refptr<ModelSafeWorker>> workers;

    std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy;

    // Must outlive SyncManager.
    ExtensionsActivity* extensions_activity;

    // Must outlive SyncManager.
    ChangeDelegate* change_delegate;

    CoreAccountId authenticated_account_id;

    // Unqiuely identifies this client to the invalidation notification server.
    std::string invalidator_client_id;

    std::unique_ptr<EngineComponentsFactory> engine_components_factory;

    // Must outlive SyncManager.
    UserShare* user_share;

    // Must outlive SyncManager.
    SyncEncryptionHandler* encryption_handler;

    // Must outlive SyncManager.
    syncable::NigoriHandler* nigori_handler;

    WeakHandle<UnrecoverableErrorHandler> unrecoverable_error_handler;
    base::Closure report_unrecoverable_error_function;

    // Carries shutdown requests across threads and will be used to cut short
    // any network I/O and tell the syncer to exit early.
    //
    // Must outlive SyncManager.
    CancelationSignal* cancelation_signal;

    // Define the polling interval. Must not be zero.
    base::TimeDelta poll_interval;

    // Initial authoritative values (usually read from prefs).
    std::string cache_guid;
    std::string birthday;
    std::string bag_of_chips;
  };

  // The state of sync the feature. If the user turned on sync explicitly, it
  // will be set to ON. Will be set to INITIALIZING until we know the actual
  // state.
  enum class SyncFeatureState { INITIALIZING, ON, OFF };

  SyncManager();
  virtual ~SyncManager();

  // Initialize the sync manager using arguments from |args|.
  //
  // Note, args is passed by non-const pointer because it contains objects like
  // unique_ptr.
  virtual void Init(InitArgs* args) = 0;

  virtual ModelTypeSet InitialSyncEndedTypes() = 0;

  // Returns those types within |types| that have an empty progress marker
  // token.
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) = 0;

  // Purge from the directory those types with non-empty progress markers
  // but without initial synced ended set.
  // Returns false if an error occurred, true otherwise.
  virtual void PurgePartiallySyncedTypes() = 0;

  // Purge those disabled types as specified by |to_purge|. |to_journal| and
  // |to_unapply| specify subsets that require special handling. |to_journal|
  // types are saved into the delete journal, while |to_unapply| have only
  // their local data deleted, while their server data is preserved.
  virtual void PurgeDisabledTypes(ModelTypeSet to_purge,
                                  ModelTypeSet to_journal,
                                  ModelTypeSet to_unapply) = 0;

  // Update tokens that we're using in Sync. Email must stay the same.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // Clears the authentication tokens.
  virtual void InvalidateCredentials() = 0;

  // Put the syncer in normal mode ready to perform nudges and polls.
  virtual void StartSyncingNormally(base::Time last_poll_time) = 0;

  // Put syncer in configuration mode. Only configuration sync cycles are
  // performed. No local changes are committed to the server.
  virtual void StartConfiguration() = 0;

  // Switches the mode of operation to CONFIGURATION_MODE and performs
  // any configuration tasks needed as determined by the params. Once complete,
  // syncer will remain in CONFIGURATION_MODE until StartSyncingNormally is
  // called.
  // |ready_task| is invoked when the configuration completes.
  virtual void ConfigureSyncer(ConfigureReason reason,
                               ModelTypeSet to_download,
                               SyncFeatureState sync_feature_state,
                               const base::Closure& ready_task) = 0;

  // Inform the syncer of a change in the invalidator's state.
  virtual void SetInvalidatorEnabled(bool invalidator_enabled) = 0;

  // Inform the syncer that its cached information about a type is obsolete.
  virtual void OnIncomingInvalidation(
      ModelType type,
      std::unique_ptr<InvalidationInterface> invalidation) = 0;

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove the given observer.  Make sure to call this if the
  // Observer is being destroyed so the SyncManager doesn't
  // potentially dereference garbage.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Status-related getter.  May be called on any thread.
  virtual SyncStatus GetDetailedStatus() const = 0;

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  virtual void SaveChanges() = 0;

  // Issue a final SaveChanges, and close sqlite handles.
  virtual void ShutdownOnSyncThread() = 0;

  // May be called from any thread.
  virtual UserShare* GetUserShare() = 0;

  // Returns non-owning pointer to ModelTypeConnector. In contrast with
  // ModelTypeConnectorProxy all calls are executed synchronously, thus the
  // pointer should be used on sync thread.
  virtual ModelTypeConnector* GetModelTypeConnector() = 0;

  // Returns an instance of the main interface for registering sync types with
  // sync engine.
  virtual std::unique_ptr<ModelTypeConnector> GetModelTypeConnectorProxy() = 0;

  // Returns the cache_guid of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string cache_guid() = 0;

  // Returns the birthday of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string birthday() = 0;

  // Returns the bag of chips of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string bag_of_chips() = 0;

  // Returns whether there are remaining unsynced items.
  virtual bool HasUnsyncedItemsForTest() = 0;

  // Returns the SyncManager's encryption handler.
  virtual SyncEncryptionHandler* GetEncryptionHandler() = 0;

  // Ask the SyncManager to fetch updates for the given types.
  virtual void RefreshTypes(ModelTypeSet types) = 0;

  // Returns any buffered protocol events.  Does not clear the buffer.
  virtual std::vector<std::unique_ptr<ProtocolEvent>>
  GetBufferedProtocolEvents() = 0;

  // Functions to manage registrations of DebugInfoObservers.
  virtual void RegisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) = 0;
  virtual void UnregisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) = 0;
  virtual bool HasDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) = 0;

  // Request that all current counter values be emitted as though they had just
  // been updated.  Useful for initializing new observers' state.
  virtual void RequestEmitDebugInfo() = 0;

  // Updates Sync's tracking of whether the cookie jar has a mismatch with the
  // chrome account. See ClientConfigParams proto message for more info.
  // Note: this does not trigger a sync cycle. It just updates the sync context.
  virtual void OnCookieJarChanged(bool account_mismatch, bool empty_jar) = 0;

  // Adds memory usage statistics to |pmd| for chrome://tracing.
  virtual void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) = 0;

  // Updates invalidation client id.
  virtual void UpdateInvalidationClientId(const std::string& client_id) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_
