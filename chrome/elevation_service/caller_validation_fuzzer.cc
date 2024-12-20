// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/elevation_service/caller_validation.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/process/process.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::ignore = elevation_service::ValidateData(
      base::Process::Current(), base::span<const uint8_t>(data, size));
  return 0;
}
