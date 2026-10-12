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
  std::string build_id =
      base::debug::ReadElfBuildId(&__executable_start, false);
  CHECK(!build_id.empty()) << "Failed to read BuildId";

  // Official builds use SHA1 (40 chars), but debug builds use whatever is
  // default. For non-lld linkers, this can be sha256, which triggers an
  // exception in AnrCollector that enforces it stays under 128 bytes.
  if (build_id.size() > 40) {
    build_id.resize(40);
  }
  return build_id;
}

}  // namespace crash_reporter
