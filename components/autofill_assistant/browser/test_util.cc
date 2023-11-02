// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/test_util.h"

namespace autofill_assistant {

bool operator==(const google::protobuf::MessageLite& proto_a,
                const google::protobuf::MessageLite& proto_b) {
  std::string serialized_proto_a;
  proto_a.SerializeToString(&serialized_proto_a);

  std::string serialized_proto_b;
  proto_b.SerializeToString(&serialized_proto_b);

  return serialized_proto_a == serialized_proto_b;
}

bool operator==(const autofill_assistant::ScriptParameterProto& proto,
                const std::pair<std::string, std::string>& pair) {
  return proto.name() == pair.first && proto.value() == pair.second;
}

}  // namespace autofill_assistant
