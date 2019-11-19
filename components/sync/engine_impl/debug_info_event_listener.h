// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_DEBUG_INFO_EVENT_LISTENER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_DEBUG_INFO_EVENT_LISTENER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/data_type_debug_info_listener.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine_impl/cycle/debug_info_getter.h"
#include "components/sync/js/js_backend.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// In order to track datatype association results, we need at least as many
// entries as datatypes. Reserve additional space for other kinds of events that
// are likely to happen during first sync or startup.
const unsigned int kMaxEntries = ModelType::NUM_ENTRIES + 10;

// Listens to events and records them in a queue. And passes the events to
// syncer when requested.
// This class is not thread safe and should only be accessed on the sync thread.
class DebugInfoEventListener : public SyncManager::Observer,
                               public SyncEncryptionHandler::Observer,
                               public DebugInfoGetter,
                               public DataTypeDebugInfoListener {
 public:
  DebugInfoEventListener();
  ~DebugInfoEventListener() override;

  // SyncManager::Observer implementation.
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_listener,
      bool success) override;
  void OnConnectionStatusChange(ConnectionStatus connection_status) override;
  void OnActionableError(const SyncProtocolError& sync_error) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

  // Sync manager events.
  void OnNudgeFromDatatype(ModelType datatype);

  // DebugInfoGetter implementation.
  void GetDebugInfo(sync_pb::DebugInfo* debug_info) override;

  // DebugInfoGetter implementation.
  void ClearDebugInfo() override;

  // DataTypeDebugInfoListener implementation.
  void OnDataTypeConfigureComplete(
      const std::vector<DataTypeConfigurationStats>& configuration_stats)
      override;

  // Returns a weak pointer to this object.
  base::WeakPtr<DataTypeDebugInfoListener> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyEventsAdded);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyQueueSize);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyGetEvents);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyClearEvents);

  void AddEventToQueue(const sync_pb::DebugEventInfo& event_info);
  void CreateAndAddEvent(sync_pb::SyncEnums::SingletonDebugEventType type);

  using DebugEventInfoQueue = base::circular_deque<sync_pb::DebugEventInfo>;
  DebugEventInfoQueue events_;

  // True indicates we had to drop one or more events to keep our limit of
  // |kMaxEntries|.
  bool events_dropped_;

  // Cryptographer has keys that are not yet decrypted.
  bool cryptographer_has_pending_keys_;

  // Cryptographer is able to encrypt data, which usually means it's initialized
  // and does not have pending keys.
  bool cryptographer_can_encrypt_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DebugInfoEventListener> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DebugInfoEventListener);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_DEBUG_INFO_EVENT_LISTENER_H_
