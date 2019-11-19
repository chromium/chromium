// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_status.h"

namespace syncer {

SyncStatus::SyncStatus()
    : notifications_enabled(false),
      notifications_received(0),
      encryption_conflicts(0),
      hierarchy_conflicts(0),
      server_conflicts(0),
      committed_count(0),
      syncing(false),
      updates_received(0),
      reflected_updates_received(0),
      tombstone_updates_received(0),
      num_commits_total(0),
      num_local_overwrites_total(0),
      num_server_overwrites_total(0),
      cryptographer_can_encrypt(false),
      crypto_has_pending_keys(false),
      has_keystore_key(false),
      passphrase_type(PassphraseType::kImplicitPassphrase),
      num_entries_by_type(ModelType::NUM_ENTRIES, 0),
      num_to_delete_entries_by_type(ModelType::NUM_ENTRIES, 0) {}

SyncStatus::SyncStatus(const SyncStatus& other) = default;

SyncStatus::~SyncStatus() {}

}  // namespace syncer
