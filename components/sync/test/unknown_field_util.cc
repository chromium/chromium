// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/unknown_field_util.h"

#include "components/sync/test/unknown_fields.pb.h"

namespace syncer::test {

void AddUnknownFieldToProto(::google::protobuf::MessageLite& proto,
                            std::string unknown_field_value) {
  sync_pb::test::UnknownFields unknown_fields;
  unknown_fields.set_unknown_field(std::move(unknown_field_value));

  proto.MergeFromString(unknown_fields.SerializeAsString());
}

std::string GetUnknownFieldValueFromProto(
    const ::google::protobuf::MessageLite& proto) {
  sync_pb::test::UnknownFields unknown_fields;
  unknown_fields.ParseFromString(proto.SerializeAsString());

  return unknown_fields.unknown_field();
}

}  // namespace syncer::test
