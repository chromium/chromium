// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/unknown_field_util.h"

#include "components/sync/test/unknown_fields.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace syncer::test {

void AddUnknownFieldToProto(::google::protobuf::MessageLite& proto,
                            std::string unknown_field_value) {
  sync_pb::test::UnknownFields unknown_fields;
  unknown_fields.set_unknown_field(std::move(unknown_field_value));

  proto.MergeFromString(unknown_fields.SerializeAsString());
}

void AddUnknownEnumFieldToProto(::google::protobuf::MessageLite& proto,
                                int field_number,
                                int value) {
  std::string serialized_unknown_field;
  {
    google::protobuf::io::StringOutputStream string_output_stream(
        &serialized_unknown_field);
    google::protobuf::io::CodedOutputStream coded_output_stream(
        &string_output_stream);

    coded_output_stream.WriteTag(
        google::protobuf::internal::WireFormatLite::MakeTag(
            field_number,
            google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT));
    coded_output_stream.WriteVarint32(value);
  }
  proto.MergeFromString(serialized_unknown_field);
}

std::string GetUnknownFieldValueFromProto(
    const ::google::protobuf::MessageLite& proto) {
  sync_pb::test::UnknownFields unknown_fields;
  unknown_fields.ParseFromString(proto.SerializeAsString());

  return unknown_fields.unknown_field();
}

int GetUnknownEnumFieldValueFromProto(
    const ::google::protobuf::MessageLite& proto,
    int field_number) {
  std::string serialized_proto = proto.SerializeAsString();
  google::protobuf::io::ArrayInputStream input_stream(serialized_proto.data(),
                                                      serialized_proto.size());
  google::protobuf::io::CodedInputStream coded_input_stream(&input_stream);

  uint32_t tag = 0;
  int value = -1;
  while ((tag = coded_input_stream.ReadTag()) != 0) {
    if (google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag) !=
        field_number) {
      google::protobuf::internal::WireFormatLite::SkipField(&coded_input_stream,
                                                            tag);
      continue;
    }

    if (google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
        google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
      uint32_t temp = 0;
      if (coded_input_stream.ReadVarint32(&temp)) {
        value = static_cast<int>(temp);
      }
    } else {
      google::protobuf::internal::WireFormatLite::SkipField(&coded_input_stream,
                                                            tag);
    }
  }

  return value;
}

}  // namespace syncer::test
