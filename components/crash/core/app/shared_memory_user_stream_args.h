// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_ARGS_H_
#define COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_ARGS_H_

// This file provides utilities for marshaling
// `base::ReadOnlySharedMemoryRegion` handles from the browser process to the
// Crashpad process via command-line arguments (`argc`/`argv`).
//
// Currently supported on platforms that pass Crashpad handler initialization
// handles via command line arguments (e.g., Windows, Linux, ChromeOS).

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) ||
                  BUILDFLAG(IS_CHROMEOS),
              "Unsupported platform.");

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "third_party/crashpad/crashpad/util/file/file_io.h"

namespace crash_reporter::internal {

inline constexpr std::string_view kUserStreamRegionSwitch =
    "--user-stream-region=";

// For each region in `regions`:
// - Appends a `--user-stream-region=<handle>,<size>` argument to
//   `command_line_arguments`.
// - Inserts the underlying platform handle (a `HANDLE` on Windows or a file
//   descriptor on POSIX) into `preserve_handles` so it can be inherited by
//   the spawned Crashpad process.
//
// Assumes the caller retains the original `regions` throughout the
// Crashpad-process spawn logic so the platform handles are not prematurely
// closed.
void AppendSharedMemoryUserStreamArgs(
    const std::vector<base::ReadOnlySharedMemoryRegion>& regions,
    std::vector<std::string>* command_line_arguments,
    std::set<crashpad::FileHandle>* preserve_handles);

// Removes `--user-stream-region=<handle>,<size>` arguments from `argc` and
// `argv`, and returns the shared memory regions referenced by the
// [handle, size] pairs in the switch values.
std::vector<base::ReadOnlySharedMemoryRegion> ExtractSharedMemoryUserStreamArgs(
    int* argc,
    char* argv[]);

}  // namespace crash_reporter::internal

#endif  // COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_ARGS_H_
