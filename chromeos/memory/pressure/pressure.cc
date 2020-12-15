// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/pressure.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace chromeos {
namespace memory {
namespace pressure {

namespace {

const base::Feature kCrOSLowMemoryNotificationPSI{
    "CrOSLowMemoryNotificationPSI", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kCrOSLowMemoryPSIThreshold{
    &kCrOSLowMemoryNotificationPSI, "CrOSLowMemoryPSIThreshold", 20};

// The reserved file cache.
constexpr char kMinFilelist[] = "/proc/sys/vm/min_filelist_kbytes";

// The estimation of how well zram based swap is compressed.
constexpr char kRamVsSwapWeight[] =
    "/sys/kernel/mm/chromeos-low_mem/ram_vs_swap_weight";

// The default value if the ram_vs_swap_weight file is not available.
constexpr uint64_t kRamVsSwapWeightDefault = 4;

// The extra free to trigger kernel memory reclaim earlier.
constexpr char kExtraFree[] = "/proc/sys/vm/extra_free_kbytes";

// The margin mem file contains the two memory levels, the first is the
// critical level and the second is the moderate level. Note, this
// file may contain more values but only the first two are used for
// memory pressure notifications in chromeos.
constexpr char kMarginMemFile[] = "/sys/kernel/mm/chromeos-low_mem/margin";

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

uint64_t GetReservedMemoryKB() {
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

bool SupportsPSI() {
  static bool supports_psi =
      base::PathExists(base::FilePath("/proc/pressure/"));
  return supports_psi;
}

// Returns the percentage of the recent 10 seconds that some process is blocked
// by memory.
double GetPSIMemoryPressure10Seconds() {
  base::FilePath psi_memory("/proc/pressure/memory");
  std::string contents;
  if (!base::ReadFileToStringNonBlocking(base::FilePath(psi_memory),
                                         &contents)) {
    PLOG(ERROR) << "Unable to read file: " << psi_memory;
    return 0;
  }
  return ParsePSIMemory(contents);
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

// Returns the percentage of the recent 10 seconds that some process is blocked
// by memory.
// Example input:
//   some avg10=0.00 avg60=0.00 avg300=0.00 total=0
//   full avg10=0.00 avg60=0.00 avg300=0.00 total=0
double ParsePSIMemory(const std::string& contents) {
  for (const base::StringPiece& line : base::SplitStringPiece(
           contents, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        line, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    if (tokens[0] == "some") {
      base::StringPairs kv_pairs;
      if (base::SplitStringIntoKeyValuePairs(line.substr(5), '=', ' ',
                                             &kv_pairs)) {
        double some_10seconds;
        if (base::StringToDouble(kv_pairs[0].second, &some_10seconds)) {
          return some_10seconds;
        } else {
          LOG(ERROR) << "Couldn't parse the value of the first pair";
        }
      } else {
        LOG(ERROR)
            << "Couldn't split the key-value pairs in /proc/pressure/memory";
      }
    }
  }
  LOG(ERROR) << "Couldn't parse /proc/pressure/memory: " << contents;
  DCHECK(false);
  return 0;
}

// CalculateAvailableMemoryUserSpaceKB implements similar available memory
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
  const uint64_t free_component =
      (free > reserved_free) ? free - reserved_free : 0;
  const uint64_t cache_component =
      (file > dirty + min_filelist) ? file - dirty - min_filelist : 0;
  const uint64_t swappable = std::min<uint64_t>(anon, info.swap_free);
  const uint64_t swap_component = swappable / ram_swap_weight;
  return free_component + cache_component + swap_component;
}

uint64_t GetAvailableMemoryKB() {
  base::SystemMemoryInfoKB info;
  uint64_t available_kb;
  if (base::GetSystemMemoryInfo(&info)) {
    available_kb = CalculateAvailableMemoryUserSpaceKB(
        info, reserved_free, min_filelist, ram_swap_weight);
  } else {
    PLOG(ERROR)
        << "Assume low memory pressure if opening/parsing meminfo failed";
    LOG_IF(FATAL, base::SysInfo::IsRunningOnChromeOS())
        << "procfs isn't mounted or unable to open /proc/meminfo";
    available_kb = 4 * 1024;
  }

  static bool using_psi = SupportsPSI() && base::FeatureList::IsEnabled(
                                               kCrOSLowMemoryNotificationPSI);
  if (using_psi) {
    auto margins = GetMemoryMarginsKB();
    const uint64_t critical_margin = margins.first;
    const uint64_t moderate_margin = margins.second;
    static double psi_threshold = kCrOSLowMemoryPSIThreshold.Get();
    // When PSI memory pressure is high, trigger moderate memory pressure.
    if (GetPSIMemoryPressure10Seconds() > psi_threshold) {
      available_kb =
          std::min(available_kb, (moderate_margin + critical_margin) / 2);
    }
  }
  return available_kb;
}

std::vector<uint64_t> GetMarginFileParts(const std::string& file) {
  std::vector<uint64_t> margin_values;
  std::string margin_contents;
  if (base::ReadFileToStringNonBlocking(base::FilePath(file),
                                        &margin_contents)) {
    std::vector<std::string> margins =
        base::SplitString(margin_contents, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& v : margins) {
      uint64_t value = 0;
      if (!base::StringToUint64(v, &value)) {
        // If any of the values weren't parseable as an uint64_T we return
        // nothing as the file format is unexpected.
        LOG(ERROR) << "Unable to parse margin file contents as integer: " << v;
        return std::vector<uint64_t>();
      }
      margin_values.push_back(value);
    }
  } else {
    PLOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Unable to read margin file";
  }
  return margin_values;
}

namespace {

// This function would return valid margins even when there are less than 2
// margin values in the kernel margin file.
std::pair<uint64_t, uint64_t> GetMemoryMarginsKBImpl() {
  const std::vector<uint64_t> margin_file_parts(
      GetMarginFileParts(kMarginMemFile));
  if (margin_file_parts.size() >= 2) {
    return {margin_file_parts[0] * 1024, margin_file_parts[1] * 1024};
  }

  // Critical margin is 5.2% of total memory, moderate margin is 40% of total
  // memory. See also /usr/share/cros/init/swap.sh on DUT.
  base::SystemMemoryInfoKB info;
  int total_memory_kb = 2 * 1024;
  if (base::GetSystemMemoryInfo(&info)) {
    total_memory_kb = info.total;
  } else {
    PLOG(ERROR)
        << "Assume 2 GiB total memory if opening/parsing meminfo failed";
    LOG_IF(FATAL, base::SysInfo::IsRunningOnChromeOS())
        << "procfs isn't mounted or unable to open /proc/meminfo";
  }
  return {total_memory_kb * 13 / 250, total_memory_kb * 2 / 5};
}

}  // namespace

std::pair<uint64_t, uint64_t> GetMemoryMarginsKB() {
  static std::pair<uint64_t, uint64_t> result(GetMemoryMarginsKBImpl());
  return result;
}

void UpdateMemoryParameters() {
  reserved_free = GetReservedMemoryKB();
  min_filelist = ReadFileToUint64(base::FilePath(kMinFilelist));
  ram_swap_weight = ReadFileToUint64(base::FilePath(kRamVsSwapWeight));
  if (ram_swap_weight == 0)
    ram_swap_weight = kRamVsSwapWeightDefault;
}

PressureChecker::PressureChecker() : weak_ptr_factory_(this) {}

PressureChecker::~PressureChecker() = default;

PressureChecker* PressureChecker::GetInstance() {
  return base::Singleton<PressureChecker>::get();
}

void PressureChecker::SetCheckingDelay(base::TimeDelta delay) {
  if (checking_timer_.GetCurrentDelay() == delay)
    return;

  if (delay.is_zero()) {
    checking_timer_.Stop();
  } else {
    checking_timer_.Start(FROM_HERE, delay,
                          base::BindRepeating(&PressureChecker::CheckPressure,
                                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void PressureChecker::AddObserver(PressureObserver* observer) {
  pressure_observers_.AddObserver(observer);
}

void PressureChecker::RemoveObserver(PressureObserver* observer) {
  pressure_observers_.RemoveObserver(observer);
}

void PressureChecker::CheckPressure() {
  std::pair<uint64_t, uint64_t> margins_kb =
      chromeos::memory::pressure::GetMemoryMarginsKB();
  uint64_t critical_margin_kb = margins_kb.first;
  uint64_t moderate_margin_kb = margins_kb.second;
  uint64_t available_kb = GetAvailableMemoryKB();

  if (available_kb < critical_margin_kb) {
    for (auto& observer : pressure_observers_) {
      observer.OnCriticalPressure();
    }
  } else if (available_kb < moderate_margin_kb) {
    for (auto& observer : pressure_observers_) {
      observer.OnModeratePressure();
    }
  }
}

}  // namespace pressure
}  // namespace memory
}  // namespace chromeos
