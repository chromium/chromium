// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/entity_data.h"

#include <ostream>
#include <utility>

#include "base/json/json_writer.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

EntityData::EntityData() = default;

EntityData::EntityData(EntityData&& other) = default;

EntityData::~EntityData() = default;

EntityData& EntityData::operator=(EntityData&& other) = default;

base::Value::Dict EntityData::ToDictionaryValue() const {
  // This is used when debugging at sync-internals page. The code in
  // sync_node_browser.js is expecting certain fields names. e.g. CTIME, MTIME,
  // and IS_DIR.
  return base::Value::Dict()
      .Set("SPECIFICS", EntitySpecificsToValue(specifics))
      .Set("ID", id)
      .Set("CLIENT_TAG_HASH", client_tag_hash.value())
      .Set("ORIGINATOR_CACHE_GUID", originator_cache_guid)
      .Set("ORIGINATOR_CLIENT_ITEM_ID", originator_client_item_id)
      .Set("SERVER_DEFINED_UNIQUE_TAG", server_defined_unique_tag)
      // The string "NON_UNIQUE_NAME" is used in sync-internals to identify the
      // node title.
      .Set("NON_UNIQUE_NAME", name)
      .Set("NAME", name)
      // The string "PARENT_ID" is used in sync-internals to build the node
      // tree.
      .Set("PARENT_ID", legacy_parent_id)
      .Set("CTIME", GetTimeDebugString(creation_time))
      .Set("MTIME", GetTimeDebugString(modification_time))
      .Set("RECIPIENT_PUBLIC_KEY",
           CrossUserSharingPublicKeyToValue(recipient_public_key))
      .Set("COLLABORATION_ID", collaboration_id);
}

size_t EntityData::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(id);
  memory_usage += EstimateMemoryUsage(client_tag_hash);
  memory_usage += EstimateMemoryUsage(originator_cache_guid);
  memory_usage += EstimateMemoryUsage(originator_client_item_id);
  memory_usage += EstimateMemoryUsage(server_defined_unique_tag);
  memory_usage += EstimateMemoryUsage(name);
  memory_usage += EstimateMemoryUsage(specifics);
  memory_usage += EstimateMemoryUsage(legacy_parent_id);
  memory_usage += EstimateMemoryUsage(recipient_public_key);
  memory_usage += EstimateMemoryUsage(collaboration_id);
  if (deletion_origin.has_value()) {
    memory_usage += EstimateMemoryUsage(*deletion_origin);
  }
  return memory_usage;
}

void PrintTo(const EntityData& entity_data, std::ostream* os) {
  std::string specifics;
  base::JSONWriter::WriteWithOptions(
      syncer::EntitySpecificsToValue(entity_data.specifics),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &specifics);
  *os << "{ id: '" << entity_data.id << "', client_tag_hash: '"
      << entity_data.client_tag_hash << "', originator_cache_guid: '"
      << entity_data.originator_cache_guid << "', originator_client_item_id: '"
      << entity_data.originator_client_item_id << "', collaboration_id: '"
      << entity_data.collaboration_id << "', server_defined_unique_tag: '"
      << entity_data.server_defined_unique_tag << "', specifics: " << specifics
      << "}";
}

}  // namespace syncer
