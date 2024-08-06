// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_

#include <string>

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// Status encapsulates detailed state about the internals of the SyncManager.
//
// This struct is closely tied to the AllStatus object which uses instances of
// it to track and report on the sync engine's internal state, and the functions
// in sync_ui_util.cc which convert the contents of this struct into a
// base::Value::Dict used to populate the chrome://sync-internals summary tab.
struct SyncStatus {
  SyncStatus();
  SyncStatus(const SyncStatus& other);
  ~SyncStatus();

  // TODO(akalin): Replace this with a NotificationsDisabledReason
  // variable.
  // True only if subscribed for notifications.
  bool notifications_enabled = false;

  // Notifications counters updated by the actions in synapi.
  int notifications_received = 0;

  SyncProtocolError sync_protocol_error;

  // Number of items the server refused to commit due to conflict during most
  // recent sync cycle.
  int server_conflicts = 0;

  // Number of items successfully committed during most recent sync cycle.
  int committed_count = 0;

  // Whether a sync cycle is going on right now.
  bool syncing = false;

  // Total updates received by the syncer since browser start.
  int updates_received = 0;
  // Of updates_received, how many were tombstones.
  int tombstone_updates_received = 0;

  // Total successful commits.
  int num_commits_total = 0;

  // Encryption related.
  DataTypeSet encrypted_types;
  bool cryptographer_can_encrypt = false;
  bool crypto_has_pending_keys = false;
  bool has_keystore_key = false;
  base::Time keystore_migration_time;
  PassphraseType passphrase_type = PassphraseType::kImplicitPassphrase;
  sync_pb::NigoriSpecifics::TrustedVaultDebugInfo trusted_vault_debug_info;

  // Per-datatype throttled status.
  DataTypeSet throttled_types;

  // Per-datatype backed off status.
  DataTypeSet backed_off_types;

  std::string cache_guid;

  // Data types having pending invalidations.
  DataTypeSet invalidated_data_types;

  // Time of next retry if sync scheduler is throttled or in backoff.
  base::Time retry_time;

  // The location of the local sync backend db file if local sync is enabled.
  std::string local_sync_folder;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_
