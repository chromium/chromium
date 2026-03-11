// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace apdu {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  ApduCommand::CreateFromMessage(data);
  ApduResponse::CreateFromMessage(data);
  return 0;
}

}  // namespace apdu
