// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_data.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/base_node.h"

namespace syncer {

void SyncData::ImmutableSyncEntityTraits::InitializeWrapper(Wrapper* wrapper) {
  *wrapper = new sync_pb::SyncEntity();
}

void SyncData::ImmutableSyncEntityTraits::DestroyWrapper(Wrapper* wrapper) {
  delete *wrapper;
}

const sync_pb::SyncEntity& SyncData::ImmutableSyncEntityTraits::Unwrap(
    const Wrapper& wrapper) {
  return *wrapper;
}

sync_pb::SyncEntity* SyncData::ImmutableSyncEntityTraits::UnwrapMutable(
    Wrapper* wrapper) {
  return *wrapper;
}

void SyncData::ImmutableSyncEntityTraits::Swap(sync_pb::SyncEntity* t1,
                                               sync_pb::SyncEntity* t2) {
  t1->Swap(t2);
}

SyncData::SyncData() : id_(kInvalidId), is_local_(false), is_valid_(false) {}

SyncData::SyncData(bool is_local, int64_t id, sync_pb::SyncEntity* entity)
    : id_(id),
      immutable_entity_(entity),
      is_local_(is_local),
      is_valid_(true) {}

SyncData::SyncData(const SyncData& other) = default;

SyncData::~SyncData() {}

// Static.
SyncData SyncData::CreateLocalDelete(const std::string& sync_tag,
                                     ModelType datatype) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(datatype, &specifics);
  return CreateLocalData(sync_tag, std::string(), specifics);
}

// Static.
SyncData SyncData::CreateLocalData(const std::string& sync_tag,
                                   const std::string& non_unique_title,
                                   const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(sync_tag);
  entity.set_non_unique_name(non_unique_title);
  entity.mutable_specifics()->CopyFrom(specifics);
  return SyncData(/*is_local=*/true, kInvalidId, &entity);
}

// Static.
SyncData SyncData::CreateRemoteData(int64_t id,
                                    sync_pb::EntitySpecifics specifics,
                                    std::string client_tag_hash) {
  sync_pb::SyncEntity entity;
  *entity.mutable_specifics() = std::move(specifics);
  entity.set_client_defined_unique_tag(std::move(client_tag_hash));
  return SyncData(/*is_local=*/false, id, &entity);
}

bool SyncData::IsValid() const {
  return is_valid_;
}

const sync_pb::EntitySpecifics& SyncData::GetSpecifics() const {
  return immutable_entity_.Get().specifics();
}

ModelType SyncData::GetDataType() const {
  return GetModelTypeFromSpecifics(GetSpecifics());
}

const std::string& SyncData::GetTitle() const {
  // TODO(zea): set this for data coming from the syncer too.
  DCHECK(immutable_entity_.Get().has_non_unique_name());
  return immutable_entity_.Get().non_unique_name();
}

bool SyncData::IsLocal() const {
  return is_local_;
}

std::string SyncData::ToString() const {
  if (!IsValid())
    return "<Invalid SyncData>";

  std::string type = ModelTypeToString(GetDataType());
  std::string specifics;
  base::JSONWriter::WriteWithOptions(*EntitySpecificsToValue(GetSpecifics()),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &specifics);

  if (IsLocal()) {
    SyncDataLocal sync_data_local(*this);
    return "{ isLocal: true, type: " + type + ", tag: " +
           sync_data_local.GetTag() + ", title: " + GetTitle() +
           ", specifics: " + specifics + "}";
  }

  SyncDataRemote sync_data_remote(*this);
  std::string id = base::NumberToString(sync_data_remote.id_);
  return "{ isLocal: false, type: " + type + ", specifics: " + specifics +
         ", id: " + id + "}";
}

void PrintTo(const SyncData& sync_data, std::ostream* os) {
  *os << sync_data.ToString();
}

SyncDataLocal::SyncDataLocal(const SyncData& sync_data) : SyncData(sync_data) {
  DCHECK(sync_data.IsLocal());
}

SyncDataLocal::~SyncDataLocal() {}

const std::string& SyncDataLocal::GetTag() const {
  return immutable_entity_.Get().client_defined_unique_tag();
}

SyncDataRemote::SyncDataRemote(const SyncData& sync_data)
    : SyncData(sync_data) {
  DCHECK(!sync_data.IsLocal());
}

SyncDataRemote::~SyncDataRemote() {}

int64_t SyncDataRemote::GetId() const {
  DCHECK(!IsLocal());
  DCHECK_NE(id_, kInvalidId);
  return id_;
}

ClientTagHash SyncDataRemote::GetClientTagHash() const {
  // It seems that client_defined_unique_tag has a bit of an overloaded use,
  // holding onto the un-hashed tag while local, and then the hashed value when
  // communicating with the server. This usage is copying the latter of these
  // cases, where this is the hashed tag value. The original tag is not sent to
  // the server so we wouldn't be able to set this value anyways. The only way
  // to recreate an un-hashed tag is for the service to do so with a specifics.
  DCHECK(!immutable_entity_.Get().client_defined_unique_tag().empty());
  return ClientTagHash::FromHashed(
      immutable_entity_.Get().client_defined_unique_tag());
}

}  // namespace syncer
