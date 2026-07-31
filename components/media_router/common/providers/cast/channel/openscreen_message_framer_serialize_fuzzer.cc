// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>

#include "base/check.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/openscreen/src/cast/common/channel/message_framer.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel_fuzzable.pb.h"

namespace cast_channel {
namespace fuzz {

DEFINE_PROTO_FUZZER(
    const fuzzable::openscreen::cast::proto::CastMessage& fuzzable_input) {
  std::string serialized;
  CHECK(fuzzable_input.SerializeToString(&serialized));
  openscreen::cast::proto::CastMessage input;
  CHECK(input.ParseFromString(serialized));

  openscreen::ErrorOr<std::vector<uint8_t>> result =
      openscreen::cast::message_serialization::Serialize(input);
}

}  // namespace fuzz
}  // namespace cast_channel
