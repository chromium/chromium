// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace syncer {

// Status encapsulates detailed state about the internals of the SyncManager.
//
// This struct is closely tied to the AllStatus object which uses instances of
// it to track and report on the sync engine's internal state, and the functions
// in sync_ui_util.cc which convert the contents of this struct into a
// DictionaryValue used to populate the about:sync summary tab.
struct SyncStatus {
  SyncStatus();
  SyncStatus(const SyncStatus& other);
  ~SyncStatus();

  // TODO(akalin): Replace this with a NotificationsDisabledReason
  // variable.
  bool notifications_enabled;  // True only if subscribed for notifications.

  // Notifications counters updated by the actions in synapi.
  int notifications_received;

  SyncProtocolError sync_protocol_error;

  // Number of encryption conflicts counted during most recent sync cycle.
  int encryption_conflicts;

  // Number of hierarchy conflicts counted during most recent sync cycle.
  int hierarchy_conflicts;

  // Number of items the server refused to commit due to conflict during most
  // recent sync cycle.
  int server_conflicts;

  // Number of items successfully committed during most recent sync cycle.
  int committed_count;

  // Whether a sync cycle is going on right now.
  bool syncing;

  // Total updates received by the syncer since browser start.
  int updates_received;
  // Total updates received that are echoes of our own changes.
  int reflected_updates_received;
  // Of updates_received, how many were tombstones.
  int tombstone_updates_received;

  // Total successful commits.
  int num_commits_total;

  // Total number of overwrites due to conflict resolver since browser start.
  int num_local_overwrites_total;
  int num_server_overwrites_total;

  // Encryption related.
  ModelTypeSet encrypted_types;
  bool cryptographer_can_encrypt;
  bool crypto_has_pending_keys;
  bool has_keystore_key;
  base::Time keystore_migration_time;
  PassphraseType passphrase_type;

  // Per-datatype throttled status.
  ModelTypeSet throttled_types;

  // Per-datatype backed off status.
  ModelTypeSet backed_off_types;

  // The unique identifer for the sync store.
  std::string sync_id;

  // The unique identifier for the invalidation client.
  std::string invalidator_client_id;

  // Counters grouped by model type
  std::vector<int> num_entries_by_type;
  std::vector<int> num_to_delete_entries_by_type;

  // Time of next retry if sync scheduler is throttled or in backoff.
  base::Time retry_time;

  // The location of the local sync backend db file if local sync is enabled.
  std::string local_sync_folder;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_STATUS_H_
