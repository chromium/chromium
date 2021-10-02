// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template.h"

#include <memory>

#include "base/check.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"

namespace desks_storage {

namespace {

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_t));
}

}  // namespace

std::unique_ptr<DeskTemplate> DeskTemplate::FromProto(
    const sync_pb::WorkspaceDeskSpecifics& pb_entry) {
  const std::string uuid(pb_entry.uuid());
  if (uuid.empty())
    return nullptr;

  const base::Time created_time = ProtoTimeToTime(pb_entry.created_time_usec());

  // Protobuf parsing enforces utf8 encoding for all strings.
  return std::make_unique<DeskTemplate>(uuid, pb_entry.name(), created_time);
}

std::unique_ptr<DeskTemplate> DeskTemplate::FromRequiredFields(
    const std::string& uuid) {
  return !uuid.empty() ? std::make_unique<DeskTemplate>(uuid, "", base::Time())
                       : nullptr;
}

DeskTemplate::DeskTemplate(const std::string& uuid,
                           const std::string& name,
                           base::Time created_time)
    : uuid_(uuid), name_(name), created_time_(created_time) {
  DCHECK(!uuid_.empty());
  DCHECK(base::IsStringUTF8(uuid_));
  DCHECK(base::IsStringUTF8(name_));
}

DeskTemplate::~DeskTemplate() = default;

sync_pb::WorkspaceDeskSpecifics DeskTemplate::AsSyncProto() const {
  sync_pb::WorkspaceDeskSpecifics pb_entry;

  pb_entry.set_uuid(uuid());
  pb_entry.set_name(name());
  pb_entry.set_created_time_usec(TimeToProtoTime(created_time()));

  // TODO(yzd) copy other data fields
  return pb_entry;
}

}  // namespace desks_storage