// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DEBUG_INFO_EVENT_LISTENER_H_
#define COMPONENTS_SYNC_ENGINE_DEBUG_INFO_EVENT_LISTENER_H_

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/cycle/debug_info_getter.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager.h"

namespace sync_pb {
class DebugEventInfo;
class DebugInfo;
class EncryptedData;
enum SyncEnums_SingletonDebugEventType : int;
}  // namespace sync_pb

namespace syncer {

// Listens to events and records them in a queue. And passes the events to
// syncer when requested.
// This class is not thread safe and should only be accessed on the sync thread.
class DebugInfoEventListener : public SyncManager::Observer,
                               public SyncEncryptionHandler::Observer,
                               public DebugInfoGetter {
 public:
  // Keep a few more events than there are data types, to ensure that any
  // data-type-specific events during first sync or startup aren't dropped.
  static constexpr const size_t kMaxEvents = GetNumDataTypes() + 10;

  DebugInfoEventListener();

  DebugInfoEventListener(const DebugInfoEventListener&) = delete;
  DebugInfoEventListener& operator=(const DebugInfoEventListener&) = delete;

  ~DebugInfoEventListener() override;

  void InitializationComplete();

  // SyncManager::Observer implementation.
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnConnectionStatusChange(ConnectionStatus connection_status) override;
  void OnActionableProtocolError(const SyncProtocolError& sync_error) override;
  void OnMigrationRequested(DataTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnSyncStatusChanged(const SyncStatus& status) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

  // Sync manager events.
  void OnNudgeFromDatatype(DataType datatype);

  // DebugInfoGetter implementation.
  sync_pb::DebugInfo GetDebugInfo() const override;

  // DebugInfoGetter implementation.
  void ClearDebugInfo() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyEventsAdded);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyQueueSize);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyGetEvents);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyClearEvents);

  void AddEventToQueue(const sync_pb::DebugEventInfo& event_info);
  void CreateAndAddEvent(sync_pb::SyncEnums_SingletonDebugEventType type);

  // Stores the most recent events, up to some limit.
  base::circular_deque<sync_pb::DebugEventInfo> events_;

  // Indicates whether any events had to be dropped because there were more than
  // the limit.
  bool events_dropped_ = false;

  // Cryptographer has keys that are not yet decrypted.
  bool cryptographer_has_pending_keys_ = false;

  // Cryptographer is able to encrypt data, which usually means it's initialized
  // and does not have pending keys.
  bool cryptographer_can_encrypt_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DEBUG_INFO_EVENT_LISTENER_H_
