// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_
#define COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// A light-weight container for sync entity data which represents either
// local data created on the local model side or remote data created on
// ModelTypeWorker.
// EntityData is supposed to be wrapped and passed by reference.
struct EntityData {
 public:
  EntityData();
  EntityData(EntityData&&);
  ~EntityData();

  EntityData& operator=(EntityData&&);

  // Typically this is a server assigned sync ID, although for a local change
  // that represents a new entity this field might be either empty or contain
  // a temporary client sync ID.
  std::string id;

  // A hash based on the client tag and model type.
  // Used for various map lookups. Should always be available for all data types
  // except bookmarks. Sent to the server as
  // SyncEntity::client_defined_unique_tag.
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
  // can't have both a client_defined_unique_tag and a
  // server_defined_unique_tag. Sent to the server as
  // SyncEntity::server_defined_unique_tag.
  std::string server_defined_unique_tag;

  // Entity name, used mostly for Debug purposes.
  std::string name;

  // Model type specific sync data.
  sync_pb::EntitySpecifics specifics;

  // Entity creation and modification timestamps.
  base::Time creation_time;
  base::Time modification_time;

  // Sync ID of the parent entity. This is supposed to be set only for
  // hierarchical datatypes (e.g. Bookmarks).
  std::string parent_id;

  // Indicate whether this is a folder or not. Relevant only for bookmarks.
  bool is_folder = false;

  // Unique position of an entity among its siblings. This is supposed to be
  // set only for datatypes that support positioning (e.g. Bookmarks).
  sync_pb::UniquePosition unique_position;

  // True if EntityData represents deleted entity; otherwise false.
  // Note that EntityData would be considered to represent a deletion if its
  // specifics hasn't been set.
  bool is_deleted() const { return specifics.ByteSize() == 0; }

  // Dumps all info into a DictionaryValue and returns it.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue();

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  DISALLOW_ASSIGN(EntityData);
};

// gMock printer helper.
void PrintTo(const EntityData& entity_data, std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_
