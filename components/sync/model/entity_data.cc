// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/entity_data.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

namespace {

std::string UniquePositionToString(
    const sync_pb::UniquePosition& unique_position) {
  return UniquePosition::FromProto(unique_position).ToDebugString();
}

}  // namespace

EntityData::EntityData() = default;

EntityData::EntityData(EntityData&& other)
    : id(std::move(other.id)),
      client_tag_hash(std::move(other.client_tag_hash)),
      originator_cache_guid(std::move(other.originator_cache_guid)),
      originator_client_item_id(std::move(other.originator_client_item_id)),
      server_defined_unique_tag(std::move(other.server_defined_unique_tag)),
      name(std::move(other.name)),
      creation_time(other.creation_time),
      modification_time(other.modification_time),
      parent_id(std::move(other.parent_id)),
      is_folder(other.is_folder) {
  specifics.Swap(&other.specifics);
  unique_position.Swap(&other.unique_position);
}

EntityData::~EntityData() = default;

EntityData& EntityData::operator=(EntityData&& other) {
  id = std::move(other.id);
  client_tag_hash = std::move(other.client_tag_hash);
  originator_cache_guid = std::move(other.originator_cache_guid);
  originator_client_item_id = std::move(other.originator_client_item_id);
  server_defined_unique_tag = std::move(other.server_defined_unique_tag);
  name = std::move(other.name);
  creation_time = other.creation_time;
  modification_time = other.modification_time;
  parent_id = other.parent_id;
  is_folder = other.is_folder;
  specifics.Swap(&other.specifics);
  unique_position.Swap(&other.unique_position);
  return *this;
}

#define ADD_TO_DICT(dict, value) \
  dict->SetString(base::ToUpperASCII(#value), value);

#define ADD_TO_DICT_WITH_TRANSFORM(dict, value, transform) \
  dict->SetString(base::ToUpperASCII(#value), transform(value));

std::unique_ptr<base::DictionaryValue> EntityData::ToDictionaryValue() {
  // This is used when debugging at sync-internals page. The code in
  // sync_node_browser.js is expecting certain fields names. e.g. CTIME, MTIME,
  // and IS_DIR.
  base::Time ctime = creation_time;
  base::Time mtime = modification_time;
  std::unique_ptr<base::DictionaryValue> dict =
      std::make_unique<base::DictionaryValue>();
  dict->Set("SPECIFICS", EntitySpecificsToValue(specifics));
  ADD_TO_DICT(dict, id);
  ADD_TO_DICT(dict, client_tag_hash.value());
  ADD_TO_DICT(dict, originator_cache_guid);
  ADD_TO_DICT(dict, originator_client_item_id);
  ADD_TO_DICT(dict, server_defined_unique_tag);
  // The string "NON_UNIQUE_NAME" is used in sync-internals to identify the node
  // title.
  dict->SetString("NON_UNIQUE_NAME", name);
  ADD_TO_DICT(dict, name);
  ADD_TO_DICT(dict, parent_id);
  ADD_TO_DICT_WITH_TRANSFORM(dict, ctime, GetTimeDebugString);
  ADD_TO_DICT_WITH_TRANSFORM(dict, mtime, GetTimeDebugString);
  ADD_TO_DICT_WITH_TRANSFORM(dict, unique_position, UniquePositionToString);
  dict->SetBoolean("IS_DIR", is_folder);
  return dict;
}

#undef ADD_TO_DICT
#undef ADD_TO_DICT_WITH_TRANSFORM

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
  memory_usage += EstimateMemoryUsage(parent_id);
  memory_usage += EstimateMemoryUsage(unique_position);
  return memory_usage;
}

void PrintTo(const EntityData& entity_data, std::ostream* os) {
  std::string specifics;
  base::JSONWriter::WriteWithOptions(
      *syncer::EntitySpecificsToValue(entity_data.specifics),
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
