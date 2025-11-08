// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proto_extras/protobuf_full_support.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/proto_extras/proto_extras_lib.h"
#include "third_party/protobuf/src/google/protobuf/unknown_field_set.h"
#include "third_party/protobuf/src/google/protobuf/util/message_differencer.h"

namespace proto_extras {

base::DictValue ToValue(
    const google::protobuf::UnknownFieldSet& unknown_fields) {
  base::DictValue dict;
  for (int i = 0; i < unknown_fields.field_count(); ++i) {
    const google::protobuf::UnknownField& field = unknown_fields.field(i);
    std::string field_name = base::NumberToString(field.number());
    switch (field.type()) {
      case google::protobuf::UnknownField::TYPE_VARINT:
        dict.Set(field_name, ToNumericTypeForValue(field.varint()));
        break;
      case google::protobuf::UnknownField::TYPE_FIXED32:
        dict.Set(field_name, ToNumericTypeForValue(field.fixed32()));
        break;
      case google::protobuf::UnknownField::TYPE_FIXED64:
        dict.Set(field_name, ToNumericTypeForValue(field.fixed64()));
        break;
      case google::protobuf::UnknownField::TYPE_LENGTH_DELIMITED:
        dict.Set(field_name, field.length_delimited());
        break;
      case google::protobuf::UnknownField::TYPE_GROUP:
        dict.Set(field_name, ToValue(field.group()));
        break;
    }
  }
  return dict;
}

bool MessageDifferencerEquals(const google::protobuf::Message& lhs,
                              const google::protobuf::Message& rhs) {
  return google::protobuf::util::MessageDifferencer::Equals(lhs, rhs);
}

}  // namespace proto_extras
