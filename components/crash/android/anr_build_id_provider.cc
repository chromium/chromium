// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/anr_build_id_provider.h"

#include <string>

#include "base/debug/elf_reader.h"

extern char __executable_start;

namespace crash_reporter {
std::string GetElfBuildId() {
  base::debug::ElfBuildIdBuffer build_id;
  base::debug::ReadElfBuildId(&__executable_start, false, build_id);
  return std::string(build_id);
}

}  // namespace crash_reporter
