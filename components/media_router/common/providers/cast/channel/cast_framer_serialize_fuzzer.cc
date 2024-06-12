// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>

#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {
namespace fuzz {

DEFINE_PROTO_FUZZER(const openscreen::cast::proto::CastMessage& input) {
  std::string native_input;
  MessageFramer::Serialize(input, &native_input);
  if (::getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;
}

}  // namespace fuzz
}  // namespace cast_channel
