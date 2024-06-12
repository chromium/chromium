// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/openscreen/src/cast/common/channel/message_framer.h"

namespace cast_channel {
namespace fuzz {

DEFINE_PROTO_FUZZER(const openscreen::cast::proto::CastMessage& input) {
  openscreen::ErrorOr<std::vector<uint8_t>> result =
      openscreen::cast::message_serialization::Serialize(input);
}

}  // namespace fuzz
}  // namespace cast_channel
