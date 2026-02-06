// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/anr_build_id_provider.h"

#include <string>

#include "base/debug/elf_reader.h"
#include "base/logging.h"

extern char __executable_start;

namespace crash_reporter {
std::string GetElfBuildId() {
  base::debug::ElfBuildIdBuffer build_id;
  size_t size =
      base::debug::ReadElfBuildId(&__executable_start, false, build_id);
  CHECK(size) << "Failed to read BuildId";

  // Official builds use SHA1 (40 chars), but debug builds use whatever is
  // default. For non-lld linkers, this can be sha256, which triggers an
  // exception in AnrCollector that enforces it stays under 128 bytes.
  return std::string(build_id).substr(0, 40);
}

}  // namespace crash_reporter
