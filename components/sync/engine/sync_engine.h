// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/engine/data_type_configurer.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "url/gurl.h"

namespace syncer {

class EngineComponentsFactory;
class HttpPostProviderFactory;
class KeyDerivationParams;
class Nigori;
class SyncEngineHost;
struct SyncStatus;

// The interface into the sync engine, which is the part of sync that performs
// communication between data types and the sync server.
// Lives on the UI thread.
class SyncEngine : public DataTypeConfigurer {
 public:
  using HttpPostProviderFactoryGetter =
      base::OnceCallback<std::unique_ptr<HttpPostProviderFactory>()>;

  // Utility struct for holding initialization options.
  struct InitParams {
    InitParams();

    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;

    InitParams(InitParams&& other);

    ~InitParams();

    raw_ptr<SyncEngineHost> host = nullptr;
    std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy;
    scoped_refptr<ExtensionsActivity> extensions_activity;
    GURL service_url;
    SyncEngine::HttpPostProviderFactoryGetter http_factory_getter;
    CoreAccountInfo authenticated_account_info;
    std::unique_ptr<SyncManagerFactory> sync_manager_factory;
    bool enable_local_sync_backend = false;
    base::FilePath local_sync_backend_folder;
    std::unique_ptr<EngineComponentsFactory> engine_components_factory;
  };

  SyncEngine();

  SyncEngine(const SyncEngine&) = delete;
  SyncEngine& operator=(const SyncEngine&) = delete;

  ~SyncEngine() override;

  // Kicks off asynchronous initialization. Optionally deletes sync data during
  // init in order to make sure we're starting fresh.
  //
  // |saved_nigori_state| is optional nigori state to restore from a previous
  // engine instance. May be null.
  virtual void Initialize(InitParams params) = 0;

  // Returns whether the asynchronous initialization process has finished.
  virtual bool IsInitialized() const = 0;

  // Inform the engine to trigger a sync cycle for |types|.
  virtual void TriggerRefresh(const DataTypeSet& types) = 0;

  // Updates the engine's SyncCredentials. The credentials must be fully
  // specified (account ID, email, and sync token). To invalidate the
  // credentials, use InvalidateCredentials() instead.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // Invalidates the SyncCredentials.
  virtual void InvalidateCredentials() = 0;

  // Transport metadata getters.
  virtual std::string GetCacheGuid() const = 0;
  virtual std::string GetBirthday() const = 0;
  virtual base::Time GetLastSyncedTimeForDebugging() const = 0;

  // Switches sync engine into configuration mode. In this mode only initial
  // data for newly enabled types is downloaded from server. No local changes
  // are committed to server.
  virtual void StartConfiguration() = 0;

  // This starts the sync engine running a Syncer object to communicate with
  // sync servers. Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  virtual void StartSyncingWithServer() = 0;

  // Starts handling incoming standalone invalidations. This method must be
  // called when data types are configured.
  virtual void StartHandlingInvalidations() = 0;

  // Asynchronously set a new passphrase for encryption. Note that it is an
  // error to call SetEncryptionPassphrase under the following circumstances:
  // - An explicit passphrase has already been set
  // - We have pending keys.
  virtual void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) = 0;

  // Use the provided decryption key to asynchronously attempt decryption. If
  // new encrypted keys arrive during the asynchronous call,
  // OnPassphraseRequired may be triggered at a later time. It is an error to
  // call this when there are no pending keys.
  virtual void SetExplicitPassphraseDecryptionKey(
      std::unique_ptr<Nigori> key) = 0;

  // Analogous to SetExplicitPassphraseDecryptionKey() but specifically for
  // TRUSTED_VAULT_PASSPHRASE: it provides new decryption keys that could
  // allow decrypting pending Nigori keys. Notifies observers of the result of
  // the operation via OnTrustedVaultKeyAccepted if the provided keys
  // successfully decrypted pending keys. |done_cb| is invoked at the very end.
  virtual void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys,
      base::OnceClosure done_cb) = 0;

  // Kick off shutdown procedure. Attempts to cut short any long-lived or
  // blocking sync thread tasks so that the shutdown on sync thread task that
  // we're about to post won't have to wait very long.
  virtual void StopSyncingForShutdown() = 0;

  // See the implementation and Core::DoShutdown for details.
  // Must be called *after* StopSyncingForShutdown.
  virtual void Shutdown(ShutdownReason reason) = 0;

  // Returns current detailed status information.
  virtual const SyncStatus& GetDetailedStatus() const = 0;

  // Determines if the underlying sync engine has made any local changes to
  // items that have not yet been synced with the server.
  // ONLY CALL THIS IF OnInitializationComplete was called!
  virtual void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const = 0;

  // Returns datatypes that are currently throttled.
  virtual void GetThrottledDataTypesForTest(
      base::OnceCallback<void(DataTypeSet)> cb) const = 0;

  // Requests that the backend forward to the fronent any protocol events in
  // its buffer and begin forwarding automatically from now on.  Repeated calls
  // to this function may result in the same events being emitted several
  // times.
  virtual void RequestBufferedProtocolEventsAndEnableForwarding() = 0;

  // Disables protocol event forwarding.
  virtual void DisableProtocolEventForwarding() = 0;

  // Notify the syncer that the cookie jar has changed.
  // See SyncManager::OnCookieJarChanged.
  virtual void OnCookieJarChanged(bool account_mismatch,
                                  base::OnceClosure callback) = 0;

  // Returns whether the poll interval elapsed since the last known poll time.
  // If returns true, there will likely be the next PERIODIC sync cycle soon but
  // it's not guaranteed, see SyncSchedulerImpl for details. Note that this may
  // diverge from a real scheduled poll time because this method uses base::Time
  // while scheduler uses base::TimeTicks (which may be paused in sleep mode).
  virtual bool IsNextPollTimeInThePast() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_H_
