// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/pressure.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace chromeos {
namespace memory {
namespace pressure {

namespace {

// The reserved file cache.
constexpr char kMinFilelist[] = "/proc/sys/vm/min_filelist_kbytes";

// The estimation of how well zram based swap is compressed.
constexpr char kRamVsSwapWeight[] =
    "/sys/kernel/mm/chromeos-low_mem/ram_vs_swap_weight";

// The extra free to trigger kernel memory reclaim earlier.
constexpr char kExtraFree[] = "/proc/sys/vm/extra_free_kbytes";

// Values saved for user space available memory calculation.  The value of
// |reserved_free| should not change unless min_free_kbytes or
// lowmem_reserve_ratio change.  The value of |min_filelist| and
// |ram_swap_weight| should not change unless the user sets them manually.
uint64_t reserved_free = 0;
uint64_t min_filelist = 0;
uint64_t ram_swap_weight = 0;

uint64_t ReadFileToUint64(const base::FilePath& file) {
  std::string file_contents;
  if (!base::ReadFileToStringNonBlocking(file, &file_contents)) {
    PLOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Unable to read uint64 from: " << file;
    return 0;
  }
  TrimWhitespaceASCII(file_contents, base::TRIM_ALL, &file_contents);
  uint64_t file_contents_uint64 = 0;
  if (!base::StringToUint64(file_contents, &file_contents_uint64))
    return 0;
  return file_contents_uint64;
}

}  // namespace

// CalculateReservedFreeKB() calculates the reserved free memory in KiB from
// /proc/zoneinfo.  Reserved pages are free pages reserved for emergent kernel
// allocation and are not available to the user space.  It's the sum of high
// watermarks and max protection pages of memory zones.  It implements the same
// reserved pages calculation in linux kernel calculate_totalreserve_pages().
//
// /proc/zoneinfo example:
// ...
// Node 0, zone    DMA32
//   pages free     422432
//         min      16270
//         low      20337
//         high     24404
//         ...
//         protection: (0, 0, 1953, 1953)
//
// The high field is the high watermark for this zone.  The protection field is
// the protected pages for lower zones.  See the lowmem_reserve_ratio section in
// https://www.kernel.org/doc/Documentation/sysctl/vm.txt.
uint64_t CalculateReservedFreeKB(const std::string& zoneinfo) {
  constexpr uint64_t kPageSizeKB = 4;

  uint64_t num_reserved_pages = 0;
  for (const base::StringPiece& line : base::SplitStringPiece(
           zoneinfo, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        line, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);

    // Skip the line if there are not enough tokens.
    if (tokens.size() < 2) {
      continue;
    }

    if (tokens[0] == "high") {
      // Parse the high watermark.
      uint64_t high = 0;
      if (base::StringToUint64(tokens[1], &high)) {
        num_reserved_pages += high;
      } else {
        LOG(ERROR) << "Couldn't parse the high field in /proc/zoneinfo: "
                   << tokens[1];
      }
    } else if (tokens[0] == "protection:") {
      // Parse the protection pages.
      uint64_t max = 0;
      for (size_t i = 1; i < tokens.size(); ++i) {
        uint64_t num = 0;
        base::StringPiece entry;
        if (i == 1) {
          // Exclude the leading '(' and the trailing ','.
          entry = tokens[i].substr(1, tokens[i].size() - 2);
        } else {
          // Exclude the trailing ',' or ')'.
          entry = tokens[i].substr(0, tokens[i].size() - 1);
        }
        if (base::StringToUint64(entry, &num)) {
          max = std::max(max, num);
        } else {
          LOG(ERROR)
              << "Couldn't parse the protection field in /proc/zoneinfo: "
              << entry;
        }
      }
      num_reserved_pages += max;
    }
  }

  return num_reserved_pages * kPageSizeKB;
}

static uint64_t GetReservedMemoryKB() {
  std::string file_contents;
  if (!base::ReadFileToStringNonBlocking(base::FilePath("/proc/zoneinfo"),
                                         &file_contents)) {
    PLOG(ERROR) << "Couldn't get /proc/zoneinfo";
    return 0;
  }

  // Reserve free pages is high watermark + lowmem_reserve and extra_free_kbytes
  // raises the high watermark.  Nullify the effect of extra_free_kbytes by
  // excluding it from the reserved pages.  The default extra_free_kbytes value
  // is 0 if the file couldn't be accessed.
  return CalculateReservedFreeKB(file_contents) -
         ReadFileToUint64(base::FilePath(kExtraFree));
}

// CalculateAvailableMemoryUserSpaceKB implements the same available memory
// calculation as kernel function get_available_mem_adj().  The available memory
// consists of 3 parts: the free memory, the file cache, and the swappable
// memory.  The available free memory is free memory minus reserved free memory.
// The available file cache is the total file cache minus reserved file cache
// (min_filelist).  Because swapping is prohibited if there is no anonymous
// memory or no swap free, the swappable memory is the minimal of anonymous
// memory and swap free.  As swapping memory is more costly than dropping file
// cache, only a fraction (1 / ram_swap_weight) of the swappable memory
// contributes to the available memory.
uint64_t CalculateAvailableMemoryUserSpaceKB(
    const base::SystemMemoryInfoKB& info,
    uint64_t reserved_free,
    uint64_t min_filelist,
    uint64_t ram_swap_weight) {
  const uint64_t free = info.free;
  const uint64_t anon = info.active_anon + info.inactive_anon;
  const uint64_t file = info.active_file + info.inactive_file;
  const uint64_t dirty = info.dirty;
  const uint64_t swap_free = info.swap_free;

  uint64_t available = (free > reserved_free) ? free - reserved_free : 0;
  available += (file > dirty + min_filelist) ? file - dirty - min_filelist : 0;
  available += std::min<uint64_t>(anon, swap_free) / ram_swap_weight;

  return available;
}

uint64_t GetAvailableMemoryKB() {
  base::SystemMemoryInfoKB info;
  CHECK(base::GetSystemMemoryInfo(&info));
  return CalculateAvailableMemoryUserSpaceKB(info, reserved_free, min_filelist,
                                             ram_swap_weight);
}

std::pair<uint64_t, uint64_t> GetMemoryMarginsKB() {
  // TODO(b/149833548): Implement this function.
  return {0, 0};
}

void UpdateMemoryParameters() {
  reserved_free = GetReservedMemoryKB();
  min_filelist = ReadFileToUint64(base::FilePath(kMinFilelist));
  ram_swap_weight = ReadFileToUint64(base::FilePath(kRamVsSwapWeight));
}

}  // namespace pressure
}  // namespace memory
}  // namespace chromeos
