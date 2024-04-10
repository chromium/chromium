// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
