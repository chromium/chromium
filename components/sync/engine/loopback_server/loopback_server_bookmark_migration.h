// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_BOOKMARK_MIGRATION_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_BOOKMARK_MIGRATION_H_

namespace sync_pb {
class LoopbackServerProto;
}  // namespace sync_pb

namespace syncer {

// Migrates legacy bookmark entities and tombstones in `proto` to the modern
// schema (populating client_tag_hash, converting IDs to deterministic format,
// normalizing specifics, and remapping hierarchy).
void MigrateLoopbackServerLegacyBookmarks(sync_pb::LoopbackServerProto* proto);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_BOOKMARK_MIGRATION_H_
