// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"

namespace apdu {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ApduCommand::CreateFromMessage(base::make_span(data, size));
  ApduResponse::CreateFromMessage(base::make_span(data, size));
  return 0;
}

}  // namespace apdu
