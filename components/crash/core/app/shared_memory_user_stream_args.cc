// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_args.h"

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) ||
                  BUILDFLAG(IS_CHROMEOS),
              "Unsupported platform.");

#include <stdint.h>
#include <string.h>

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#else
#include <fcntl.h>

#include "base/files/scoped_file.h"
#endif

namespace crash_reporter::internal {

namespace {

// Parses `handle_and_size` formatted as "<handle>,<size>" and reconstructs a
// valid `base::ReadOnlySharedMemoryRegion` instance from it.
// Returns `std::nullopt` if parsing or validation fails.
std::optional<base::ReadOnlySharedMemoryRegion>
DeserializeSharedMemoryUserStreamArgument(std::string_view handle_and_size) {
  size_t comma_pos = handle_and_size.find(',');
  if (comma_pos == std::string_view::npos) {
    return std::nullopt;
  }

  std::string_view handle_str = handle_and_size.substr(0, comma_pos);
  std::string_view size_str = handle_and_size.substr(comma_pos + 1);

  size_t handle_value = 0;
  if (!base::StringToSizeT(handle_str, &handle_value)) {
    return std::nullopt;
  }

  size_t size = 0;
  if (!base::StringToSizeT(size_str, &size)) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_WIN)
  HANDLE raw_handle = reinterpret_cast<HANDLE>(handle_value);
  DWORD flags;

  // Verify the underlying `HANDLE` is valid and open. `base::win::ScopedHandle`
  // enforces tight lifecycle invariants and will fatally crash on destruction
  // if it attempts to close an invalid/unopened handle.
  // Note: This only protects against unopened/garbage handles. If a valid but
  // incorrectly-typed handle (e.g., an `Event`) is passed, `TakeOrFail` will
  // safely reject it and gracefully close it, taking intended ownership.
  if (!::GetHandleInformation(raw_handle, &flags)) {
    return std::nullopt;
  }

  base::win::ScopedHandle platform_handle(raw_handle);
  auto platform_region = base::subtle::PlatformSharedMemoryRegion::TakeOrFail(
      std::move(platform_handle),
      base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, size,
      base::UnguessableToken::Create());
#else
  if (!base::IsValueInRangeForNumericType<int>(handle_value)) {
    return std::nullopt;
  }
  int fd = static_cast<int>(handle_value);

  // Verify the FD is valid and actually open. `base::ScopedFD` enforces tight
  // lifecycle invariants and will fatally crash on destruction if it attempts
  // to close an invalid/unopened file descriptor.
  // Note: This only protects against unopened/garbage FDs. If a valid but
  // incorrectly-typed FD (e.g., `stdout`) is passed, `TakeOrFail` will safely
  // reject it and gracefully close it, taking intended ownership.
  if (fcntl(fd, F_GETFD) == -1) {
    return std::nullopt;
  }

  base::ScopedFD platform_handle(fd);
  auto platform_region = base::subtle::PlatformSharedMemoryRegion::TakeOrFail(
      base::subtle::ScopedFDPair(std::move(platform_handle), base::ScopedFD()),
      base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, size,
      base::UnguessableToken::Create());
#endif

  if (!platform_region.has_value()) {
    return std::nullopt;
  }

  auto read_only_region = base::ReadOnlySharedMemoryRegion::Deserialize(
      std::move(platform_region.value()));
  if (!read_only_region.IsValid()) {
    return std::nullopt;
  }

  return read_only_region;
}

}  // namespace

void AppendSharedMemoryUserStreamArgs(
    const std::vector<base::ReadOnlySharedMemoryRegion>& regions,
    std::vector<std::string>* command_line_arguments,
    std::set<crashpad::FileHandle>* preserve_handles) {
  for (const auto& region : regions) {
    if (!region.IsValid()) {
      continue;
    }

    crashpad::FileHandle handle;
    size_t handle_value;
#if BUILDFLAG(IS_WIN)
    handle = region.GetPlatformHandle();
    handle_value = reinterpret_cast<size_t>(handle);
#else
    handle = region.GetPlatformHandle().fd;
    handle_value = static_cast<size_t>(handle);
#endif

    command_line_arguments->push_back(base::StrCat(
        {kUserStreamRegionSwitch, base::NumberToString(handle_value), ",",
         base::NumberToString(region.GetSize())}));
    preserve_handles->insert(handle);
  }
}

std::vector<base::ReadOnlySharedMemoryRegion> ExtractSharedMemoryUserStreamArgs(
    int* argc,
    char* argv[]) {
  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  if (!argc || *argc <= 0 || !argv) {
    return regions;
  }

  int new_argc = 1;

  // SAFETY: `argv` receives `*argc + 1` elements from the OS, where the final
  // element is contractually guaranteed to be a `nullptr`.
  auto argv_span =
      UNSAFE_BUFFERS(base::span<char*>(argv, static_cast<size_t>(*argc) + 1u));

  for (size_t i = 1; i < static_cast<size_t>(*argc); ++i) {
    std::string_view arg(argv_span[i]);
    if (base::StartsWith(arg, kUserStreamRegionSwitch)) {
      arg.remove_prefix(kUserStreamRegionSwitch.length());
      if (auto region = DeserializeSharedMemoryUserStreamArgument(arg)) {
        regions.push_back(std::move(*region));
      }
    } else {
      argv_span[new_argc++] = argv_span[i];
    }
  }

  argv_span[new_argc] = nullptr;
  *argc = new_argc;

  return regions;
}

}  // namespace crash_reporter::internal
