// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROTO_EXTRAS_PROTOBUF_FULL_SUPPORT_H_
#define COMPONENTS_PROTO_EXTRAS_PROTOBUF_FULL_SUPPORT_H_

#include "base/component_export.h"
#include "base/values.h"
#include "third_party/protobuf/src/google/protobuf/message.h"

namespace google::protobuf {
class UnknownFieldSet;
}  // namespace google::protobuf

namespace proto_extras {

base::DictValue ToValue(
    const google::protobuf::UnknownFieldSet& unknown_fields);

bool MessageDifferencerEquals(const google::protobuf::Message& lhs,
                              const google::protobuf::Message& rhs);

template <typename MessageType>
  requires std::is_base_of_v<google::protobuf::Message, MessageType>
void SerializeUnknownFields(const MessageType& message, base::DictValue& dict) {
  if (message.unknown_fields().empty()) {
    return;
  }
  dict.Set("unknown_fields", ToValue(message.unknown_fields()));
}

}  // namespace proto_extras

#endif  // COMPONENTS_PROTO_EXTRAS_PROTOBUF_FULL_SUPPORT_H_
