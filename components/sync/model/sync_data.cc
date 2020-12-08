// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_data.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"

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

SyncData::SyncData() : is_local_(false), is_valid_(false) {}

SyncData::SyncData(bool is_local, sync_pb::SyncEntity* entity)
    : immutable_entity_(entity), is_local_(is_local), is_valid_(true) {}

SyncData::SyncData(const SyncData& other) = default;

SyncData::~SyncData() {}

// Static.
SyncData SyncData::CreateLocalDelete(const std::string& client_tag_unhashed,
                                     ModelType datatype) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(datatype, &specifics);
  return CreateLocalData(client_tag_unhashed, std::string(), specifics);
}

// Static.
SyncData SyncData::CreateLocalData(const std::string& client_tag_unhashed,
                                   const std::string& non_unique_title,
                                   const sync_pb::EntitySpecifics& specifics) {
  const ModelType model_type = GetModelTypeFromSpecifics(specifics);
  DCHECK(IsRealDataType(model_type));

  DCHECK(!client_tag_unhashed.empty());
  const ClientTagHash client_tag_hash =
      ClientTagHash::FromUnhashed(model_type, client_tag_unhashed);

  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(client_tag_hash.value());
  entity.set_non_unique_name(non_unique_title);
  entity.mutable_specifics()->CopyFrom(specifics);

  return SyncData(/*is_local=*/true, &entity);
}

// Static.
SyncData SyncData::CreateRemoteData(sync_pb::EntitySpecifics specifics,
                                    const ClientTagHash& client_tag_hash) {
  sync_pb::SyncEntity entity;
  *entity.mutable_specifics() = std::move(specifics);
  entity.set_client_defined_unique_tag(client_tag_hash.value());
  return SyncData(/*is_local=*/false, &entity);
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

ClientTagHash SyncData::GetClientTagHash() const {
  return ClientTagHash::FromHashed(
      immutable_entity_.Get().client_defined_unique_tag());
}

const std::string& SyncData::GetTitle() const {
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
  std::string is_local_string = IsLocal() ? "true" : "false";

  return "{ isLocal: " + is_local_string + ", type: " + type +
         ", tagHash: " + GetClientTagHash().value() + ", title: " + GetTitle() +
         ", specifics: " + specifics + "}";
}

void PrintTo(const SyncData& sync_data, std::ostream* os) {
  *os << sync_data.ToString();
}

}  // namespace syncer
