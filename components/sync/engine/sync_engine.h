// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_backend_registrar.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_manager_factory.h"

class GURL;

namespace syncer {

class CancelationSignal;
class HttpPostProviderFactory;
class SyncEngineHost;
class SyncManagerFactory;
class UnrecoverableErrorHandler;

// The interface into the sync engine, which is the part of sync that performs
// communication between model types and the sync server. In prod the engine
// will always live on the sync thread and the object implementing this
// interface will handle crossing threads if necessary.
class SyncEngine : public ModelTypeConfigurer {
 public:
  using Status = SyncStatus;
  using HttpPostProviderFactoryGetter =
      base::OnceCallback<std::unique_ptr<HttpPostProviderFactory>(
          CancelationSignal*)>;

  // Utility struct for holding initialization options.
  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);
    ~InitParams();

    scoped_refptr<base::SequencedTaskRunner> sync_task_runner;
    SyncEngineHost* host = nullptr;
    std::unique_ptr<SyncBackendRegistrar> registrar;
    std::vector<std::unique_ptr<SyncEncryptionHandler::Observer>>
        encryption_observer_proxies;
    scoped_refptr<ExtensionsActivity> extensions_activity;
    WeakHandle<JsEventHandler> event_handler;
    GURL service_url;
    std::string sync_user_agent;
    SyncEngine::HttpPostProviderFactoryGetter http_factory_getter;
    SyncCredentials credentials;
    std::string invalidator_client_id;
    std::unique_ptr<SyncManagerFactory> sync_manager_factory;
    bool delete_sync_data_folder = false;
    bool enable_local_sync_backend = false;
    base::FilePath local_sync_backend_folder;
    std::string restored_key_for_bootstrapping;
    std::string restored_keystore_key_for_bootstrapping;
    std::unique_ptr<EngineComponentsFactory> engine_components_factory;
    WeakHandle<UnrecoverableErrorHandler> unrecoverable_error_handler;
    base::Closure report_unrecoverable_error_function;
    std::unique_ptr<SyncEncryptionHandler::NigoriState> saved_nigori_state;
    std::map<ModelType, int64_t> invalidation_versions;

    // Define the polling intervals. Must not be zero.
    base::TimeDelta short_poll_interval;
    base::TimeDelta long_poll_interval;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  SyncEngine();
  ~SyncEngine() override;

  // Kicks off asynchronous initialization. Optionally deletes sync data during
  // init in order to make sure we're starting fresh.
  //
  // |saved_nigori_state| is optional nigori state to restore from a previous
  // engine instance. May be null.
  virtual void Initialize(InitParams params) = 0;

  // Inform the engine to trigger a sync cycle for |types|.
  virtual void TriggerRefresh(const ModelTypeSet& types) = 0;

  // Updates the engine's SyncCredentials. The credentials must be fully
  // specified (account ID, email, and sync token). To invalidate the
  // credentials, use InvalidateCredentials() instead.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // Invalidates the SyncCredentials.
  virtual void InvalidateCredentials() = 0;

  // Switches sync engine into configuration mode. In this mode only initial
  // data for newly enabled types is downloaded from server. No local changes
  // are committed to server.
  virtual void StartConfiguration() = 0;

  // This starts the sync engine running a Syncer object to communicate with
  // sync servers. Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  virtual void StartSyncingWithServer() = 0;

  // Asynchronously set a new passphrase for encryption. Note that it is an
  // error to call SetEncryptionPassphrase under the following circumstances:
  // - An explicit passphrase has already been set
  // - We have pending keys.
  virtual void SetEncryptionPassphrase(const std::string& passphrase) = 0;

  // Use the provided passphrase to asynchronously attempt decryption. If new
  // encrypted keys arrive during the asynchronous call, OnPassphraseRequired
  // may be triggered at a later time. It is an error to call this when there
  // are no pending keys.
  virtual void SetDecryptionPassphrase(const std::string& passphrase) = 0;

  // Kick off shutdown procedure. Attempts to cut short any long-lived or
  // blocking sync thread tasks so that the shutdown on sync thread task that
  // we're about to post won't have to wait very long.
  virtual void StopSyncingForShutdown() = 0;

  // See the implementation and Core::DoShutdown for details.
  // Must be called *after* StopSyncingForShutdown.
  virtual void Shutdown(ShutdownReason reason) = 0;

  // Turns on encryption of all present and future sync data.
  virtual void EnableEncryptEverything() = 0;

  // Obtain a handle to the UserShare needed for creating transactions. Should
  // not be called before we signal initialization is complete with
  // OnBackendInitialized().
  virtual UserShare* GetUserShare() const = 0;

  // Called from any thread to obtain current detailed status information.
  virtual Status GetDetailedStatus() = 0;

  // Determines if the underlying sync engine has made any local changes to
  // items that have not yet been synced with the server.
  // ONLY CALL THIS IF OnInitializationComplete was called!
  virtual void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const = 0;

  // True if the cryptographer has any keys available to attempt decryption.
  // Could mean we've downloaded and loaded Nigori objects, or we bootstrapped
  // using a token previously received.
  virtual bool IsCryptographerReady(const BaseTransaction* trans) const = 0;

  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const = 0;

  // Send a message to the sync thread to persist the Directory to disk.
  virtual void FlushDirectory() const = 0;

  // Requests that the backend forward to the fronent any protocol events in
  // its buffer and begin forwarding automatically from now on.  Repeated calls
  // to this function may result in the same events being emitted several
  // times.
  virtual void RequestBufferedProtocolEventsAndEnableForwarding() = 0;

  // Disables protocol event forwarding.
  virtual void DisableProtocolEventForwarding() = 0;

  // Enables the sending of directory type debug counters.  Also, for every
  // time it is called, it makes an explicit request that updates to an update
  // for all counters be emitted.
  virtual void EnableDirectoryTypeDebugInfoForwarding() = 0;

  // Disables the sending of directory type debug counters.
  virtual void DisableDirectoryTypeDebugInfoForwarding() = 0;

  // See SyncManager::ClearServerData.
  virtual void ClearServerData(const base::Closure& callback) = 0;

  // Notify the syncer that the cookie jar has changed.
  // See SyncManager::OnCookieJarChanged.
  virtual void OnCookieJarChanged(bool account_mismatch,
                                  bool empty_jar,
                                  const base::Closure& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_
