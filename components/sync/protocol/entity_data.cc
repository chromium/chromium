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
  base::Value::Dict dict;
  dict.Set("SPECIFICS", EntitySpecificsToValue(specifics));
  dict.Set("ID", id);
  dict.Set("CLIENT_TAG_HASH", client_tag_hash.value());
  dict.Set("ORIGINATOR_CACHE_GUID", originator_cache_guid);
  dict.Set("ORIGINATOR_CLIENT_ITEM_ID", originator_client_item_id);
  dict.Set("SERVER_DEFINED_UNIQUE_TAG", server_defined_unique_tag);
  // The string "NON_UNIQUE_NAME" is used in sync-internals to identify the node
  // title.
  dict.Set("NON_UNIQUE_NAME", name);
  dict.Set("NAME", name);
  // The string "PARENT_ID" is used in sync-internals to build the node tree.
  dict.Set("PARENT_ID", legacy_parent_id);
  dict.Set("CTIME", GetTimeDebugString(creation_time));
  dict.Set("MTIME", GetTimeDebugString(modification_time));
  return dict;
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
      << entity_data.originator_client_item_id
      << "', server_defined_unique_tag: '"
      << entity_data.server_defined_unique_tag << "', specifics: " << specifics
      << "}";
}

}  // namespace syncer
