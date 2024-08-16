// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_ENTITY_DATA_H_
#define COMPONENTS_SYNC_PROTOCOL_ENTITY_DATA_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/deletion_origin.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

// A light-weight container for sync entity data which represents either
// local data created on the local model side or remote data created on
// DataTypeWorker.
// EntityData is supposed to be wrapped and passed by reference.
struct EntityData {
 public:
  EntityData();

  EntityData& operator=(const EntityData&) = delete;

  EntityData(EntityData&&);
  EntityData& operator=(EntityData&&);

  ~EntityData();

  // Typically this is a server assigned sync ID, although for a local change
  // that represents a new entity this field might be either empty or contain
  // a temporary client sync ID.
  std::string id;

  // A hash based on the client tag and data type.
  // Used for various map lookups. Should always be available for all data types
  // except bookmarks (for bookmarks it depends on the version of the client
  // that originally created the bookmark).
  ClientTagHash client_tag_hash;

  // A GUID that identifies the the sync client who initially committed this
  // entity. It's relevant only for bookmarks. See the definition in sync.proto
  // for more details.
  std::string originator_cache_guid;

  // The local item id of this entry from the client that initially committed
  // this entity. It's relevant only for bookmarks. See the definition in
  // sync.proto for more details.
  std::string originator_client_item_id;

  // This tag identifies this item as being a uniquely instanced item.  An item
  // can't have both a client_tag_hash and a
  // server_defined_unique_tag. Sent to the server as
  // SyncEntity::server_defined_unique_tag.
  std::string server_defined_unique_tag;

  // Entity name, used mostly for Debug purposes.
  std::string name;

  // Data type specific sync data.
  sync_pb::EntitySpecifics specifics;

  // Entity creation and modification timestamps.
  base::Time creation_time;
  base::Time modification_time;

  // Server-provided sync ID of the parent entity, used for legacy bookmarks
  // only. Unused for modern data created or reuploaded by M94 or above, which
  // relies exclusively on the parent's GUID in BookmarkSpecifics.
  // WARNING: Avoid references to this field outside
  // components/sync_bookmarks/parent_guid_preprocessing.cc.
  std::string legacy_parent_id;

  // Recipient's Public Key used for cross-user sharing data types. Used for
  // only outgoing password sharing invitations (created locally).
  sync_pb::CrossUserSharingPublicKey recipient_public_key;

  // Indicate whether bookmark's |unique_position| was missing in the original
  // specifics during GetUpdates. If the |unique_position| in specifics was
  // evaluated by AdaptUniquePositionForBookmark(), this field will be set to
  // true. Relevant only for bookmarks.
  bool is_bookmark_unique_position_in_specifics_preprocessed = false;

  // Collaboration with which the current entity is associated. Empty for
  // non-shared types.
  std::string collaboration_id;

  // True if EntityData represents deleted entity; otherwise false.
  // Note that EntityData would be considered to represent a deletion if its
  // specifics hasn't been set.
  bool is_deleted() const { return specifics.ByteSize() == 0; }

  // Optionally populated for outgoing deletions. See corresponding field in
  // SyncEntity for details.
  std::optional<sync_pb::DeletionOrigin> deletion_origin;

  // Dumps all info into a base::Value::Dict and returns it.
  base::Value::Dict ToDictionaryValue() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;
};

// gMock printer helper.
void PrintTo(const EntityData& entity_data, std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_ENTITY_DATA_H_
