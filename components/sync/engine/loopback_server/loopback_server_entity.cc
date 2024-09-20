// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/loopback_server_entity.h"

#include <limits>
#include <string_view>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/engine/loopback_server/persistent_permanent_entity.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;

using syncer::DataType;

namespace {
// The separator used when formatting IDs.
//
// We chose the underscore character because it doesn't conflict with the
// special characters used by base/base64.h's encoding, which is also used in
// the construction of some IDs.
const char kIdSeparator[] = "_";
}  // namespace

namespace syncer {

LoopbackServerEntity::~LoopbackServerEntity() = default;

// static
std::unique_ptr<LoopbackServerEntity>
LoopbackServerEntity::CreateEntityFromProto(
    const sync_pb::LoopbackServerEntity& entity) {
  switch (entity.type()) {
    case sync_pb::LoopbackServerEntity_Type_TOMBSTONE:
      return PersistentTombstoneEntity::CreateFromEntity(entity.entity());
    case sync_pb::LoopbackServerEntity_Type_PERMANENT:
      return std::make_unique<PersistentPermanentEntity>(
          entity.entity().id_string(), entity.entity().version(),
          syncer::GetDataTypeFromSpecifics(entity.entity().specifics()),
          entity.entity().name(), entity.entity().parent_id_string(),
          entity.entity().server_defined_unique_tag(),
          entity.entity().specifics());
    case sync_pb::LoopbackServerEntity_Type_BOOKMARK:
      return PersistentBookmarkEntity::CreateFromEntity(entity.entity());
    case sync_pb::LoopbackServerEntity_Type_UNIQUE:
      return PersistentUniqueClientEntity::CreateFromEntity(entity.entity());
    case sync_pb::LoopbackServerEntity_Type_UNKNOWN:
      NOTREACHED_IN_MIGRATION() << "Unknown type encountered";
  }
  return nullptr;
}

const std::string& LoopbackServerEntity::GetId() const {
  return id_;
}

DataType LoopbackServerEntity::GetDataType() const {
  return data_type_;
}

int64_t LoopbackServerEntity::GetVersion() const {
  return version_;
}

void LoopbackServerEntity::SetVersion(int64_t version) {
  version_ = version;
}

const std::string& LoopbackServerEntity::GetName() const {
  return name_;
}

void LoopbackServerEntity::SetName(const std::string& name) {
  name_ = name;
}

void LoopbackServerEntity::SetSpecifics(
    const sync_pb::EntitySpecifics& updated_specifics) {
  specifics_ = updated_specifics;
}

sync_pb::EntitySpecifics LoopbackServerEntity::GetSpecifics() const {
  return specifics_;
}

bool LoopbackServerEntity::IsDeleted() const {
  return false;
}

bool LoopbackServerEntity::IsFolder() const {
  return false;
}

bool LoopbackServerEntity::IsPermanent() const {
  return false;
}

sync_pb::LoopbackServerEntity_Type
LoopbackServerEntity::GetLoopbackServerEntityType() const {
  NOTREACHED_IN_MIGRATION();
  return sync_pb::LoopbackServerEntity_Type_UNKNOWN;
}

// static
string LoopbackServerEntity::CreateId(const DataType& data_type,
                                      const string& inner_id) {
  int field_number = GetSpecificsFieldNumberFromDataType(data_type);
  return base::StringPrintf("%d%s%s", field_number, kIdSeparator,
                            inner_id.c_str());
}

// static
std::string LoopbackServerEntity::GetTopLevelId(const DataType& data_type) {
  return LoopbackServerEntity::CreateId(
      data_type, syncer::DataTypeToProtocolRootTag(data_type));
}

// static
DataType LoopbackServerEntity::GetDataTypeFromId(const string& id) {
  vector<std::string_view> tokens = base::SplitStringPiece(
      id, kIdSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  int field_number;
  if (tokens.size() != 2 || !base::StringToInt(tokens[0], &field_number)) {
    return syncer::UNSPECIFIED;
  }

  return syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
}

// static
std::string LoopbackServerEntity::GetInnerIdFromId(const std::string& id) {
  vector<std::string_view> tokens = base::SplitStringPiece(
      id, kIdSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() != 2) {
    return std::string();
  }

  return std::string(tokens[1]);
}

LoopbackServerEntity::LoopbackServerEntity(const string& id,
                                           const DataType& data_type,
                                           int64_t version,
                                           const string& name)
    : id_(id), data_type_(data_type), version_(version), name_(name) {}

void LoopbackServerEntity::SerializeBaseProtoFields(
    sync_pb::SyncEntity* sync_entity) const {
  sync_pb::EntitySpecifics* specifics = sync_entity->mutable_specifics();
  specifics->CopyFrom(specifics_);

  // LoopbackServerEntity fields
  sync_entity->set_id_string(id_);
  sync_entity->set_version(version_);
  sync_entity->set_name(name_);

  // Data via accessors
  sync_entity->set_deleted(IsDeleted());
  sync_entity->set_folder(IsFolder());

  if (RequiresParentId()) {
    sync_entity->set_parent_id_string(GetParentId());
  }
}

void LoopbackServerEntity::SerializeAsLoopbackServerEntity(
    sync_pb::LoopbackServerEntity* entity) const {
  entity->set_type(GetLoopbackServerEntityType());
  entity->set_data_type(static_cast<int64_t>(GetDataType()));
  SerializeAsProto(entity->mutable_entity());
}

}  // namespace syncer
